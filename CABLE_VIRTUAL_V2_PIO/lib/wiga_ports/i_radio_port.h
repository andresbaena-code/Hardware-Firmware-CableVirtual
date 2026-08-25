// Puerto de radio. Contrato: contracts/ports.md seccion 3 (RD-01..RD-07).
#pragma once

#include <stdint.h>

#include "app_protocol.h"

namespace wiga {

class IRadioPort {
public:
    virtual ~IRadioPort() {}
    virtual void begin() = 0;

    /// Serializa y transmite una orden. NO DEBE vaciar el buffer de recepcion
    /// (RD-02, FR-018). Devuelve false si no pudo entregarla al radio.
    virtual bool send(const proto::ValveCommand& command) = 0;

    /// Drena el radio y escribe hasta maxReports confirmaciones VALIDAS en out.
    /// Debe leer tramas repetidamente hasta agotar el buffer o llegar al limite
    /// (RD-04, research.md R-002). Devuelve cuantas escribio.
    virtual uint8_t poll(proto::ValveReport* out, uint8_t maxReports) = 0;

    /// Tramas descartadas por corrupcion o desincronizacion (RD-07, FR-028).
    virtual uint16_t framingErrors() const = 0;
};

}  // namespace wiga
