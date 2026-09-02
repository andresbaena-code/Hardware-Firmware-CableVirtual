// Doble de prueba de IDigitalInputPort. Niveles fijados por la prueba, con
// capacidad de simular rebote.
//
// El doble NO fija una convencion de cableado propia: la toma de la
// configuracion. Si manana el tablero se cablea al reves, las pruebas siguen
// diciendo lo mismo sin tocarlas, y un doble no puede mentir sobre la polaridad
// real de la instalacion.
#pragma once

#include "i_digital_input_port.h"
#include "installation_map.h"

namespace wiga {
namespace test {

class FakeInputs : public IDigitalInputPort {
public:
    FakeInputs() {
        for (uint8_t i = 0; i < config::kValveCount; ++i) {
            levels_[i] = config::kPanelIdleLevel;
        }
    }

    void configure() override { configured_ = true; }

    SignalLevel read(ValveIndex index) const override { return levels_[index]; }

    void set(ValveIndex index, SignalLevel level) { levels_[index] = level; }
    /// El operador pide riego para esa valvula.
    void requestOpen(ValveIndex index) { levels_[index] = config::kPanelActiveLevel; }
    /// El operador deja de pedir riego.
    void requestClose(ValveIndex index) { levels_[index] = config::kPanelIdleLevel; }

    bool configured() const { return configured_; }

private:
    SignalLevel levels_[config::kValveCount];
    bool        configured_ = false;
};

}  // namespace test
}  // namespace wiga
