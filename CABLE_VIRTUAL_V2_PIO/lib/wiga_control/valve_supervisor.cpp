// Cubre: FR-001, FR-003, FR-005, FR-009, FR-010, FR-011, FR-016..FR-021
#include "valve_supervisor.h"

namespace wiga {
namespace {

RetryParams makeRetryParams() {
    RetryParams p;
    p.retryIntervalMs   = config::kRetryIntervalMs;
    p.backoffIntervalMs = config::kBackoffIntervalMs;
    p.maxAttempts       = config::kMaxAttempts;
    return p;
}

}  // namespace

ValveSupervisor::ValveSupervisor(IClock& clock,
                                 IDigitalInputPort& inputs,
                                 IRadioPort& radio,
                                 IDisplayPort& display,
                                 ILogPort& log)
    : clock_(clock),
      inputs_(inputs),
      radio_(radio),
      display_(display),
      log_(log),
      retry_(makeRetryParams()),
      scheduler_(config::kLatchGuardMs) {
    for (uint8_t i = 0; i < config::kValveCount; ++i) {
        shownSymbol_[i] = 0;
    }
}

// --- T1: arranque sin rafaga (FR-010) ---------------------------------------
void ValveSupervisor::begin() {
    const uint32_t now = clock_.nowMs();
    inputs_.configure();

    for (uint8_t i = 0; i < config::kValveCount; ++i) {
        const SignalLevel level = inputs_.read(i);
        const ValveState  state = toValveState(level);

        debouncers_[i].seed(level, now);
        records_[i].requested       = state;
        records_[i].confirmed       = state;   // se asume coherente al arrancar
        records_[i].phase           = ValvePhase::Verified;
        records_[i].attempts        = 0;
        records_[i].nextAttemptAtMs = now;
        records_[i].radioFailures   = 0;
    }
    refreshDisplay();
}

void ValveSupervisor::tick() {
    const uint32_t now = clock_.nowMs();

    pollInputs(now);
    pollRadio(now);       // antes de decidir reintentos: FR-018, R-003
    serviceTimers(now);
    serviceScheduler(now);
    refreshDisplay();
}

// --- FR-001, FR-002, FR-003: entradas del tablero ---------------------------
void ValveSupervisor::pollInputs(uint32_t nowMs) {
    for (uint8_t i = 0; i < config::kValveCount; ++i) {
        const SignalLevel raw = inputs_.read(i);
        if (!debouncers_[i].update(raw, nowMs, config::kDebounceMs)) {
            continue;  // sin cambio estable
        }
        const ValveState requested = toValveState(debouncers_[i].stable());
        if (requested == records_[i].requested) {
            continue;
        }
        // T2 / T9 / T11
        retry_.onRequestChanged(records_[i], requested);
    }
}

// --- FR-005, FR-016, FR-017, FR-018: confirmaciones de los nodos ------------
void ValveSupervisor::pollRadio(uint32_t nowMs) {
    (void)nowMs;
    proto::ValveReport reports[config::kMaxFramesPerPoll];
    const uint8_t received = radio_.poll(reports, config::kMaxFramesPerPoll);

    for (uint8_t r = 0; r < received; ++r) {
        // RX-05: la valvula debe estar configurada.
        const uint8_t index =
            config::findValveIndex(reports[r].nodeAddressLsb, reports[r].valveId);
        if (index == kNoValve) {
            log_.event(LogLevel::Warn, logtag::kDrop, kNoValve, reports[r].valveId);
            continue;
        }

        const ValveState confirmed = toValveState(reports[r].level);

        // RX-06 / T5: una confirmacion que no coincide con lo solicitado NO
        // confirma; la secuencia de reintentos continua.
        if (confirmed != records_[index].requested) {
            records_[index].confirmed = confirmed;
            log_.event(LogLevel::Warn, logtag::kDrop, index,
                       static_cast<int32_t>(rawOf(reports[r].level)));
            continue;
        }

        // T4: unica ruta que da una valvula por confirmada (invariante R-01).
        retry_.onConfirmed(records_[index], confirmed);
        log_.event(LogLevel::Info, logtag::kAck, index,
                   static_cast<int32_t>(rawOf(reports[r].level)));
    }
}

// --- FR-006, FR-007: expiraciones y reposo prolongado -----------------------
void ValveSupervisor::serviceTimers(uint32_t nowMs) {
    for (uint8_t i = 0; i < config::kValveCount; ++i) {
        ValveRecord& record = records_[i];

        if (record.phase == ValvePhase::Awaiting && retry_.waitExpired(record, nowMs)) {
            // T6 / T7
            const bool enteredBackoff = retry_.onTimeout(record, nowMs);
            if (enteredBackoff) {
                log_.event(LogLevel::Error, logtag::kBackoff, i,
                           static_cast<int32_t>(record.radioFailures));
            } else {
                log_.event(LogLevel::Warn, logtag::kTimeout, i,
                           static_cast<int32_t>(record.attempts));
            }
            continue;
        }

        if (record.phase == ValvePhase::Backoff && retry_.waitExpired(record, nowMs)) {
            // T8: vence el reposo. attempts se mantiene en el limite, de modo
            // que el siguiente fallo vuelve a costar una hora (SC-005).
            record.phase = ValvePhase::Pending;
        }
    }
}

// --- FR-004: una orden a la vez ---------------------------------------------
void ValveSupervisor::serviceScheduler(uint32_t nowMs) {
    const ValveIndex index = scheduler_.grant(records_, config::kValveCount, nowMs);
    if (index == kNoValve) {
        return;
    }

    const config::ValveMapping& mapping = config::valveMapping(index);
    proto::ValveCommand command;
    command.nodeAddressLsb = mapping.nodeAddressLsb;
    command.valveId        = mapping.valveId;
    command.level          = toSignalLevel(records_[index].requested);

    if (!radio_.send(command)) {
        // El radio no acepto la trama. Se reintentara en la proxima vuelta sin
        // consumir intento ni abrir la ventana de exclusion.
        log_.event(LogLevel::Error, logtag::kDrop, index, -1);
        return;
    }

    // T3
    retry_.onTransmitted(records_[index], nowMs);
    scheduler_.markServed(index, nowMs);
    log_.event(LogLevel::Info, logtag::kTx, index,
               static_cast<int32_t>(records_[index].attempts));
}

// --- FR-019, FR-020, FR-021: panel ------------------------------------------
char ValveSupervisor::symbolFor(const ValveRecord& record) {
    switch (record.phase) {
        case ValvePhase::Verified:
            // El simbolo refleja el ESTADO FISICO, no el nivel electrico.
            return (record.confirmed == ValveState::Open) ? symbol::kOpen
                                                          : symbol::kClosed;
        case ValvePhase::Pending:
        case ValvePhase::Awaiting:
            return symbol::kPending;
        case ValvePhase::Backoff:
        default:
            return symbol::kFailed;
    }
}

void ValveSupervisor::refreshDisplay() {
    for (uint8_t i = 0; i < config::kValveCount; ++i) {
        const char s = symbolFor(records_[i]);
        if (s != shownSymbol_[i]) {
            shownSymbol_[i] = s;
            display_.setValveSymbol(i, s);
        }
    }
}

}  // namespace wiga
