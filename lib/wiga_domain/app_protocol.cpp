// Cubre: FR-014, FR-015, FR-016  (contracts/application-protocol.md)
#include "app_protocol.h"

namespace wiga {
namespace proto {

uint8_t encodeCommand(const ValveCommand& command, uint8_t* out, uint8_t capacity) {
    if (out == nullptr || capacity < kCommandLength) return 0;
    if (command.valveId > kMaxValveId) return 0;

    const uint8_t level = rawOf(command.level);
    out[0] = kHeader;
    out[1] = kOpcodeValve;
    out[2] = command.valveId;
    out[3] = level;
    out[4] = checksumOf(kHeader, kOpcodeValve, command.valveId, level);
    return kCommandLength;
}

DecodeError decodeReport(const uint8_t* data, uint8_t length,
                         uint32_t sourceAddressLsb, ValveReport* out) {
    // RX-01: nunca se indexa mas alla de la longitud reportada.
    if (data == nullptr || out == nullptr) return DecodeError::TooShort;
    if (length < kMinReportLength) return DecodeError::TooShort;

    // RX-02
    if (data[0] != kHeader) return DecodeError::BadHeader;
    if (data[1] != kOpcodeValve) return DecodeError::BadOpcode;

    // RX-03: el verificador se valida solo cuando esta presente. Se aceptan
    // tramas de 4 bytes por compatibilidad con nodos de firmware antiguo.
    if (length >= kCommandLength) {
        const uint8_t expected = checksumOf(data[0], data[1], data[2], data[3]);
        if (data[4] != expected) return DecodeError::BadChecksum;
    }

    // RX-04
    if (data[3] > 1) return DecodeError::BadLevel;
    if (data[2] > kMaxValveId) return DecodeError::BadValveId;

    out->nodeAddressLsb = sourceAddressLsb;
    out->valveId        = data[2];
    out->level          = levelFromRaw(data[3]);
    return DecodeError::None;
}

}  // namespace proto
}  // namespace wiga
