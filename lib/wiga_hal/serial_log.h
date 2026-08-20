// -----------------------------------------------------------------------------
//  Adaptador de ILogPort sobre el puerto serie de depuracion.
//  Contrato: ports.md seccion 6 (LG-01..LG-04). Cubre: FR-027, FR-028, FR-029
//
//  Compilar con -DWIGA_LOG_LEVEL=0 elimina las trazas sin alterar ninguna otra
//  ruta de ejecucion.
// -----------------------------------------------------------------------------
#pragma once

#include "i_log_port.h"

#ifndef WIGA_LOG_LEVEL
#define WIGA_LOG_LEVEL 3
#endif

namespace wiga {
namespace hal {

class SerialLog : public ILogPort {
public:
    void begin() override;
    void event(LogLevel level, const char* tag,
               ValveIndex index, int32_t value) override;
    void message(LogLevel level, const char* text) override;

private:
    static bool enabled(LogLevel level) {
        return static_cast<uint8_t>(level) <= WIGA_LOG_LEVEL;
    }
};

}  // namespace hal
}  // namespace wiga
