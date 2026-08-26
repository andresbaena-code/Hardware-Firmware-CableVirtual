// Cubre: FR-001, FR-021b
#include "arduino_digital_input.h"

#include <Arduino.h>

#include "installation_map.h"

namespace wiga {
namespace hal {

void ArduinoDigitalInput::configure() {
    // IN-01: pull-up interno. El contacto del tablero cierra a masa, de modo que
    // el nivel bajo corresponde a la solicitud de apertura (misma logica inversa
    // que la etapa de potencia del nodo).
    for (uint8_t i = 0; i < config::kValveCount; ++i) {
        pinMode(config::valveMapping(i).inputPin, INPUT_PULLUP);
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
