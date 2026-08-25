// Cubre: FR-004
#include "latch_scheduler.h"

#include "time_math.h"

namespace wiga {

bool LatchScheduler::ready(uint32_t nowMs) const {
    if (!active_) return true;
    return elapsed(nowMs, guardStartMs_, guardMs_);
}

ValveIndex LatchScheduler::grant(const ValveRecord* records, uint8_t count,
                                 uint32_t nowMs) const {
    if (records == nullptr || count == 0) return kNoValve;
    if (!ready(nowMs)) return kNoValve;  // S-01: una orden a la vez

    // S-02: barrido circular desde la siguiente a la ultima atendida.
    for (uint8_t offset = 1; offset <= count; ++offset) {
        const uint8_t i = static_cast<uint8_t>((lastServed_ + offset) % count);
        if (records[i].phase == ValvePhase::Pending) {
            return i;
        }
    }
    return kNoValve;
}

void LatchScheduler::markServed(ValveIndex index, uint32_t nowMs) {
    lastServed_   = index;
    guardStartMs_ = nowMs;
    active_       = true;
}

}  // namespace wiga
