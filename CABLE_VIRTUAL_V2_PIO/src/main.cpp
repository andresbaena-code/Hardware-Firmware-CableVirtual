// -----------------------------------------------------------------------------
//  WIGA Cable Virtual - raiz de composicion.
//  Cubre: FR-011, FR-012, FR-026  (contracts/ports.md, CP-01, CP-02)
//
//  Este archivo NO contiene logica de negocio. Su unica responsabilidad es
//  crear los adaptadores concretos, inyectarlos en el supervisor y ejecutar el
//  lazo. Toda la logica vive en lib/wiga_control y lib/wiga_domain, donde se
//  verifica en el equipo del desarrollador sin hardware.
// -----------------------------------------------------------------------------
#include <Arduino.h>

#include "arduino_clock.h"
#include "arduino_digital_input.h"
#include "avr_watchdog.h"
#include "firmware_version.h"
#include "lcd_display_adapter.h"
#include "reset_cause.h"
#include "serial_log.h"
#include "time_math.h"
#include "timing_config.h"
#include "valve_supervisor.h"
#include "xbee_radio_adapter.h"

namespace {

// Adaptadores concretos. Estaticos: sin asignacion dinamica (Principio de
// plataforma). Unico lugar del proyecto donde aparecen tipos de hardware.
wiga::hal::ArduinoClock       g_clock;
wiga::hal::ArduinoDigitalInput g_inputs;
wiga::hal::XBeeRadioAdapter   g_radio;
wiga::hal::LcdDisplayAdapter  g_display;
wiga::hal::SerialLog          g_log;
wiga::hal::AvrWatchdog        g_watchdog;

wiga::ValveSupervisor g_supervisor(g_clock, g_inputs, g_radio, g_display, g_log);

uint32_t g_lastTickMs = 0;

}  // namespace

void setup() {
    // WD-01: el watchdog se desactiva antes de cualquier inicializacion lenta.
    g_watchdog.disable();

    g_log.begin();
    g_radio.begin();
    g_display.begin();

    // RA-08: en AVR, Serial1.begin() e I2C.init() no retornan error.
    // La salud del LCD se monitorea via healthy() en el lazo.

    // FR-026: version y causa del ultimo reinicio.
    g_log.message(wiga::LogLevel::Info, wiga::hal::kFirmwareName);
    g_log.message(wiga::LogLevel::Info, wiga::hal::kFirmwareVersion);
    g_log.message(wiga::LogLevel::Info, wiga::hal::kBuildDate);

    const uint8_t cause = wiga::hal::lastResetCause();
    g_log.event(wiga::LogLevel::Info, wiga::logtag::kBoot,
                wiga::kNoValve, static_cast<int32_t>(cause));
    g_log.message(wiga::LogLevel::Info, wiga::hal::resetCauseName(cause));

    g_display.setStatusLine("WIGA V2 - LATCH");

    // T1 / FR-010: siembra el estado desde las entradas fisicas sin transmitir.
    g_supervisor.begin();

    // WD-02
    g_watchdog.enable();
    g_lastTickMs = millis();
}

void loop() {
    // WD-03: unico punto de refresco del watchdog en todo el firmware.
    g_watchdog.feed();

    // FR-011: cadencia por reloj, sin delay(). El lazo sigue girando entre
    // vueltas de control, de modo que el panel se refresca y la radio se drena
    // sin esperas activas.
    const uint32_t now = millis();
    if (wiga::elapsed(now, g_lastTickMs, wiga::config::kLoopPeriodMs)) {
        g_lastTickMs = now;
        g_supervisor.tick();
    }

    // DP-02: como maximo una celda del panel por vuelta.
    g_display.tick();
}
