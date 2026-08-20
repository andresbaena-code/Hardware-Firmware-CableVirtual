// Doble de prueba de IClock. Permite posicionarse cerca del desbordamiento de
// 32 bits para verificar FR-013 sin esperar 49 dias.
#pragma once

#include "i_clock.h"

namespace wiga {
namespace test {

class FakeClock : public IClock {
public:
    uint32_t nowMs() const override { return now_; }

    void set(uint32_t ms) { now_ = ms; }
    void advance(uint32_t ms) { now_ += ms; }

private:
    uint32_t now_ = 0;
};

}  // namespace test
}  // namespace wiga
