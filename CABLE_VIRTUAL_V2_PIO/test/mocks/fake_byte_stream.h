// -----------------------------------------------------------------------------
//  Doble de prueba de IByteStream: una tuberia de bytes en memoria.
//  Cubre: FR-123
//
//  Modela lo que de verdad hay debajo del protocolo, que son bytes. Un doble a
//  nivel de bytes NO PUEDE mentir sobre el protocolo, porque no lo conoce.
//
//  Sustituye al FakeModbus anterior, que era infiel al adaptador real de tres
//  maneras conocidas: entregaba cada reporte una sola vez, no podia expresar un
//  fallo persistente de envio, y su contador de enviados desbordaba su propia
//  capacidad en silencio.
// -----------------------------------------------------------------------------
#pragma once

#include <stdint.h>

#include "i_byte_stream.h"

namespace wiga {
namespace test {

constexpr uint16_t kFakeStreamCapacity = 512;

class FakeByteStream : public IByteStream {
public:
    // --- IByteStream ---------------------------------------------------------

    void begin() override {
        begun_ = true;
        transmitting_ = false;
    }

    uint8_t available() const override {
        const uint16_t pending = static_cast<uint16_t>(rxCount_ - rxHead_);
        return pending > 255 ? 255 : static_cast<uint8_t>(pending);
    }

    uint8_t read() override {
        if (rxHead_ >= rxCount_) {
            return 0;
        }
        return rx_[rxHead_++];
    }

    void beginTransmission(uint8_t length) override {
        (void)length;
        transmitting_ = true;
        ++transmissionCount_;
    }

    void write(const uint8_t* data, uint8_t length) override {
        for (uint8_t i = 0; i < length && txCount_ < kFakeStreamCapacity; ++i) {
            tx_[txCount_++] = data[i];
        }
    }

    /// El doble entrega los bytes al instante, asi que no hay nada que avanzar.
    void service() override { ++serviceCalls_; }

    bool transmitting() const override { return transmitting_; }

    void endTransmission() override {
        // Fiel al contrato: soltar la linea antes de tiempo no debe surtir
        // efecto, para que una prueba pueda detectar esa infraccion.
        if (!transmitting_) {
            ++releasedWhileIdle_;
        }
        transmitting_ = false;
    }

    // --- API de prueba: alimentar el bus -------------------------------------

    /// Encola bytes como si llegaran del maestro.
    void feed(const uint8_t* data, uint16_t length) {
        for (uint16_t i = 0; i < length && rxCount_ < kFakeStreamCapacity; ++i) {
            rx_[rxCount_++] = data[i];
        }
    }

    void feedByte(uint8_t b) { feed(&b, 1); }

    /// Simula que el hardware termino de transmitir. Sin esto, transmitting()
    /// se mantiene, que es justo el comportamiento que el adaptador debe
    /// respetar antes de soltar la linea.
    void completeTransmission() { transmitting_ = false; }

    // --- API de prueba: observar lo transmitido ------------------------------

    uint16_t sentCount() const { return txCount_; }
    const uint8_t* sent() const { return tx_; }

    uint8_t sentAt(uint16_t i) const {
        return i < txCount_ ? tx_[i] : 0;
    }

    bool sentEquals(const uint8_t* expected, uint16_t length) const {
        if (txCount_ != length) {
            return false;
        }
        for (uint16_t i = 0; i < length; ++i) {
            if (tx_[i] != expected[i]) {
                return false;
            }
        }
        return true;
    }

    void clearSent() { txCount_ = 0; }

    uint16_t transmissionCount() const { return transmissionCount_; }
    uint16_t serviceCalls() const { return serviceCalls_; }
    uint16_t releasedWhileIdle() const { return releasedWhileIdle_; }
    bool begun() const { return begun_; }

private:
    uint8_t  rx_[kFakeStreamCapacity];
    uint8_t  tx_[kFakeStreamCapacity];
    uint16_t rxCount_           = 0;
    uint16_t rxHead_            = 0;
    uint16_t txCount_           = 0;
    uint16_t transmissionCount_ = 0;
    uint16_t releasedWhileIdle_ = 0;
    uint16_t serviceCalls_      = 0;
    bool     begun_             = false;
    bool     transmitting_      = false;
};

}  // namespace test
}  // namespace wiga
