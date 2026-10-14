// -----------------------------------------------------------------------------
//  Parametros temporales del sistema. Punto unico de configuracion.
//  Cubre: FR-023  (data-model.md 2.2, research.md R-005/R-006/R-007)
//
//  Los valores heredados provienen de CABLE_VIRTUAL___LATCH_V1.ino, donde se
//  expresaban en vueltas de lazo de 15 ms. Aqui se expresan en milisegundos
//  reales, que es lo que el sistema mide de verdad.
// -----------------------------------------------------------------------------
#pragma once

#include <stdint.h>

#include "installation_map.h"

namespace wiga {
namespace config {

// --- Bus Modbus RTU: el controlador es ESCLAVO -------------------------------
//  Cubre: FR-102, FR-105, FR-114, FR-117  (contracts/register-map.md)
//
//  El puente UC100 es el maestro y el unico que habla primero. El identificador
//  no lo impone el puente: es un campo por canal que se configura en el UC100
//  para que coincida con el del esclavo.

constexpr uint8_t  kModbusSlaveId = 1;

/// Plazo tras el cual una trama A MEDIAS se considera perdida y se descarta.
///
/// NO es el t3.5 del estandar, y el motivo importa:
///
/// El t3.5 a 9600 baudios son 4,01 ms, pero medirlo exige la marca de tiempo de
/// CADA byte, y esa la tendria que dar la interrupcion del UART. HardwareSerial
/// de Arduino no la expone: lo unico disponible es el reloj del LAZO, que
/// captura millis() una vez por vuelta.
///
/// Con un umbral de 4 ms eso era un defecto real, verificado en banco: cuando
/// el lazo tardaba mas de 4 ms -algo que ocurre en cuanto el panel LCD escribe
/// por I2C-, dos bytes que habian llegado SEGUIDOS por el cable parecian
/// separados por mas del silencio, y la trama se descartaba a medias. El esclavo
/// recibia las peticiones y las tiraba, con el sintoma de no responder nunca.
///
/// El delimitador primario es la LONGITUD ESPERADA -las cuatro funciones
/// soportadas usan peticiones de 8 bytes fijos-, asi que este plazo solo debe
/// atrapar una trama parcial que ya no va a completarse. Un valor generoso es
/// mas correcto que uno ajustado: 200 ms son mas de veinte veces lo que tarda
/// una trama completa en el cable (8,3 ms), de modo que ninguna llegada legitima
/// lo alcanza, y una trama truncada por ruido se descarta de sobra antes del
/// siguiente sondeo del maestro.
constexpr uint16_t kModbusSilenceMs = 200;

/// Tiempo que tarda la trama mas larga soportada en el cable, para documentar
/// el margen del plazo de arriba: 8 bytes * 10 bits / 9600 baudios.
constexpr uint16_t kModbusFrameTimeMs = 9;
static_assert(kModbusSilenceMs > kModbusFrameTimeMs * 10,
              "C-12: el plazo de trama perdida debe superar con holgura lo que "
              "tarda una trama completa, o un lazo lento descartara tramas buenas");

/// Sin peticiones durante este plazo, el maestro se considera ausente: UC100
/// apagado o cable RS485 cortado. Es el fallo mas probable en campo y el unico
/// que el controlador puede detectar por si solo (FR-114).
constexpr uint32_t kBusSilentThresholdMs = 30000UL;

// --- Mapa de registros publicado ---------------------------------------------
//  Un registro por valvula: las reglas IF-THEN del UC100 evaluan el VALOR de un
//  canal contra un umbral, de modo que empaquetar varias valvulas en un registro
//  impediria condicionarlas por separado (research.md R-105).

constexpr uint16_t kValveRegisterBase = 0;
constexpr uint16_t kDiagRegisterBase  = kValveRegisterBase + kValveCount;  // 24
constexpr uint16_t kDiagRegisterCount = 8;

/// Total DERIVADO, nunca escrito dos veces (Principio V).
constexpr uint16_t kRegisterCount = kValveCount + kDiagRegisterCount;

/// Desplazamientos dentro del bloque de diagnostico.
namespace diag {
constexpr uint16_t kFirmwareVersion = kDiagRegisterBase + 0;
constexpr uint16_t kSecondsSincePoll = kDiagRegisterBase + 1;
constexpr uint16_t kCrcErrors        = kDiagRegisterBase + 2;
constexpr uint16_t kFormatErrors     = kDiagRegisterBase + 3;
constexpr uint16_t kValidRequests    = kDiagRegisterBase + 4;
constexpr uint16_t kResetCause       = kDiagRegisterBase + 5;
constexpr uint16_t kValvesIrrigating = kDiagRegisterBase + 6;
constexpr uint16_t kReserved         = kDiagRegisterBase + 7;
}  // namespace diag

// --- Invariantes del bus -----------------------------------------------------
static_assert(kModbusSlaveId >= 1 && kModbusSlaveId <= 247,
              "C-08: el identificador de esclavo debe estar en 1..247");
static_assert(kModbusSilenceMs > 0,
              "C-09: el silencio entre tramas debe ser mayor que cero");
static_assert(kDiagRegisterBase == kValveCount,
              "C-10: el bloque de diagnostico arranca justo tras las valvulas");
static_assert(kRegisterCount == kValveCount + kDiagRegisterCount,
              "C-11: el total de registros se deriva, no se escribe a mano");

// --- Cadencia y filtrado -----------------------------------------------------
constexpr uint16_t kLoopPeriodMs = 15;    ///< cadencia nominal del lazo (heredado)
constexpr uint16_t kDebounceMs   = 50;    ///< ventana de estabilidad de entrada (R-006)

// --- Politica de disparo y reintentos ---------------------------------------
constexpr uint16_t kLatchGuardMs      = 1500;       ///< exclusion entre disparos (heredado)
constexpr uint32_t kRetryIntervalMs   = 30000UL;    ///< espera de reintento rapido (heredado)
constexpr uint32_t kBackoffIntervalMs = 3600000UL;  ///< reposo prolongado, 60 min (heredado)
constexpr uint8_t  kMaxAttempts       = 5;          ///< intentos rapidos (heredado)

// --- Separacion de comandos por UC511 ---------------------------------------
//  Cubre: FR-214  (specs/003-retardo-apertura-cancelable, research.md R-308)
//
//  Un UC511 acciona dos valvulas latch con un pulso de 12 V / 50 ms, y pierde
//  sin aviso el segundo de dos comandos que le lleguen con menos de 5 s entre
//  ellos (medido en el piloto; lo mas probable es la recarga del condensador
//  del pulso). A equipos distintos no les pasa.

/// Separacion minima que tolera un UC511 entre dos comandos. Dato de campo,
/// no del fabricante: si una medicion nueva lo cambia, se cambia aqui.
constexpr uint32_t kActuatorMinGapMs = 5000UL;

/// Separacion que el controlador impone entre dos cambios publicados del mismo
/// UC511. Los 3 s sobre el minimo absorben la latencia del sondeo del UC100
/// (~1 s medida): dos cambios publicados a 8 s pueden llegar al campo algo mas
/// juntos de lo que salieron.
constexpr uint32_t kCommandSpacingMs = 8000UL;

static_assert(kCommandSpacingMs >= kActuatorMinGapMs,
              "TC-01: la separacion entre comandos no puede ser menor que la "
              "que tolera el UC511, o el segundo comando se pierde en campo");

// --- Presupuesto por vuelta de lazo -----------------------------------------
constexpr uint8_t  kMaxFramesPerPoll    = 4;  ///< tramas drenadas por vuelta (R-002)
constexpr uint8_t  kDisplayCellsPerTick = 1;  ///< celdas de LCD escritas por vuelta (R-007)
constexpr uint32_t kMaxPollDurationMs   = 10; ///< limite del drenaje de radio por vuelta

// --- Panel LCD ---------------------------------------------------------------
constexpr uint8_t kLcdI2cAddress = 0x27;
constexpr uint8_t kLcdColumns    = 20;
constexpr uint8_t kLcdRows       = 4;
constexpr uint8_t kValvesPerRow  = 8;
constexpr uint8_t kLcdLabelWidth = 4;   ///< ancho de la etiqueta "E1: "
constexpr uint8_t kLcdMaxConsecutiveFailures = 10;  ///< umbral para marcar el panel caido

/// Cadencia de reintento cuando el panel se dio por caido. Se mide en TIEMPO
/// REAL, no en vueltas de lazo: display.tick() se llama en cada vuelta del lazo
/// y no una vez cada kLoopPeriodMs, de modo que contar vueltas daba una cadencia
/// quince veces mas rapida de la documentada.
constexpr uint32_t kLcdRecoveryIntervalMs = 1000UL;

/// Cadencia del sondeo de presencia del panel. Un LCD desconectado en caliente
/// responde NACK, no expiracion, asi que hay que preguntarle activamente.
constexpr uint32_t kLcdProbeIntervalMs = 1000UL;

// --- Puertos serie -----------------------------------------------------------
constexpr uint32_t kRadioBaudRate   = 9600;
constexpr uint32_t kConsoleBaudRate = 9600;
/// Maximo que admite el expansor I2C del panel.
///
/// El modulo LCD2004A de esta instalacion lleva un PCF8574T, que es de MODO
/// ESTANDAR: 100 kHz. No confundir con el PCA8574, que si admite 400 kHz.
constexpr uint32_t kLcdExpanderMaxHz = 100000UL;

/// Velocidad del bus I2C del panel.
///
/// 100 kHz, el maximo del PCF8574T. Antes iba a 200 kHz y funcionaba, pero por
/// tolerancia del chip y no por diseño: en un tablero de finca, con cables
/// largos al panel y contactores conmutando cerca, ese es justo el margen que se
/// pierde primero.
///
/// Bajarlo hace que cada celda del panel tarde el doble, y hasta hace poco eso
/// habria roto la recepcion Modbus: con el umbral de trama incompleta en 4 ms,
/// un lazo mas lento descartaba tramas buenas. Al pasar ese umbral a 200 ms el
/// bus quedo inmune a la duracion del lazo, y solo entonces se pudo ir a la
/// velocidad correcta del expansor sin efectos secundarios.
constexpr uint32_t kI2cClockHz = 100000UL;

static_assert(kI2cClockHz <= kLcdExpanderMaxHz,
              "C-13: la velocidad del I2C supera lo que admite el expansor del "
              "panel (PCF8574T: 100 kHz)");

// --- Invariantes de configuracion -------------------------------------------
static_assert(kValvesPerRow > 0,
              "C-07: kValvesPerRow debe ser mayor que cero (evita division por cero)");
static_assert(kDebounceMs >= kLoopPeriodMs,
              "C-05: la ventana antirebote debe cubrir al menos una vuelta de lazo");
static_assert(kRetryIntervalMs < kBackoffIntervalMs,
              "C-06: el reposo prolongado debe superar al reintento rapido");
static_assert(kMaxAttempts > 0,
              "debe permitirse al menos un intento");
static_assert(kValvesPerRow * (kLcdRows - 1) >= 24,
              "el panel debe poder mostrar las 24 valvulas en las 3 primeras filas");
static_assert(kLcdLabelWidth + (kValvesPerRow * 2) <= kLcdColumns,
              "las etiquetas y los simbolos no caben en el ancho del panel");

}  // namespace config
}  // namespace wiga
