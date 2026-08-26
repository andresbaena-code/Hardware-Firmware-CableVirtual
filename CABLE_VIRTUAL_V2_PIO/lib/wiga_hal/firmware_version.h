// Identificacion del firmware. Cubre: FR-026
//
// RA-07 NOTA: estos constexpr viven en SRAM en AVR (~40 bytes). Podrian
// trasladarse a PROGMEM con pgm_read_byte en el call site, pero como solo se
// imprimen una vez al arrancar, el impacto es aceptable dentro del presupuesto
// de 75% de RAM.
#pragma once

namespace wiga {
namespace hal {

constexpr char kFirmwareName[]    = "WIGA Cable Virtual";
constexpr char kFirmwareVersion[] = "2.0.0";
constexpr char kBuildDate[]       = __DATE__ " " __TIME__;

}  // namespace hal
}  // namespace wiga
