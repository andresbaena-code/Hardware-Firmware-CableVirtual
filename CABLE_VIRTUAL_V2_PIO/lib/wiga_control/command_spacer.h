// -----------------------------------------------------------------------------
//  Separador de comandos por UC511.
//  Cubre: FR-204, FR-205, FR-206, FR-207, FR-208, FR-209, FR-210, FR-211,
//         FR-217 (codigo puro, verificado en test/test_control/test_command_spacer.cpp)
//  (specs/003-retardo-apertura-cancelable, contracts/command-spacer.md)
//
//  Un UC511 pierde sin aviso el segundo de dos comandos que le llegan con menos
//  de 5 s entre ellos. Esta clase decide CUANDO puede publicarse el cambio de
//  una valvula: nunca dos cambios del mismo equipo con menos de
//  kCommandSpacingMs entre ellos. Lo que llega dentro del intervalo espera su
//  turno, y si el tablero lo revierte mientras espera, se descarta sin que al
//  campo le llegue nada.
//
//  No conoce puertos, ni registros, ni el panel: recibe el estado deseado y el
//  tiempo como argumentos, y devuelve los cambios autorizados. Asi toda la
//  logica de turnos se prueba en host (Principios III y IV).
// -----------------------------------------------------------------------------
#pragma once

#include <stdint.h>

#include "installation_map.h"
#include "timing_config.h"
#include "valve_types.h"

namespace wiga {

class CommandSpacer {
public:
    /// Resultado de informar el estado deseado de una valvula.
    enum class Outcome : uint8_t {
        Unchanged,  ///< nada nuevo: ni pendiente creado ni cancelado
        Queued,     ///< deseado distinto del publicado: hay un cambio pendiente
        Cancelled,  ///< el deseado volvio al publicado antes de su turno
    };

    CommandSpacer();

    /// Siembra el estado publicado al arrancar, sin esperar (FR-211). `published`
    /// tiene config::kValveCount elementos.
    void begin(const ValveState* published, uint32_t nowMs);

    /// Informa el estado deseado de una valvula. Se llama en CADA vuelta, no solo
    /// en el flanco: un cambio puede tardar varias vueltas en publicarse.
    Outcome update(ValveIndex index, ValveState desired, uint32_t nowMs);

    /// Siguiente cambio autorizado en esta vuelta. Si hay uno, lo da por
    /// publicado, rellena `index` y `state` y devuelve true. Llamar hasta que
    /// devuelva false.
    bool next(uint32_t nowMs, ValveIndex& index, ValveState& state);

    ValveState published(ValveIndex index) const;
    bool pending(ValveIndex index) const;

private:
    bool actuatorFree(uint8_t slot, uint32_t nowMs) const;
    bool takesPrecedence(ValveIndex candidate, ValveIndex current, uint32_t nowMs) const;

    static uint8_t slotOf(ValveIndex index) {
        return static_cast<uint8_t>(config::actuatorOf(index) - 1);
    }

    // Por valvula. El estado pendiente es siempre el contrario del publicado,
    // asi que no se guarda (data-model.md 2.1).
    ValveState published_[config::kValveCount];
    bool       pending_[config::kValveCount];
    uint32_t   pendingSinceMs_[config::kValveCount];

    // Por UC511, indexado por numero de equipo - 1.
    uint32_t lastChangeMs_[config::kActuatorCount];
    bool     hasLastChange_[config::kActuatorCount];
};

}  // namespace wiga
