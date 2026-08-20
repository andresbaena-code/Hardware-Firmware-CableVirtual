// Puerto de diagnostico. Contrato: contracts/ports.md seccion 6 (LG-01..LG-04).
// Cubre: FR-026, FR-027, FR-028, FR-029
#pragma once

#include <stdint.h>

#include "valve_types.h"

namespace wiga {

enum class LogLevel : uint8_t { Error = 1, Warn = 2, Info = 3, Debug = 4 };

/// Catalogo de eventos. Ver contracts/ports.md seccion 6.
namespace logtag {
constexpr char kBoot[]    = "BOOT";  ///< causa del ultimo reinicio
constexpr char kVersion[] = "VER";   ///< version de firmware
constexpr char kTx[]      = "TX";    ///< orden transmitida
constexpr char kAck[]     = "ACK";   ///< confirmacion valida recibida
constexpr char kTimeout[] = "TMO";   ///< vencio la espera sin confirmar
constexpr char kBackoff[] = "BACK";  ///< entra en reposo prolongado
constexpr char kDrop[]    = "DROP";  ///< trama descartada
constexpr char kLcd[]     = "LCD";   ///< panel marcado como no sano
}  // namespace logtag

class ILogPort {
public:
    virtual ~ILogPort() {}
    virtual void begin() = 0;
    /// Evento con valvula asociada. No bloquea: si el buffer esta lleno, la
    /// traza se pierde (LG-01).
    virtual void event(LogLevel level, const char* tag,
                       ValveIndex index, int32_t value) = 0;
    /// Mensaje sin valvula asociada.
    virtual void message(LogLevel level, const char* text) = 0;
};

}  // namespace wiga
