// Cubre: FR-026  (research.md R-008)
#include "reset_cause.h"

#include <avr/io.h>
#include <avr/wdt.h>

namespace {

/// Copia de MCUSR. Vive en .noinit para sobrevivir a la inicializacion de datos.
uint8_t g_mcusrMirror __attribute__((section(".noinit")));

}  // namespace

extern "C" {

/// Se ejecuta antes de main(), antes de que el arranque de Arduino limpie MCUSR.
void wiga_capture_reset_cause(void)
    __attribute__((naked, used, section(".init3")));

void wiga_capture_reset_cause(void) {
    g_mcusrMirror = MCUSR;
    MCUSR = 0;
    wdt_disable();
}

}  // extern "C"

namespace wiga {
namespace hal {

uint8_t lastResetCause() { return g_mcusrMirror; }

const char* resetCauseName(uint8_t mcusr) {
    // Orden de prioridad: el watchdog es el que importa para el diagnostico.
    if (mcusr & (1 << WDRF)) return "WATCHDOG";
    if (mcusr & (1 << BORF)) return "BROWNOUT";
    if (mcusr & (1 << EXTRF)) return "EXTERNO";
    if (mcusr & (1 << PORF)) return "ALIMENTACION";
    return "DESCONOCIDA";
}

}  // namespace hal
}  // namespace wiga
