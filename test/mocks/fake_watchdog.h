// Doble de prueba de IWatchdogPort. Cuenta invocaciones para verificar WD-03
// (feed() en un unico punto del lazo).
#pragma once

#include "i_watchdog_port.h"

#include <stdint.h>

namespace wiga {
namespace test {

class FakeWatchdog : public IWatchdogPort {
public:
    void disable() override { ++disables_; enabled_ = false; }
    void enable() override { ++enables_; enabled_ = true; }
    void feed() override { ++feeds_; }

    uint32_t feeds() const { return feeds_; }
    uint32_t enables() const { return enables_; }
    uint32_t disables() const { return disables_; }
    bool enabled() const { return enabled_; }

private:
    uint32_t feeds_    = 0;
    uint32_t enables_  = 0;
    uint32_t disables_ = 0;
    bool     enabled_  = false;
};

}  // namespace test
}  // namespace wiga
