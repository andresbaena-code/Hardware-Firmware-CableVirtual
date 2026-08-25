// -----------------------------------------------------------------------------
//  Estado de una valvula.
//  Cubre: FR-005, FR-009  (data-model.md seccion 3, invariantes R-01..R-04)
//
//  10 bytes por valvula, 240 bytes para 24. Sin asignacion dinamica.
// -----------------------------------------------------------------------------
#pragma once

#include <stdint.h>

#include "valve_types.h"

namespace wiga {

/// Fase de la maquina de estados. Ver data-model.md seccion 4.
enum class ValvePhase : uint8_t {
    Verified,   ///< en regimen: el estado confirmado responde a lo solicitado
    Pending,    ///< hay un cambio que transmitir; espera turno de disparo
    Awaiting,   ///< orden transmitida; espera confirmacion del nodo
    Backoff,    ///< reintentos agotados; reposo prolongado (FR-007)
};

struct ValveRecord {
    ValveState requested       = ValveState::Closed;  ///< lo que pide el tablero
    ValveState confirmed       = ValveState::Closed;  ///< lo que confirmo el nodo
    ValvePhase phase           = ValvePhase::Verified;
    uint8_t    attempts        = 0;   ///< intentos del ciclo actual (<= kMaxAttempts)
    uint32_t   nextAttemptAtMs = 0;   ///< marca del proximo intento permitido
    uint16_t   radioFailures   = 0;   ///< fallos acumulados, para diagnostico (FR-028)

    /// Invariante R-01: solo pasa a true al procesar una confirmacion valida y
    /// coincidente. Ninguna otra ruta puede establecerlo.
    bool verified() const { return phase == ValvePhase::Verified; }
};

}  // namespace wiga
