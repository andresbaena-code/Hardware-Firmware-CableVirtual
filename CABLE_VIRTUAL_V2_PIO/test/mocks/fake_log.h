// Doble de prueba de ILogPort. Acumula eventos para aserciones.
#pragma once

#include "i_log_port.h"

#include <string.h>

namespace wiga {
namespace test {

constexpr uint16_t kFakeLogCapacity = 256;

struct LoggedEvent {
    LogLevel   level;
    const char* tag;
    ValveIndex index;
    int32_t    value;
};

class FakeLog : public ILogPort {
public:
    void begin() override { begun_ = true; }

    void event(LogLevel level, const char* tag,
               ValveIndex index, int32_t value) override {
        if (count_ < kFakeLogCapacity) {
            events_[count_].level = level;
            events_[count_].tag   = tag;
            events_[count_].index = index;
            events_[count_].value = value;
        }
        ++count_;
    }

    void message(LogLevel level, const char* text) override {
        (void)level;
        (void)text;
        ++messages_;
    }

    // --- API de prueba ---
    uint16_t count() const { return count_; }
    const LoggedEvent& at(uint16_t i) const { return events_[i]; }
    void clear() { count_ = 0; }

    uint16_t countOf(const char* tag) const {
        uint16_t n = 0;
        const uint16_t limit = (count_ < kFakeLogCapacity) ? count_ : kFakeLogCapacity;
        for (uint16_t i = 0; i < limit; ++i) {
            if (strcmp(events_[i].tag, tag) == 0) ++n;
        }
        return n;
    }

    uint16_t countOf(const char* tag, ValveIndex index) const {
        uint16_t n = 0;
        const uint16_t limit = (count_ < kFakeLogCapacity) ? count_ : kFakeLogCapacity;
        for (uint16_t i = 0; i < limit; ++i) {
            if (events_[i].index == index && strcmp(events_[i].tag, tag) == 0) ++n;
        }
        return n;
    }

    bool begun() const { return begun_; }

private:
    LoggedEvent events_[kFakeLogCapacity];
    uint16_t    count_    = 0;
    uint16_t    messages_ = 0;
    bool        begun_    = false;
};

}  // namespace test
}  // namespace wiga
