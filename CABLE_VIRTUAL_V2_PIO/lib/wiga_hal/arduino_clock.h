// Adaptador de IClock sobre millis(). Contrato: ports.md seccion 1.
#pragma once

#include <Arduino.h>

#include "i_clock.h"

namespace wiga {
namespace hal {

class ArduinoClock : public IClock {
public:
    uint32_t nowMs() const override { return millis(); }
};

}  // namespace hal
}  // namespace wiga
