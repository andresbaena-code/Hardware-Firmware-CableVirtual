// Adaptador de IDigitalInputPort. Contrato: ports.md seccion 2 (IN-01..IN-04).
// Cubre: FR-001
#pragma once

#include "i_digital_input_port.h"

namespace wiga {
namespace hal {

class ArduinoDigitalInput : public IDigitalInputPort {
public:
    void configure() override;
    SignalLevel read(ValveIndex index) const override;
};

}  // namespace hal
}  // namespace wiga
