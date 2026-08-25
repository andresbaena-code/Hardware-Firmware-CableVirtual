// Banco de pruebas del supervisor: reloj, entradas, radio, panel y consola
// falsos, cableados igual que en src/main.cpp pero sin una sola linea de
// Arduino. Es lo que hace verificable la logica de control en el escritorio.
#pragma once

#include "fake_clock.h"
#include "fake_display.h"
#include "fake_inputs.h"
#include "fake_log.h"
#include "fake_radio.h"
#include "valve_supervisor.h"

namespace wiga {
namespace test {

// --- Derivacion del destino desde el mapa -----------------------------------
// Las pruebas NO fijan direcciones de radio a mano. Si lo hicieran, cambiar la
// finca obligaria a editarlas, y la promesa del criterio SC-004 ("configurar
// una instalacion nueva toca un solo archivo") seria falsa.

inline uint32_t nodeOf(ValveIndex index) {
    return config::valveMapping(index).nodeAddressLsb;
}

inline uint8_t idOf(ValveIndex index) {
    return config::valveMapping(index).valveId;
}

/// Ultima valvula del mapa: sirve como canal "lejano" en cualquier instalacion.
constexpr ValveIndex kLastValve = config::kValveCount - 1;

/// Cuantas valvulas se averian en las pruebas de aislamiento, dejando siempre
/// al menos una sana.
constexpr ValveIndex kFailingCount =
    (config::kValveCount > 8) ? 8 : static_cast<ValveIndex>(config::kValveCount - 1);

static_assert(config::kValveCount >= 3,
              "las pruebas de integracion necesitan al menos 3 valvulas");

struct Bench {
    FakeClock       clock;
    FakeInputs      inputs;
    FakeRadio       radio;
    FakeDisplay     display;
    FakeLog         log;
    ValveSupervisor supervisor;

    Bench() : supervisor(clock, inputs, radio, display, log) {}

    /// Salta `ms` de golpe y ejecuta UNA vuelta del lazo.
    /// Usar solo para comprobar un instante concreto: no representa la cadencia
    /// real y deja al planificador con una sola oportunidad de disparo.
    void step(uint32_t ms) {
        clock.advance(ms);
        supervisor.tick();
    }

    /// Deja correr el sistema durante `ms` a la cadencia real del lazo.
    /// Es la forma correcta de simular el paso del tiempo cuando intervienen
    /// varias valvulas: en 30 s reales hay 2000 vueltas, no una.
    void run(uint32_t ms) {
        const uint32_t period = config::kLoopPeriodMs;
        for (uint32_t elapsedMs = 0; elapsedMs < ms; elapsedMs += period) {
            clock.advance(period);
            supervisor.tick();
        }
    }

    /// Conmuta una entrada y deja pasar la ventana antirebote completa.
    void settleInput(uint32_t debounceMs = config::kDebounceMs) {
        supervisor.tick();       // aparece el candidato
        step(debounceMs);        // se consolida el nivel estable
    }
};

}  // namespace test
}  // namespace wiga
