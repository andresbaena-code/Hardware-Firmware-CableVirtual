// Cubre: FR-108, FR-121
#include "rs485_stream_adapter.h"

#include "time_math.h"

namespace wiga {
namespace hal {
namespace {

/// Margen sobre la duracion calculada, para absorber la resolucion de millis()
/// y el ultimo bit que sigue en el registro de desplazamiento. Soltar DE antes
/// de tiempo truncaria la propia respuesta (regla DE-02).
constexpr uint32_t kTxMarginMs = 3;

constexpr uint8_t kBitsPerByte = 10;  // 8N1: arranque + 8 datos + parada

}  // namespace

Rs485StreamAdapter::Rs485StreamAdapter(HardwareSerial& port, uint8_t deRePin,
                                       uint32_t baudRate)
    : port_(port), deRePin_(deRePin), baudRate_(baudRate) {}

void Rs485StreamAdapter::begin() {
    port_.begin(baudRate_);
    pinMode(deRePin_, OUTPUT);
    digitalWrite(deRePin_, LOW);  // DE-05: el reposo es RECEPCION
    txActive_  = false;
    pendingLen_ = 0;
    pendingSent_ = 0;
}

uint8_t Rs485StreamAdapter::available() const {
    // DE-04: mientras transmitimos, lo que entra es nuestro propio eco.
    if (txActive_) {
        return 0;
    }
    const int pending = port_.available();
    if (pending <= 0) {
        return 0;
    }
    return pending > 255 ? 255 : static_cast<uint8_t>(pending);
}

uint8_t Rs485StreamAdapter::read() {
    const int value = port_.read();
    return value < 0 ? 0 : static_cast<uint8_t>(value);
}

uint32_t Rs485StreamAdapter::transmitDurationMs(uint8_t bytes) const {
    const uint32_t bits = static_cast<uint32_t>(bytes) * kBitsPerByte;
    // Redondeo hacia arriba, para no quedarse corto.
    const uint32_t ms = (bits * 1000UL + baudRate_ - 1UL) / baudRate_;
    return ms + kTxMarginMs;
}

void Rs485StreamAdapter::beginTransmission(uint8_t length) {
    digitalWrite(deRePin_, HIGH);  // DE-01: la linea es nuestra
    txActive_    = true;
    pendingLen_  = 0;
    pendingSent_ = 0;
    txDoneAtMs_  = millis() + transmitDurationMs(length);
}

void Rs485StreamAdapter::write(const uint8_t* data, uint8_t length) {
    // Se copia al buffer propio y se entrega en service(). Escribir aqui
    // directamente bloquearia cuando la respuesta no cupiera en el buffer de
    // del puerto, que es de 64 B frente a los 69 B del peor caso.
    for (uint8_t i = 0; i < length && pendingLen_ < kPendingCapacity; ++i) {
        pending_[pendingLen_++] = data[i];
    }
    service();  // adelanta lo que quepa ya mismo
}

void Rs485StreamAdapter::service() {
    while (pendingSent_ < pendingLen_) {
        // availableForWrite() dice cuanto cabe SIN bloquear. Si no cabe nada,
        // se deja para la vuelta siguiente.
        if (port_.availableForWrite() <= 0) {
            return;
        }
        port_.write(pending_[pendingSent_++]);
    }
}

bool Rs485StreamAdapter::transmitting() const {
    if (!txActive_) {
        return false;
    }
    // Quedan bytes por entregar al hardware.
    if (pendingSent_ < pendingLen_) {
        return true;
    }
    // Doble condicion (regla DE-02): tiene que haber pasado el tiempo estimado
    // Y haberse vaciado el buffer de transmision.
    if (!reached(millis(), txDoneAtMs_)) {
        return true;
    }
    return port_.availableForWrite() < (SERIAL_TX_BUFFER_SIZE - 1);
}

void Rs485StreamAdapter::endTransmission() {
    if (transmitting()) {
        return;  // DE-02: soltar ahora truncaria la respuesta
    }
    digitalWrite(deRePin_, LOW);  // vuelta a RECEPCION
    txActive_    = false;
    pendingLen_  = 0;
    pendingSent_ = 0;
}

}  // namespace hal
}  // namespace wiga
