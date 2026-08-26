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

namespace wiga {
namespace config {

// --- Direccion de red XBee ---------------------------------------------------
/// 32 bits altos de la direccion de 64 bits. Constante para toda la red (Digi).
constexpr uint32_t kXBeeAddressMsb = 0x0013A200UL;

// --- Cadencia y filtrado -----------------------------------------------------
constexpr uint16_t kLoopPeriodMs = 15;    ///< cadencia nominal del lazo (heredado)
constexpr uint16_t kDebounceMs   = 50;    ///< ventana de estabilidad de entrada (R-006)

// --- Politica de disparo y reintentos ---------------------------------------
constexpr uint16_t kLatchGuardMs      = 1500;       ///< exclusion entre disparos (heredado)
constexpr uint32_t kRetryIntervalMs   = 30000UL;    ///< espera de reintento rapido (heredado)
constexpr uint32_t kBackoffIntervalMs = 3600000UL;  ///< reposo prolongado, 60 min (heredado)
constexpr uint8_t  kMaxAttempts       = 5;          ///< intentos rapidos (heredado)

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

// --- Puertos serie -----------------------------------------------------------
constexpr uint32_t kRadioBaudRate   = 9600;
constexpr uint32_t kConsoleBaudRate = 9600;
constexpr uint32_t kI2cClockHz      = 400000UL;  ///< 400 kHz para no bloquear (R-007)

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
