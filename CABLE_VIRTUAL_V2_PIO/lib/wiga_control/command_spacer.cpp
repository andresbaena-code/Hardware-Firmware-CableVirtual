// Cubre: FR-204, FR-205, FR-206, FR-207, FR-208, FR-209, FR-210, FR-211
#include "command_spacer.h"

#include "time_math.h"

namespace wiga {

CommandSpacer::CommandSpacer() {
    for (uint8_t i = 0; i < config::kValveCount; ++i) {
        published_[i]      = ValveState::Closed;
        pending_[i]        = false;
        pendingSinceMs_[i] = 0;
    }
    for (uint8_t a = 0; a < config::kActuatorCount; ++a) {
        lastChangeMs_[a]  = 0;
        hasLastChange_[a] = false;
    }
}

// --- FR-211: siembra sin esperar ---------------------------------------------
void CommandSpacer::begin(const ValveState* published, uint32_t nowMs) {
    for (uint8_t a = 0; a < config::kActuatorCount; ++a) {
        hasLastChange_[a] = false;
    }
    for (uint8_t i = 0; i < config::kValveCount; ++i) {
        published_[i] = published[i];
        pending_[i]   = false;

        // Si el UC100 arranco a la vez -tablero y puente en la misma
        // alimentacion-, puede emitir esta apertura en su primer sondeo. Se
        // cuenta como cambio del equipo para que otro cambio del mismo UC511
        // no le llegue pegado (research.md R-305). Un equipo con todo cerrado no
        // emite nada al arrancar y queda libre.
        if (published[i] == ValveState::Open) {
            const uint8_t slot   = slotOf(i);
            lastChangeMs_[slot]  = nowMs;
            hasLastChange_[slot] = true;
        }
    }
}

// --- FR-206: un pendiente revertido se descarta ------------------------------
CommandSpacer::Outcome CommandSpacer::update(ValveIndex index, ValveState desired,
                                             uint32_t nowMs) {
    if (index >= config::kValveCount) {
        return Outcome::Unchanged;
    }

    if (desired == published_[index]) {
        if (!pending_[index]) {
            return Outcome::Unchanged;
        }
        // El tablero revirtio el pedido antes de su turno: al campo no le llega
        // nada, y el pendiente no consume turno del equipo.
        pending_[index] = false;
        return Outcome::Cancelled;
    }

    if (pending_[index]) {
        return Outcome::Unchanged;  // ya esperaba; su antiguedad no se toca
    }
    pending_[index]        = true;
    pendingSinceMs_[index] = nowMs;
    return Outcome::Queued;
}

// --- FR-204, FR-205, FR-207, FR-208: turnos por UC511 ------------------------
bool CommandSpacer::next(uint32_t nowMs, ValveIndex& index, ValveState& state) {
    for (uint8_t i = 0; i < config::kValveCount; ++i) {
        if (!pending_[i] || !actuatorFree(slotOf(i), nowMs)) {
            continue;
        }

        // Primer pendiente con equipo libre: se elige el que tiene preferencia
        // DENTRO de su equipo. El orden entre equipos distintos da igual, porque
        // no comparten condensador (FR-208).
        const uint8_t slot = slotOf(i);
        ValveIndex chosen = i;
        for (uint8_t j = static_cast<uint8_t>(i + 1); j < config::kValveCount; ++j) {
            if (pending_[j] && slotOf(j) == slot && takesPrecedence(j, chosen, nowMs)) {
                chosen = j;
            }
        }

        // FR-209: el estado que se publica es siempre el contrario del publicado,
        // que es justo lo que el tablero pide mientras el pendiente exista.
        published_[chosen] = (published_[chosen] == ValveState::Open)
                                 ? ValveState::Closed
                                 : ValveState::Open;
        pending_[chosen]     = false;
        lastChangeMs_[slot]  = nowMs;
        hasLastChange_[slot] = true;

        index = chosen;
        state = published_[chosen];
        return true;
    }
    return false;
}

ValveState CommandSpacer::published(ValveIndex index) const {
    return (index < config::kValveCount) ? published_[index] : ValveState::Closed;
}

bool CommandSpacer::pending(ValveIndex index) const {
    return (index < config::kValveCount) && pending_[index];
}

/// FR-210: wiga::elapsed es correcto a traves del desbordamiento de millis().
bool CommandSpacer::actuatorFree(uint8_t slot, uint32_t nowMs) const {
    return !hasLastChange_[slot] ||
           elapsed(nowMs, lastChangeMs_[slot], config::kCommandSpacingMs);
}

/// FR-207: cierre antes que apertura -el estado seguro es cerrado-, luego el
/// pedido mas antiguo. A igualdad gana `current`, que siempre tiene el indice
/// menor porque el recorrido es ascendente.
bool CommandSpacer::takesPrecedence(ValveIndex candidate, ValveIndex current,
                                    uint32_t nowMs) const {
    const bool candidateCloses = (published_[candidate] == ValveState::Open);
    const bool currentCloses   = (published_[current] == ValveState::Open);
    if (candidateCloses != currentCloses) {
        return candidateCloses;
    }
    // La antiguedad se compara como tiempo transcurrido, no como marca absoluta,
    // para que el desbordamiento no invierta el orden.
    const uint32_t candidateAge = nowMs - pendingSinceMs_[candidate];
    const uint32_t currentAge   = nowMs - pendingSinceMs_[current];
    return candidateAge > currentAge;
}

}  // namespace wiga
