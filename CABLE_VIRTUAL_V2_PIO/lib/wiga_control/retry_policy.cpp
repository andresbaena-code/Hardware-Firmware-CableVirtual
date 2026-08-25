// Cubre: FR-006, FR-007, FR-008, FR-013
#include "retry_policy.h"

#include "time_math.h"

namespace wiga {

void RetryPolicy::onRequestChanged(ValveRecord& record, ValveState requested) const {
    record.requested = requested;
    if (requested == record.confirmed) {
        // T11: el operador volvio atras antes del disparo. No hay nada que
        // transmitir: se ahorra un ciclo latch completo.
        record.phase = ValvePhase::Verified;
    } else {
        record.phase = ValvePhase::Pending;
    }
    record.attempts        = 0;  // FR-008: reintentos rapidos de nuevo disponibles
    record.nextAttemptAtMs = 0;
}

void RetryPolicy::onTransmitted(ValveRecord& record, uint32_t nowMs) const {
    if (record.attempts < params_.maxAttempts) {
        ++record.attempts;
    }
    record.phase           = ValvePhase::Awaiting;
    record.nextAttemptAtMs = nowMs + params_.retryIntervalMs;
}

void RetryPolicy::onConfirmed(ValveRecord& record, ValveState confirmed) const {
    // Invariante R-01: unico punto donde la fase pasa a Verified.
    record.confirmed       = confirmed;
    record.phase           = ValvePhase::Verified;
    record.attempts        = 0;
    record.nextAttemptAtMs = 0;
}

bool RetryPolicy::onTimeout(ValveRecord& record, uint32_t nowMs) const {
    if (record.radioFailures < 0xFFFF) {
        ++record.radioFailures;  // FR-028
    }
    if (record.attempts >= params_.maxAttempts) {
        // T7: reintentos agotados. Reposo prolongado sin transmitir.
        record.phase           = ValvePhase::Backoff;
        record.nextAttemptAtMs = nowMs + params_.backoffIntervalMs;
        return true;
    }
    // T6: queda margen de reintento rapido.
    record.phase = ValvePhase::Pending;
    return false;
}

bool RetryPolicy::waitExpired(const ValveRecord& record, uint32_t nowMs) const {
    // Invariante R-04: comparacion de plazo en aritmetica modular, correcta al
    // desbordar millis().
    return reached(nowMs, record.nextAttemptAtMs);
}

}  // namespace wiga
