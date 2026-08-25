// Cubre: FR-002
#include "debouncer.h"

#include "time_math.h"

namespace wiga {

void Debouncer::seed(SignalLevel level, uint32_t nowMs) {
    stable_    = level;
    candidate_ = level;
    sinceMs_   = nowMs;
}

bool Debouncer::update(SignalLevel raw, uint32_t nowMs, uint16_t windowMs) {
    if (raw != candidate_) {
        // D-01: cualquier lectura distinta reinicia la ventana.
        candidate_ = raw;
        sinceMs_   = nowMs;
        return false;
    }
    if (candidate_ != stable_ && elapsed(nowMs, sinceMs_, windowMs)) {
        stable_ = candidate_;
        return true;
    }
    return false;
}

}  // namespace wiga
