// Puerto de tiempo. Contrato: contracts/ports.md seccion 1 (CLK-01..CLK-03).
#pragma once

#include <stdint.h>

namespace wiga {

class IClock {
public:
    virtual ~IClock() {}
    /// Milisegundos desde el arranque. Desborda cada ~49,7 dias: los consumidores
    /// DEBEN comparar con elapsed(), nunca con marcas absolutas (CLK-02).
    virtual uint32_t nowMs() const = 0;
};

}  // namespace wiga
