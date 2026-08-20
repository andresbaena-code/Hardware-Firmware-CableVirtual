// Identificacion del firmware. Cubre: FR-026
#pragma once

namespace wiga {
namespace hal {

constexpr char kFirmwareName[]    = "WIGA Cable Virtual";
constexpr char kFirmwareVersion[] = "2.0.0";
constexpr char kBuildDate[]       = __DATE__ " " __TIME__;

}  // namespace hal
}  // namespace wiga
