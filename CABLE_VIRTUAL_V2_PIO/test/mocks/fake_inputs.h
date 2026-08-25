// Doble de prueba de IDigitalInputPort. Niveles fijados por la prueba, con
// capacidad de simular rebote.
#pragma once

#include "i_digital_input_port.h"
#include "installation_map.h"

namespace wiga {
namespace test {

class FakeInputs : public IDigitalInputPort {
public:
    FakeInputs() {
        // Reposo: pull-up activo => nivel alto => valvula cerrada.
        for (uint8_t i = 0; i < config::kValveCount; ++i) {
            levels_[i] = SignalLevel::High;
        }
    }

    void configure() override { configured_ = true; }

    SignalLevel read(ValveIndex index) const override { return levels_[index]; }

    void set(ValveIndex index, SignalLevel level) { levels_[index] = level; }
    void requestOpen(ValveIndex index) { levels_[index] = SignalLevel::Low; }
    void requestClose(ValveIndex index) { levels_[index] = SignalLevel::High; }

    bool configured() const { return configured_; }

private:
    SignalLevel levels_[config::kValveCount];
    bool        configured_ = false;
};

}  // namespace test
}  // namespace wiga
