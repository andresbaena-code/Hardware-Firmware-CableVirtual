// Cubre: FR-014, FR-016, FR-017, FR-018
#include "xbee_radio_adapter.h"

#include <Arduino.h>

#include "timing_config.h"

namespace wiga {
namespace hal {

void XBeeRadioAdapter::begin() {
    Serial1.begin(config::kRadioBaudRate);
    xbee_.setSerial(Serial1);
}

bool XBeeRadioAdapter::send(const proto::ValveCommand& command) {
    // RD-02 / FR-018: NO se vacia el buffer de recepcion. El firmware base hacia
    // `while (Serial1.available()) Serial1.read();` aqui, descartando
    // confirmaciones ya recibidas y provocando reintentos -y disparos de
    // solenoide- sobre valvulas que si habian obedecido.

    uint8_t payload[proto::kCommandLength];
    const uint8_t length = proto::encodeCommand(command, payload, sizeof(payload));
    if (length == 0) return false;

    // RD-03: la trama completa con escapes ronda los 24 bytes y cabe en el
    // buffer de transmision de 64 bytes del HardwareSerial, asi que write() no
    // llega a bloquear.
    XBeeAddress64 destination(config::kXBeeAddressMsb, command.nodeAddressLsb);
    ZBTxRequest request(destination, payload, length);
    request.setFrameId(0);  // heredado: desactiva el acuse del propio radio
    xbee_.send(request);
    return true;
}

uint8_t XBeeRadioAdapter::poll(proto::ValveReport* out, uint8_t maxReports) {
    uint8_t written = 0;

    // RD-04: readPacket() devuelve UNA sola trama por llamada y deja el resto
    // en el buffer (research.md R-002). Hay que drenarlo en bucle, acotado para
    // no violar el presupuesto de ciclo del Principio I.
    while (written < maxReports) {
        xbee_.readPacket();

        if (xbee_.getResponse().isError()) {
            if (framingErrors_ < 0xFFFF) ++framingErrors_;  // RD-07
            continue;
        }
        if (!xbee_.getResponse().isAvailable()) {
            break;  // buffer agotado
        }
        if (xbee_.getResponse().getApiId() != ZB_RX_RESPONSE) {
            continue;  // otra trama de la capa XBee; no es asunto nuestro
        }

        xbee_.getResponse().getZBRxResponse(rx_);

        // RD-05: reglas RX-01..RX-04 del contrato de protocolo.
        const uint32_t source = rx_.getRemoteAddress64().getLsb();
        const proto::DecodeError error =
            proto::decodeReport(rx_.getData(), rx_.getDataLength(), source, &out[written]);

        if (error == proto::DecodeError::None) {
            ++written;  // RD-06: solo se entregan tramas validadas por completo
        } else if (framingErrors_ < 0xFFFF) {
            ++framingErrors_;
        }
    }
    return written;
}

}  // namespace hal
}  // namespace wiga
