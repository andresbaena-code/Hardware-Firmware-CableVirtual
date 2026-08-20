// Puerto de watchdog. Contrato: contracts/ports.md seccion 5 (WD-01..WD-04).
#pragma once

namespace wiga {

class IWatchdogPort {
public:
    virtual ~IWatchdogPort() {}
    virtual void disable() = 0;
    virtual void enable() = 0;
    /// Se invoca en UN UNICO punto del lazo principal. Prohibido llamarlo desde
    /// una espera, un bucle interno o un manejador de error (WD-03, FR-012).
    virtual void feed() = 0;
};

}  // namespace wiga
