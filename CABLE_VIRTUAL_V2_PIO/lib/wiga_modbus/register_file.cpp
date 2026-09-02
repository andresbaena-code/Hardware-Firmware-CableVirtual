// Cubre: FR-107, FR-109, FR-110, FR-113
#include "register_file.h"

namespace wiga {
namespace modbus {

RegisterFile::RegisterFile() {
    for (uint16_t i = 0; i < config::kRegisterCount; ++i) {
        values_[i] = 0;
    }
}

void RegisterFile::set(uint16_t index, uint16_t value) {
    if (index >= config::kRegisterCount) {
        return;  // acotado: nunca desborda
    }
    values_[index] = value;
}

uint16_t RegisterFile::get(uint16_t index) const {
    if (index >= config::kRegisterCount) {
        return 0;  // acotado: nunca desborda
    }
    return values_[index];
}

bool RegisterFile::coilBit(uint16_t index) const {
    return get(index) != 0;
}

bool RegisterFile::rangeFits(uint16_t start, uint16_t quantity) {
    if (quantity == 0) {
        return false;
    }
    // 32 bits: en 16 la suma podria dar la vuelta y un rango invalido pasaria.
    const uint32_t last = static_cast<uint32_t>(start) +
                          static_cast<uint32_t>(quantity);
    return last <= static_cast<uint32_t>(config::kRegisterCount);
}

}  // namespace modbus
}  // namespace wiga
