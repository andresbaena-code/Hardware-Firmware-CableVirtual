// Adaptador de IWatchdogPort. Contrato: ports.md seccion 5 (WD-01..WD-04).
// Cubre: FR-012
#pragma once

#include "i_watchdog_port.h"

namespace wiga {
namespace hal {

class AvrWatchdog : public IWatchdogPort {
public:
    void disable() override;
    void enable() override;
    void feed() override;
};

}  // namespace hal
}  // namespace wiga
