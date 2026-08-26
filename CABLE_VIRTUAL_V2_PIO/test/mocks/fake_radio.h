// Doble de prueba de IRadioPort.
//
// Registra las ordenes transmitidas y permite encolar confirmaciones
// arbitrarias, incluidas las de nodos no configurados. NO ofrece ninguna forma
// de vaciar el buffer de entrada: la regla RD-02 (FR-018) queda garantizada por
// la ausencia de esa operacion en el contrato.
#pragma once

#include "i_radio_port.h"

namespace wiga {
namespace test {

constexpr uint8_t kFakeRadioCapacity = 64;

class FakeRadio : public IRadioPort {
public:
    void begin() override { begun_ = true; }

    bool send(const proto::ValveCommand& command) override {
        if (failNextSend_) {
            failNextSend_ = false;
            return false;
        }
        if (sentCount_ < kFakeRadioCapacity) {
            sent_[sentCount_] = command;
        }
        ++sentCount_;
        return true;
    }

    uint8_t poll(proto::ValveReport* out, uint8_t maxReports) override {
        uint8_t written = 0;
        while (written < maxReports && queueHead_ < queueCount_) {
            out[written++] = queue_[queueHead_++];
        }
        if (queueHead_ >= queueCount_) {
            queueHead_  = 0;
            queueCount_ = 0;
        }
        return written;
    }

    uint16_t framingErrors() const override { return framingErrors_; }

    // --- API de prueba ---
    void enqueue(uint32_t nodeAddressLsb, uint8_t valveId, SignalLevel level) {
        if (queueCount_ >= kFakeRadioCapacity) return;
        queue_[queueCount_].nodeAddressLsb = nodeAddressLsb;
        queue_[queueCount_].valveId        = valveId;
        queue_[queueCount_].level          = level;
        ++queueCount_;
    }

    void failNextSend() { failNextSend_ = true; }

    // RA-08: inyectar tramas corruptas para verificar FR-016 en el supervisor.
    void enqueueCorrupt(uint32_t nodeAddressLsb) {
        if (queueCount_ >= kFakeRadioCapacity) return;
        queue_[queueCount_].nodeAddressLsb = nodeAddressLsb;
        queue_[queueCount_].valveId        = 0xFF;  // id invalido
        queue_[queueCount_].level          = SignalLevel::High;
        ++queueCount_;
    }

    uint16_t sentCount() const { return sentCount_; }
    const proto::ValveCommand& sent(uint16_t i) const {
        return sent_[i < kFakeRadioCapacity ? i : kFakeRadioCapacity - 1];
    }
    const proto::ValveCommand& lastSent() const {
        return sent_[sentCount_ > 0 ? sentCount_ - 1 : 0];
    }
    void clearSent() { sentCount_ = 0; }
    bool begun() const { return begun_; }

private:
    proto::ValveCommand sent_[kFakeRadioCapacity];
    proto::ValveReport  queue_[kFakeRadioCapacity];
    uint16_t sentCount_     = 0;
    uint8_t  queueCount_    = 0;
    uint8_t  queueHead_     = 0;
    uint16_t framingErrors_ = 0;
    bool     begun_         = false;
    bool     failNextSend_  = false;
};

}  // namespace test
}  // namespace wiga
