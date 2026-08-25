// -----------------------------------------------------------------------------
//  Exclusion mutua de disparo latch.
//  Cubre: FR-004  (data-model.md 5.1, invariantes S-01/S-02)
//
//  Solo UNA orden puede transmitirse a la vez, con una ventana de exclusion
//  entre disparos. Sin esto, varios solenoides arrancan juntos y el pico de
//  corriente agota la bateria de 12 V.
//
//  El barrido es CIRCULAR desde la ultima valvula atendida (S-02). El firmware
//  base recorria siempre desde 0 y hacia break en la primera candidata: una
//  valvula de indice bajo que fallaba de forma persistente monopolizaba los
//  turnos y las de indice alto nunca se atendian.
// -----------------------------------------------------------------------------
#pragma once

#include <stdint.h>

#include "valve_record.h"

namespace wiga {

class LatchScheduler {
public:
    explicit LatchScheduler(uint16_t guardMs) : guardMs_(guardMs) {}

    /// true si la ventana de exclusion ya vencio y se puede disparar (S-01).
    bool ready(uint32_t nowMs) const;

    /// Elige la proxima valvula a disparar entre las que estan en Pending y con
    /// su espera vencida, con barrido circular. Devuelve kNoValve si no hay
    /// candidata o si la ventana de exclusion sigue vigente.
    ValveIndex grant(const ValveRecord* records, uint8_t count, uint32_t nowMs) const;

    /// Registra que se disparo `index` y abre la ventana de exclusion.
    void markServed(ValveIndex index, uint32_t nowMs);

    ValveIndex lastServed() const { return lastServed_; }

private:
    uint16_t   guardMs_;
    uint32_t   guardStartMs_ = 0;
    bool       active_       = false;
    ValveIndex lastServed_   = 0;
};

}  // namespace wiga
