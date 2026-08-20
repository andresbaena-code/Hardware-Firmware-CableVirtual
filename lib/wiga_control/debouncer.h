// -----------------------------------------------------------------------------
//  Filtro de estabilidad de una entrada del tablero.
//  Cubre: FR-002  (data-model.md 5.2, invariantes D-01/D-02, research.md R-006)
//
//  El firmware base no tenia antirebote: cualquier microcorte del contacto
//  disparaba una orden latch con su pico de corriente.
// -----------------------------------------------------------------------------
#pragma once

#include <stdint.h>

#include "valve_types.h"

namespace wiga {

class Debouncer {
public:
    /// Fija el nivel estable inicial sin generar transicion (arranque, FR-010).
    void seed(SignalLevel level, uint32_t nowMs);

    /// Procesa una lectura cruda. Devuelve true SOLO si el nivel estable cambio,
    /// es decir, si el candidato se mantuvo durante toda la ventana (D-01).
    bool update(SignalLevel raw, uint32_t nowMs, uint16_t windowMs);

    SignalLevel stable() const { return stable_; }

private:
    SignalLevel stable_    = SignalLevel::High;
    SignalLevel candidate_ = SignalLevel::High;
    uint32_t    sinceMs_   = 0;
};

}  // namespace wiga
