// Cubre: FR-023, FR-024  (contracts/installation-map.md)
#include "installation_map.h"

namespace wiga {
namespace config {

const ValveMapping& valveMapping(uint8_t index) {
    return detail::kMap[index];
}

uint8_t findValveIndex(uint32_t nodeAddressLsb, uint8_t valveId) {
    for (uint8_t i = 0; i < kValveCount; ++i) {
        if (detail::kMap[i].nodeAddressLsb == nodeAddressLsb &&
            detail::kMap[i].valveId == valveId) {
            return i;
        }
    }
    return kNoValve;  // RX-05: valvula no configurada
}

}  // namespace config
}  // namespace wiga
