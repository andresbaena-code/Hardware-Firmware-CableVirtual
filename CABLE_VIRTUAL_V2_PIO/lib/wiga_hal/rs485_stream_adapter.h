// -----------------------------------------------------------------------------
//  Adaptador de IByteStream sobre un puerto serie del AVR y un MAX485.
//  Cubre: FR-108, FR-121  (contracts/modbus-slave.md seccion 4)
//
//  Este adaptador NO TOMA NINGUNA DECISION de protocolo: solo mueve bytes entre
//  el puerto serie y la capa pura lib/wiga_modbus. Toda la logica que decide vive
//  alli, donde el entorno de pruebas puede ejercitarla.
//
//  El PUERTO se inyecta en vez de fijarlo aqui: cual de los cuatro UART lleva el
//  transceptor es una decision de cableado de la placa, y por tanto pertenece a
//  la raiz de composicion. En esta placa es Serial2 (TXD2=PH1, RXD2=PH0).
//
//  PROHIBIDO flush(): para la respuesta mayor de 53 bytes a 9600
//  baudios bloquearia 55,2 ms, veintisiete veces el limite de 2 ms del
//  Principio I. En su lugar hay una maquina de transmision que libera la linea
//  en una vuelta POSTERIOR del lazo.
// -----------------------------------------------------------------------------
#pragma once

#include <Arduino.h>

#include "i_byte_stream.h"

namespace wiga {
namespace hal {

class Rs485StreamAdapter : public IByteStream {
public:
    Rs485StreamAdapter(HardwareSerial& port, uint8_t deRePin, uint32_t baudRate);

    void    begin() override;
    uint8_t available() const override;
    uint8_t read() override;
    void    beginTransmission(uint8_t length) override;
    void    write(const uint8_t* data, uint8_t length) override;
    void    service() override;
    bool    transmitting() const override;
    void    endTransmission() override;

private:
    /// Milisegundos que tardan `bytes` en salir del cable, con margen.
    /// A 8N1 son 10 bits por byte en la linea.
    uint32_t transmitDurationMs(uint8_t bytes) const;

    /// Peor caso: leer los 32 registros de una vez son 3 + 64 + 2 = 69 bytes.
    static constexpr uint8_t kPendingCapacity = 72;

    HardwareSerial& port_;
    const uint8_t  deRePin_;
    const uint32_t baudRate_;
    uint8_t        pending_[kPendingCapacity];
    uint8_t        pendingLen_  = 0;
    uint8_t        pendingSent_ = 0;
    uint32_t       txDoneAtMs_  = 0;
    bool           txActive_    = false;
};

}  // namespace hal
}  // namespace wiga
