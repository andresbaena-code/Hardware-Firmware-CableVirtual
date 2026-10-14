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
constexpr char kTx[]      = "TX";    ///< el nivel pedido por el tablero cambio
constexpr char kDrop[]    = "DROP";  ///< trama descartada / salud del panel
constexpr char kLcd[]     = "LCD";   ///< panel marcado como no sano

// Separacion de comandos por UC511 (spec 003). Cubre: FR-213, FR-216
constexpr char kWait[]         = "WAIT";    ///< cambio retenido hasta el turno de su UC511
constexpr char kCancel[]       = "CANC";    ///< cambio pendiente revertido por el tablero
constexpr char kNotInstalled[] = "NOINST";  ///< pedido sobre una valvula de UC511 no instalado
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
