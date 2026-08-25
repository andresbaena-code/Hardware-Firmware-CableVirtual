// Puerto de panel. Contrato: contracts/ports.md seccion 4 (DP-01..DP-05).
#pragma once

#include "valve_types.h"

namespace wiga {

class IDisplayPort {
public:
    virtual ~IDisplayPort() {}
    virtual void begin() = 0;

    /// Marca el simbolo deseado. NO escribe al hardware (DP-01).
    /// El simbolo representa el ESTADO FISICO de la valvula (DP-05, NV-02).
    virtual void setValveSymbol(ValveIndex index, char symbol) = 0;

    /// Marca la linea de estado. NO escribe al hardware (DP-01).
    virtual void setStatusLine(const char* text) = 0;

    /// Escribe como maximo kDisplayCellsPerTick celdas sucias y regresa.
    /// Unico metodo que toca el bus I2C en regimen (DP-02).
    virtual void tick() = 0;

    /// false si el panel dejo de responder. El control no depende de esto (DP-04).
    virtual bool healthy() const = 0;
};

}  // namespace wiga
