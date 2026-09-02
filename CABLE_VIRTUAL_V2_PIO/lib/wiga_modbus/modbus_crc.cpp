// Cubre: FR-104
#include "modbus_crc.h"

namespace wiga {
namespace modbus {

uint16_t crc16(const uint8_t* data, uint8_t length) {
    uint16_t crc = 0xFFFF;
    if (data == nullptr) {
        return crc;
    }
    for (uint8_t i = 0; i < length; ++i) {
        crc ^= static_cast<uint16_t>(data[i]);
        for (uint8_t bit = 0; bit < 8; ++bit) {
            if (crc & 0x0001u) {
                crc = static_cast<uint16_t>((crc >> 1) ^ 0xA001u);
            } else {
                crc = static_cast<uint16_t>(crc >> 1);
            }
        }
    }
    return crc;
}

}  // namespace modbus
}  // namespace wiga
