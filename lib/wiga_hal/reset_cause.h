// -----------------------------------------------------------------------------
//  Captura de la causa del ultimo reinicio.
//  Cubre: FR-026  (research.md R-008)
//
//  Sin esto, un reinicio por watchdog en campo es indistinguible de un corte de
//  alimentacion, y el tecnico no sabe si persigue un problema de firmware o de
//  bateria.
//
//  LIMITACION CONOCIDA: algunos bootloaders (optiboot) limpian MCUSR antes de
//  que corra la seccion .init3. Si el flag de watchdog no aparece en banco,
//  documentarlo en docs/diagnostico.md; el control no depende de este dato.
// -----------------------------------------------------------------------------
#pragma once

#include <stdint.h>

namespace wiga {
namespace hal {

/// Bits de MCUSR capturados antes de que el arranque de Arduino los limpie.
uint8_t lastResetCause();

/// Texto legible de la causa dominante. Cadena en Flash gestionada por quien
/// la imprime; aqui se devuelve un identificador corto y estable.
const char* resetCauseName(uint8_t mcusr);

}  // namespace hal
}  // namespace wiga
