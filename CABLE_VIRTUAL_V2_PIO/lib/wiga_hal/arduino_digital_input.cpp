// Cubre: FR-001, FR-021b
#include "arduino_digital_input.h"

#include <Arduino.h>

#include "installation_map.h"

namespace wiga {
namespace hal {

void ArduinoDigitalInput::configure() {
    // IN-01: el modo del pin lo decide el CABLEADO del tablero, declarado en
    // installation_map.h, no una suposicion de este adaptador.
    //
    // Esta instalacion energiza la entrada a 3,3 V al activarla y la sujeta con
    // pull-down externa en reposo. Activar ademas el pull-up interno formaria un
    // divisor con esa resistencia y podria dejar el tablero ilegible.
    const uint8_t mode = config::kPanelUsesInternalPullup ? INPUT_PULLUP : INPUT;
    for (uint8_t i = 0; i < config::kValveCount; ++i) {
        pinMode(config::valveMapping(i).inputPin, mode);
    }
}

SignalLevel ArduinoDigitalInput::read(ValveIndex index) const {
    if (index >= config::kValveCount) return SignalLevel::High;
    // IN-02: se devuelve el nivel CRUDO. La conversion a estado fisico ocurre
    // en un unico punto del dominio, no aqui.
    const uint8_t pin = config::valveMapping(index).inputPin;
    return (digitalRead(pin) == HIGH) ? SignalLevel::High : SignalLevel::Low;
}

}  // namespace hal
}  // namespace wiga
