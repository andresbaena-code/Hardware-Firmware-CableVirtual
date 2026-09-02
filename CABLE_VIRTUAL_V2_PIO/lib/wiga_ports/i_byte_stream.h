// -----------------------------------------------------------------------------
//  Puerto de bytes con control de direccion del bus.
//  Cubre: FR-108, FR-121  (contracts/modbus-slave.md seccion 1.1)
//
//  Aisla la capa pura del protocolo del hardware serie. Es lo unico que
//  lib/wiga_modbus necesita del mundo exterior.
//
//  REGLA DE ORO: ninguna operacion puede bloquear mas de lo que cuesta mover un
//  byte a un buffer. La espera de que la trama salga del cable se resuelve ENTRE
//  vueltas del lazo consultando transmitting(), nunca dentro de una llamada.
//
//  Es la diferencia con la implementacion anterior, que hacia Serial1.flush():
//  para la respuesta mayor de 53 bytes a 9600 baudios eso bloquea 55,2 ms,
//  veintisiete veces el limite de 2 ms del Principio I.
// -----------------------------------------------------------------------------
#pragma once

#include <stdint.h>

namespace wiga {

class IByteStream {
public:
    virtual ~IByteStream() {}

    /// Abre el puerto y deja el transceptor en RECEPCION (regla DE-05).
    virtual void begin() = 0;

    /// Bytes disponibles para leer. No bloquea.
    virtual uint8_t available() const = 0;

    /// Un byte. Solo valido si available() > 0.
    virtual uint8_t read() = 0;

    /// Toma la linea: afirma DE y prepara el envio (regla DE-01).
    virtual void beginTransmission(uint8_t length) = 0;

    /// Entrega los bytes. NO espera a que salgan del cable, y NO bloquea aunque
    /// no quepan de una vez: los que sobren se entregan en vueltas posteriores
    /// a traves de service().
    virtual void write(const uint8_t* data, uint8_t length) = 0;

    /// Avanza la transmision pendiente. Se llama una vez por vuelta del lazo.
    ///
    /// Existe porque la respuesta mayor (69 B al leer los 32 registros) no cabe
    /// en el buffer de transmision del Mega (64 B). Sin este escalonamiento,
    /// write() bloquearia esperando hueco unos 5 ms, por encima del limite de
    /// 2 ms del Principio I.
    virtual void service() = 0;

    /// true mientras la trama siga saliendo. El llamador lo consulta en vueltas
    /// posteriores del lazo (regla DE-03).
    virtual bool transmitting() const = 0;

    /// Libera la linea. Solo tiene efecto si transmitting() es false, porque
    /// soltarla antes truncaria la propia respuesta (regla DE-02).
    virtual void endTransmission() = 0;
};

}  // namespace wiga
