// -----------------------------------------------------------------------------
//  Politica de reintentos y proteccion de bateria.
//  Cubre: FR-006, FR-007, FR-008, FR-013  (data-model.md seccion 4, T3/T6/T7/T8/T9)
//
//  Principio II (NO NEGOCIABLE): los reintentos son finitos y, agotados, el
//  canal entra en reposo prolongado. Cada disparo latch es un pico de corriente
//  sobre una bateria de 12 V / 9 Ah.
// -----------------------------------------------------------------------------
#pragma once

#include <stdint.h>

#include "valve_record.h"

namespace wiga {

/// Parametros de la politica. Se inyectan para poder probarlos con valores
/// pequenos sin depender de la configuracion de produccion.
struct RetryParams {
    uint32_t retryIntervalMs;
    uint32_t backoffIntervalMs;
    uint8_t  maxAttempts;
};

class RetryPolicy {
public:
    explicit RetryPolicy(const RetryParams& params) : params_(params) {}

    /// T2/T9: la entrada estable cambio. Reinicia el ciclo de intentos y
    /// abandona de inmediato cualquier reposo prolongado (FR-008).
    void onRequestChanged(ValveRecord& record, ValveState requested) const;

    /// T3: se acaba de transmitir una orden. Consume un intento y programa la
    /// espera de confirmacion (FR-006).
    void onTransmitted(ValveRecord& record, uint32_t nowMs) const;

    /// T4: llego una confirmacion valida y coincidente (FR-005).
    void onConfirmed(ValveRecord& record, ValveState confirmed) const;

    /// T6/T7: vencio la espera sin confirmar. Devuelve true si la valvula entro
    /// en reposo prolongado. Contabiliza el fallo de radio (FR-028).
    bool onTimeout(ValveRecord& record, uint32_t nowMs) const;

    /// true si la espera vigente de la valvula ya vencio (T6/T7/T8).
    bool waitExpired(const ValveRecord& record, uint32_t nowMs) const;

private:
    RetryParams params_;
};

}  // namespace wiga
