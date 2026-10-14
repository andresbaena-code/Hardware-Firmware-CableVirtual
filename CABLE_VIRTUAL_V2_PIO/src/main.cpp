// -----------------------------------------------------------------------------
//  WIGA Cable Virtual - raiz de composicion.
//  Cubre: FR-101, FR-113, FR-114, FR-116, FR-121  (contracts/modbus-slave.md)
//
//  Este archivo NO contiene logica de negocio. Su unica responsabilidad es
//  crear los adaptadores concretos, inyectarlos y ejecutar el lazo.
//
//  TOPOLOGIA REAL (specs/002-modbus-esclavo-rs485):
//
//    [ este controlador ]      [ UC100 ]           [ UC501 ]
//      ESCLAVO Modbus    <--   MAESTRO Modbus  -->  agente D2D
//      publica el estado       y controlador D2D    acciona la valvula
//                              LoRa directo, SIN gateway
//
//  El controlador NUNCA inicia una transaccion ni emite ordenes de valvula:
//  solo publica en registros lo que el tablero pide, y contesta cuando le
//  preguntan. La confirmacion del campo no existe, porque el protocolo de radio
//  no acusa recibo (constitucion v2.0.0, Principio II).
// -----------------------------------------------------------------------------
#include <Arduino.h>

#include "arduino_clock.h"
#include "arduino_digital_input.h"
#include "avr_watchdog.h"
#include "firmware_version.h"
#include "installation_map.h"
#include "lcd_display_adapter.h"
#include "modbus_slave.h"
#include "register_file.h"
#include "reset_cause.h"
#include "rs485_stream_adapter.h"
#include "serial_log.h"
#include "time_math.h"
#include "timing_config.h"
#include "valve_publisher.h"

namespace {

// Adaptadores concretos. Estaticos: sin asignacion dinamica (Principio de
// plataforma). Unico lugar del proyecto donde aparecen tipos de hardware.
wiga::hal::ArduinoClock        g_clock;
wiga::hal::ArduinoDigitalInput g_inputs;
wiga::hal::LcdDisplayAdapter   g_display;
wiga::hal::SerialLog           g_log;
wiga::hal::AvrWatchdog         g_watchdog;

// El transceptor RS485 cuelga del SEGUNDO puerto serie de esta placa:
// TXD2 = PH1 = pin 16, RXD2 = PH0 = pin 17, DE/RE = PH3 = pin 6.
// La consola de diagnostico sigue en Serial.
wiga::hal::Rs485StreamAdapter g_bus(Serial2,
                                    wiga::config::kRs485DeRePin,
                                    wiga::config::kRadioBaudRate);

// El banco de registros es la unica fuente de verdad del estado pedido.
wiga::modbus::RegisterFile g_registers;

wiga::modbus::ModbusSlave g_slave(g_registers,
                                  wiga::config::kModbusSlaveId,
                                  wiga::config::kModbusSilenceMs);

wiga::ValvePublisher g_publisher(g_clock, g_inputs, g_registers, g_display, g_log);

uint32_t g_lastTickMs  = 0;
uint32_t g_lastDiagMs  = 0;
uint8_t  g_resetCause   = 0;
bool     g_panelHealthy = true;

/// Atiende el bus. No bloquea: consume los bytes disponibles, y si hay una
/// respuesta lista la entrega al hardware sin esperar a que salga del cable.
void serviceBus(uint32_t nowMs) {
    // Avanza una transmision pendiente de vueltas anteriores.
    g_bus.service();

    // Si terminamos de transmitir, se libera la linea (regla DE-02).
    if (!g_bus.transmitting()) {
        g_bus.endTransmission();
    }

    // Drena lo recibido. Acotado por vuelta para no monopolizar el lazo.
    uint8_t budget = 32;
    while (budget-- > 0 && g_bus.available() > 0) {
        g_slave.onByte(g_bus.read(), nowMs);
    }
    g_slave.poll(nowMs);

    // Una respuesta lista se entrega ahora; el cable tarda lo suyo, pero el
    // lazo no la espera.
    if (g_slave.hasResponse() && !g_bus.transmitting()) {
        g_bus.beginTransmission(g_slave.responseLength());
        g_bus.write(g_slave.response(), g_slave.responseLength());
        g_slave.responseSent();
    }
}

/// Refresca el bloque de diagnostico y la senal de bus mudo una vez por segundo.
void serviceDiagnostics(uint32_t nowMs) {
    if (!wiga::elapsed(nowMs, g_lastDiagMs, 1000)) {
        return;
    }
    g_lastDiagMs = nowMs;

    g_publisher.publishDiagnostics(g_slave.secondsSinceRequest(nowMs),
                                   g_slave.stats().crcErrors,
                                   g_slave.stats().formatErrors,
                                   g_slave.stats().validRequests,
                                   g_resetCause);

    // FR-114: el maestro ausente es el fallo mas probable en campo. La decision
    // vive en la capa pura, donde las pruebas la alcanzan.
    g_publisher.setBusHealthy(
        !g_slave.masterSilent(nowMs, wiga::config::kBusSilentThresholdMs));

    // La salud del panel SI se consume ahora. Antes existia healthy(), se
    // calculaba, y no la leia nadie: un comentario de este archivo afirmaba que
    // se monitoreaba en el lazo y no era cierto.
    const bool panelHealthy = g_display.healthy();
    if (panelHealthy != g_panelHealthy) {
        g_panelHealthy = panelHealthy;
        g_log.event(panelHealthy ? wiga::LogLevel::Info : wiga::LogLevel::Error,
                    wiga::logtag::kDrop, wiga::kNoValve, panelHealthy ? 1 : 0);
        g_log.message(panelHealthy ? wiga::LogLevel::Info : wiga::LogLevel::Error,
                      panelHealthy ? "PANEL LCD OK" : "PANEL LCD SIN RESPUESTA");
    }
}

}  // namespace

void setup() {
    // WD-01: el watchdog se desactiva antes de cualquier inicializacion lenta.
    g_watchdog.disable();

    g_log.begin();
    g_bus.begin();

    g_display.begin();

    // FR-026: version y causa del ultimo reinicio.
    g_log.message(wiga::LogLevel::Info, wiga::hal::kFirmwareName);
    g_log.message(wiga::LogLevel::Info, wiga::hal::kFirmwareVersion);
    g_log.message(wiga::LogLevel::Info, wiga::hal::kBuildDate);

    g_resetCause = wiga::hal::lastResetCause();
    g_log.event(wiga::LogLevel::Info, wiga::logtag::kBoot,
                wiga::kNoValve, static_cast<int32_t>(g_resetCause));
    g_log.message(wiga::LogLevel::Info, wiga::hal::resetCauseName(g_resetCause));

    g_registers.set(wiga::config::diag::kFirmwareVersion, 0x0200);

    // FR-116: el banco se siembra desde las entradas fisicas AQUI, antes de que
    // loop() atienda el bus. Si el esclavo contestara antes, devolveria 0 para
    // una valvula que si esta regando y el UC100 la cerraria.
    g_publisher.begin();

    // Flush de bytes residuales del UC100 que llegaron durante el boot.
    // Sin esto, el primer ciclo de serviceBus() parsea un frame truncado
    // y registra un format error innecesario.
    while (g_bus.available() > 0) {
        g_bus.read();
    }

    // WD-02
    g_watchdog.enable();
    g_lastTickMs = millis();
    g_lastDiagMs = g_lastTickMs;
}

void loop() {
    // WD-03: unico punto de refresco del watchdog en todo el firmware.
    g_watchdog.feed();

    const uint32_t now = millis();

    // El bus se atiende en CADA vuelta: el maestro puede sondear cada 10 ms y
    // no hay que hacerle esperar. Ninguna de estas llamadas bloquea.
    serviceBus(now);

    // FR-121: cadencia por reloj, sin delay().
    if (wiga::elapsed(now, g_lastTickMs, wiga::config::kLoopPeriodMs)) {
        g_lastTickMs = now;
        g_publisher.tick();
    }

    serviceDiagnostics(now);

    // DP-02: como maximo una celda del panel por vuelta.
    g_display.tick();
}