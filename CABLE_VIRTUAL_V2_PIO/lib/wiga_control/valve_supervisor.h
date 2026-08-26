// -----------------------------------------------------------------------------
//  Supervisor de valvulas: orquesta la maquina de estados completa.
//  Cubre: FR-001, FR-003, FR-005, FR-009, FR-010, FR-011, FR-016, FR-017, FR-018
//         FR-019, FR-020, FR-021  (data-model.md seccion 4, transiciones T1..T11)
//
//  Esta clase NO conoce ningun tipo concreto de hardware: solo puertos. Por eso
//  se compila y se prueba en el equipo del desarrollador (Principio III y IV).
// -----------------------------------------------------------------------------
#pragma once

#include <stdint.h>

#include "debouncer.h"
#include "i_clock.h"
#include "i_digital_input_port.h"
#include "i_display_port.h"
#include "i_log_port.h"
#include "i_radio_port.h"
#include "installation_map.h"
#include "latch_scheduler.h"
#include "retry_policy.h"
#include "timing_config.h"
#include "valve_record.h"

namespace wiga {

/// Simbolos del panel. Representan el ESTADO FISICO de la valvula (NV-02).
namespace symbol {
constexpr char kClosed  = '0';
constexpr char kOpen    = '1';
constexpr char kPending = '?';
constexpr char kFailed  = '!';
}  // namespace symbol

class ValveSupervisor {
public:
    ValveSupervisor(IClock& clock,
                    IDigitalInputPort& inputs,
                    IRadioPort& radio,
                    IDisplayPort& display,
                    ILogPort& log);

    /// T1: siembra el estado desde las entradas fisicas. NO transmite ninguna
    /// orden: tras un reset por watchdog no puede haber una rafaga de disparos
    /// sobre la bateria (FR-010, Principio II).
    void begin();

    /// Una vuelta del lazo de control. No bloquea (FR-011).
    void tick();

    // --- Introspeccion para pruebas y diagnostico ---
    const ValveRecord& record(ValveIndex index) const {
        return records_[index < config::kValveCount ? index : 0];
    }
    uint8_t valveCount() const { return config::kValveCount; }

private:
    void pollInputs(uint32_t nowMs);
    void pollRadio(uint32_t nowMs);
    void serviceTimers(uint32_t nowMs);
    void serviceScheduler(uint32_t nowMs);
    void refreshDisplay();

    static char symbolFor(const ValveRecord& record);

    IClock&            clock_;
    IDigitalInputPort& inputs_;
    IRadioPort&        radio_;
    IDisplayPort&      display_;
    ILogPort&          log_;

    RetryPolicy    retry_;
    LatchScheduler scheduler_;

    ValveRecord records_[config::kValveCount];
    Debouncer   debouncers_[config::kValveCount];
    char        shownSymbol_[config::kValveCount];
};

}  // namespace wiga
