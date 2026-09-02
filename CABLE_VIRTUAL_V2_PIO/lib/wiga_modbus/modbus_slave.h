// -----------------------------------------------------------------------------
//  Esclavo Modbus RTU: ensamblado de trama, analisis y composicion de respuesta.
//  Cubre: FR-101, FR-103, FR-104, FR-105, FR-106, FR-109, FR-113, FR-114
//         (contracts/modbus-slave.md secciones 2 y 3)
//
//  SIN ENTRADA/SALIDA PROPIA: bytes entran, bytes salen. Esa ausencia de E/S es
//  lo que la hace comprobable en env:native, y es la diferencia estructural con
//  la implementacion anterior, que hablaba con Serial1 directamente y por eso
//  acumulo 25 defectos sin que ninguna prueba la tocase.
//
//  Esta cabecera NO incluye <Arduino.h>. Ver lib/wiga_modbus/README.md.
// -----------------------------------------------------------------------------
#pragma once

#include <stdint.h>

#include "modbus_types.h"
#include "register_file.h"

namespace wiga {
namespace modbus {

/// Contadores de salud del bus. Saturan en su maximo en vez de dar la vuelta:
/// un contador que vuelve a cero miente sobre la salud del bus (FR-113).
struct BusStats {
    uint16_t validRequests = 0;
    uint16_t crcErrors     = 0;
    uint16_t formatErrors  = 0;
};

/// Capacidad de recepcion. La peticion mayor soportada son 8 bytes; el margen
/// permite reconocer y rechazar con excepcion las escrituras de longitud
/// variable pequenas en vez de limitarse a callar.
constexpr uint8_t kRxCapacity = 16;

/// Capacidad de respuesta. Peor caso: leer los 32 registros de una vez son
/// 3 + 64 + 2 = 69 bytes.
constexpr uint8_t kTxCapacity = 72;

class ModbusSlave {
public:
    ModbusSlave(RegisterFile& registers, uint8_t slaveId, uint16_t silenceMs);

    /// Acumula un byte recibido del bus.
    void onByte(uint8_t value, uint32_t nowMs);

    /// Resincroniza: descarta una trama parcial cuando vence el silencio.
    void poll(uint32_t nowMs);

    // --- Respuesta pendiente -------------------------------------------------
    bool hasResponse() const { return txLen_ > 0; }
    const uint8_t* response() const { return tx_; }
    uint8_t responseLength() const { return txLen_; }

    /// El adaptador confirma que la entrego al hardware.
    void responseSent() { txLen_ = 0; }

    // --- Diagnostico ---------------------------------------------------------
    const BusStats& stats() const { return stats_; }

    /// Marca de la ultima peticion VALIDA dirigida a nosotros. Alimenta la
    /// deteccion de maestro ausente (FR-114).
    uint32_t lastRequestAtMs() const { return lastRequestAtMs_; }
    bool everServed() const { return everServed_; }

    /// true si el maestro lleva mas de `thresholdMs` sin preguntar, o si nunca
    /// pregunto. Es el fallo mas probable en campo -UC100 apagado o cable RS485
    /// cortado- y el unico que el controlador puede detectar por si solo.
    ///
    /// Vive aqui y no en la raiz de composicion para que sea comprobable en
    /// host: una decision en main.cpp queda fuera del alcance de las pruebas.
    bool masterSilent(uint32_t nowMs, uint32_t thresholdMs) const;

    /// Segundos desde la ultima peticion valida, saturados a 0xFFFF. Si nunca
    /// hubo ninguna, devuelve 0xFFFF.
    uint16_t secondsSinceRequest(uint32_t nowMs) const;

private:
    /// Longitud total esperada de la peticion en curso, o 0 si aun no se puede
    /// determinar con los bytes recibidos.
    uint8_t expectedLength() const;

    /// Valida y atiende la trama completa que hay en rx_.
    void processFrame(uint32_t nowMs);

    Outcome buildReadBits(uint16_t start, uint16_t quantity, uint8_t functionCode);
    Outcome buildReadRegisters(uint16_t start, uint16_t quantity, uint8_t functionCode);
    void    buildException(uint8_t functionCode, uint8_t code);
    void    finishFrame(uint8_t bodyLength);

    void resetRx();
    void bumpFormatError();

    RegisterFile& registers_;
    const uint8_t  slaveId_;
    const uint16_t silenceMs_;

    uint8_t  rx_[kRxCapacity];
    uint8_t  rxLen_          = 0;
    uint32_t lastByteAtMs_   = 0;
    bool     overflow_       = false;
    bool     receiving_      = false;

    uint8_t  tx_[kTxCapacity];
    uint8_t  txLen_          = 0;

    BusStats stats_;
    uint32_t lastRequestAtMs_ = 0;
    bool     everServed_      = false;
};

}  // namespace modbus
}  // namespace wiga
