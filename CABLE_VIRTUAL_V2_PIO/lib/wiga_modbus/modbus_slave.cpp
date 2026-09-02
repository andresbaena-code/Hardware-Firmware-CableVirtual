// Cubre: FR-101, FR-103, FR-104, FR-105, FR-106, FR-109, FR-113, FR-114
#include "modbus_slave.h"

#include "modbus_crc.h"
#include "time_math.h"

namespace wiga {
namespace modbus {
namespace {

/// Codigos de funcion de escritura. No se soportan: el tablero es la unica
/// fuente de verdad del estado pedido, y ninguna trama del bus puede alterarlo
/// (regla MB-06). Se listan para poder rechazarlos con excepcion en vez de con
/// silencio, que es lo que el contrato exige.
constexpr uint8_t kWriteSingleCoil      = 0x05;
constexpr uint8_t kWriteSingleRegister  = 0x06;
constexpr uint8_t kWriteMultipleCoils   = 0x0F;
constexpr uint8_t kWriteMultipleRegs    = 0x10;

constexpr uint8_t kHeaderLength = 2;  // id + funcion

uint16_t word16(uint8_t hi, uint8_t lo) {
    return static_cast<uint16_t>((static_cast<uint16_t>(hi) << 8) | lo);
}

bool isSupportedRead(uint8_t functionCode) {
    return functionCode == fc::kReadCoils ||
           functionCode == fc::kReadDiscreteInputs ||
           functionCode == fc::kReadHoldingRegisters ||
           functionCode == fc::kReadInputRegisters;
}

bool isBitFunction(uint8_t functionCode) {
    return functionCode == fc::kReadCoils ||
           functionCode == fc::kReadDiscreteInputs;
}

}  // namespace

ModbusSlave::ModbusSlave(RegisterFile& registers, uint8_t slaveId, uint16_t silenceMs)
    : registers_(registers), slaveId_(slaveId), silenceMs_(silenceMs) {
    for (uint8_t i = 0; i < kRxCapacity; ++i) rx_[i] = 0;
    for (uint8_t i = 0; i < kTxCapacity; ++i) tx_[i] = 0;
}

// --- Ensamblado de trama -----------------------------------------------------

void ModbusSlave::resetRx() {
    rxLen_     = 0;
    overflow_  = false;
    receiving_ = false;
}

void ModbusSlave::bumpFormatError() {
    if (stats_.formatErrors < 0xFFFF) ++stats_.formatErrors;
}

void ModbusSlave::onByte(uint8_t value, uint32_t nowMs) {
    // El silencio entre tramas descarta lo parcial y resincroniza (MB-09).
    if (receiving_ && elapsed(nowMs, lastByteAtMs_, silenceMs_)) {
        if (rxLen_ > 0) bumpFormatError();
        resetRx();
    }

    receiving_    = true;
    lastByteAtMs_ = nowMs;

    if (rxLen_ < kRxCapacity) {
        rx_[rxLen_++] = value;
    } else {
        // MB-10: no cabe. Se marca y se descarta hasta el proximo silencio;
        // jamas se escribe fuera del buffer.
        overflow_ = true;
        return;
    }

    if (overflow_) return;

    const uint8_t expected = expectedLength();
    if (expected != 0 && rxLen_ >= expected) {
        processFrame(nowMs);
        resetRx();
    }
}

void ModbusSlave::poll(uint32_t nowMs) {
    if (!receiving_) return;
    if (!elapsed(nowMs, lastByteAtMs_, silenceMs_)) return;

    // Vencio el silencio con una trama a medias: se descarta.
    if (rxLen_ > 0 || overflow_) bumpFormatError();
    resetRx();
}

uint8_t ModbusSlave::expectedLength() const {
    if (rxLen_ < kHeaderLength) {
        return 0;  // todavia no se conoce el codigo de funcion
    }
    const uint8_t functionCode = rx_[1];

    if (isSupportedRead(functionCode) ||
        functionCode == kWriteSingleCoil ||
        functionCode == kWriteSingleRegister) {
        return kReadRequestLength;  // 8 bytes fijos
    }

    if (functionCode == kWriteMultipleCoils || functionCode == kWriteMultipleRegs) {
        // id, fc, addr(2), qty(2), byteCount(1), datos(N), crc(2)
        if (rxLen_ < 7) return 0;
        const uint32_t total = 7u + static_cast<uint32_t>(rx_[6]) + kCrcLength;
        // Aritmetica de 32 bits: en 8 bits esta suma daria la vuelta, que es
        // exactamente el defecto verificado de la implementacion anterior.
        return total > kRxCapacity ? 0xFF : static_cast<uint8_t>(total);
    }

    // Funcion desconocida: se asume el tamano estandar para poder validarla y
    // responder excepcion en vez de callar.
    return kReadRequestLength;
}

// --- Analisis y respuesta ----------------------------------------------------

void ModbusSlave::processFrame(uint32_t nowMs) {
    // MB-01: dirigida a otro identificador -> silencio absoluto, sin tocar nada.
    if (rx_[0] != slaveId_) {
        return;
    }

    // MB-02: el verificador se comprueba sobre la trama COMPLETA, que debe dar
    // cero. Si falla, silencio.
    if (crc16(rx_, rxLen_) != 0) {
        if (stats_.crcErrors < 0xFFFF) ++stats_.crcErrors;
        return;
    }

    const uint8_t functionCode = rx_[1];

    // A partir de aqui la peticion es valida en forma y es para nosotros.
    if (stats_.validRequests < 0xFFFF) ++stats_.validRequests;
    lastRequestAtMs_ = nowMs;
    everServed_      = true;

    // MB-04 y MB-06: toda escritura y toda funcion no soportada, con excepcion.
    if (!isSupportedRead(functionCode)) {
        buildException(functionCode, exc::kIllegalFunction);
        return;
    }

    const uint16_t start    = word16(rx_[2], rx_[3]);
    const uint16_t quantity = word16(rx_[4], rx_[5]);

    // MB-06: cantidad cero o por encima del limite del estandar.
    const uint16_t limit = isBitFunction(functionCode) ? kMaxReadBits
                                                       : kMaxReadRegisters;
    if (quantity == 0 || quantity > limit) {
        buildException(functionCode, exc::kIllegalDataValue);
        return;
    }

    // MB-05: el rango debe caber entero en el mapa.
    if (!RegisterFile::rangeFits(start, quantity)) {
        buildException(functionCode, exc::kIllegalDataAddress);
        return;
    }

    if (isBitFunction(functionCode)) {
        buildReadBits(start, quantity, functionCode);
    } else {
        buildReadRegisters(start, quantity, functionCode);
    }
}

Outcome ModbusSlave::buildReadBits(uint16_t start, uint16_t quantity,
                                   uint8_t functionCode) {
    const uint8_t byteCount = static_cast<uint8_t>((quantity + 7u) / 8u);

    // Guarda de capacidad: nunca se escribe fuera de tx_.
    if (static_cast<uint16_t>(3 + byteCount + kCrcLength) > kTxCapacity) {
        buildException(functionCode, exc::kIllegalDataValue);
        return Outcome::Exception;
    }

    tx_[0] = slaveId_;
    tx_[1] = functionCode;
    tx_[2] = byteCount;
    for (uint8_t i = 0; i < byteCount; ++i) tx_[3 + i] = 0;

    for (uint16_t i = 0; i < quantity; ++i) {
        if (registers_.coilBit(static_cast<uint16_t>(start + i))) {
            tx_[3 + (i / 8)] |= static_cast<uint8_t>(1u << (i % 8));
        }
    }

    finishFrame(static_cast<uint8_t>(3 + byteCount));
    return Outcome::Response;
}

Outcome ModbusSlave::buildReadRegisters(uint16_t start, uint16_t quantity,
                                        uint8_t functionCode) {
    const uint16_t byteCount = static_cast<uint16_t>(quantity * 2u);

    if (byteCount > 255u ||
        static_cast<uint16_t>(3 + byteCount + kCrcLength) > kTxCapacity) {
        buildException(functionCode, exc::kIllegalDataValue);
        return Outcome::Exception;
    }

    tx_[0] = slaveId_;
    tx_[1] = functionCode;
    tx_[2] = static_cast<uint8_t>(byteCount);

    for (uint16_t i = 0; i < quantity; ++i) {
        const uint16_t value = registers_.get(static_cast<uint16_t>(start + i));
        tx_[3 + i * 2]     = static_cast<uint8_t>(value >> 8);
        tx_[3 + i * 2 + 1] = static_cast<uint8_t>(value & 0xFF);
    }

    finishFrame(static_cast<uint8_t>(3 + byteCount));
    return Outcome::Response;
}

void ModbusSlave::buildException(uint8_t functionCode, uint8_t code) {
    tx_[0] = slaveId_;
    tx_[1] = static_cast<uint8_t>(functionCode | kExceptionFlag);
    tx_[2] = code;
    finishFrame(3);
}

bool ModbusSlave::masterSilent(uint32_t nowMs, uint32_t thresholdMs) const {
    if (!everServed_) {
        return true;  // nunca pregunto nadie
    }
    // Aritmetica modular: correcta al desbordar millis() a los 49,7 dias.
    return elapsed(nowMs, lastRequestAtMs_, thresholdMs);
}

uint16_t ModbusSlave::secondsSinceRequest(uint32_t nowMs) const {
    if (!everServed_) {
        return 0xFFFF;
    }
    const uint32_t deltaMs = static_cast<uint32_t>(nowMs - lastRequestAtMs_);
    const uint32_t seconds = deltaMs / 1000UL;
    return seconds > 0xFFFFUL ? 0xFFFF : static_cast<uint16_t>(seconds);
}

void ModbusSlave::finishFrame(uint8_t bodyLength) {
    const uint16_t crc = crc16(tx_, bodyLength);
    // El verificador viaja con el byte BAJO primero, al reves que el resto de
    // campos de 16 bits del protocolo.
    tx_[bodyLength]     = static_cast<uint8_t>(crc & 0xFF);
    tx_[bodyLength + 1] = static_cast<uint8_t>(crc >> 8);
    txLen_              = static_cast<uint8_t>(bodyLength + kCrcLength);
}

}  // namespace modbus
}  // namespace wiga
