// Puerto de entradas del tablero. Contrato: contracts/ports.md seccion 2.
#pragma once

#include "valve_types.h"

namespace wiga {

class IDigitalInputPort {
public:
    virtual ~IDigitalInputPort() {}
    /// Configura los pines con pull-up interno. Se llama una vez (IN-01).
    virtual void configure() = 0;
    /// Nivel electrico CRUDO. La conversion a estado fisico y el antirebote
    /// ocurren en la capa de control, no aqui (IN-02, FR-021b).
    virtual SignalLevel read(ValveIndex index) const = 0;
};

}  // namespace wiga
