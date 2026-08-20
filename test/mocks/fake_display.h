// Doble de prueba de IDisplayPort. Expone los simbolos para aserciones.
#pragma once

#include "i_display_port.h"
#include "installation_map.h"

namespace wiga {
namespace test {

class FakeDisplay : public IDisplayPort {
public:
    FakeDisplay() {
        for (uint8_t i = 0; i < config::kValveCount; ++i) symbols_[i] = ' ';
    }

    void begin() override { begun_ = true; }

    void setValveSymbol(ValveIndex index, char symbol) override {
        symbols_[index] = symbol;
        ++writes_;
    }

    void setStatusLine(const char* text) override { status_ = text; }

    void tick() override { ++ticks_; }

    bool healthy() const override { return healthy_; }

    // --- API de prueba ---
    char symbol(ValveIndex index) const { return symbols_[index]; }
    const char* status() const { return status_; }
    uint32_t writes() const { return writes_; }
    uint32_t ticks() const { return ticks_; }
    void setHealthy(bool value) { healthy_ = value; }
    bool begun() const { return begun_; }

private:
    char        symbols_[config::kValveCount];
    const char* status_  = "";
    uint32_t    writes_  = 0;
    uint32_t    ticks_   = 0;
    bool        healthy_ = true;
    bool        begun_   = false;
};

}  // namespace test
}  // namespace wiga
