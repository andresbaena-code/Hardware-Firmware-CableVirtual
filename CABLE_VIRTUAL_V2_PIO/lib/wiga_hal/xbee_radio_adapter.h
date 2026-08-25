// -----------------------------------------------------------------------------
//  Adaptador de IRadioPort sobre la libreria XBee en modo API escapado (AP=2).
//  Contrato: ports.md seccion 3 (RD-01..RD-07). Cubre: FR-014, FR-017, FR-018
//
//  MODO API: los nodos de campo operan con AP=2 (escapado). Ver research.md
//  R-001: la libreria escapa siempre en transmision y desescapa siempre en
//  recepcion. Cambiar a AP=1 rompe la instalacion de forma intermitente.
// -----------------------------------------------------------------------------
#pragma once

#include <XBee.h>

#include "i_radio_port.h"

namespace wiga {
namespace hal {

class XBeeRadioAdapter : public IRadioPort {
public:
    void begin() override;
    bool send(const proto::ValveCommand& command) override;
    uint8_t poll(proto::ValveReport* out, uint8_t maxReports) override;
    uint16_t framingErrors() const override { return framingErrors_; }

private:
    XBee          xbee_;
    ZBRxResponse  rx_;
    uint16_t      framingErrors_ = 0;
};

}  // namespace hal
}  // namespace wiga
