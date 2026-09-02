// -----------------------------------------------------------------------------
//  Banco de registros publicado al maestro del bus.
//  Cubre: FR-107, FR-109, FR-110, FR-113  (data-model.md 2.1)
//
//  Es la UNICA fuente de verdad del estado pedido: lo escribe la capa de control
//  y lo leen el protocolo y el panel. Mantener una copia paralela seria duplicar
//  la fuente de verdad, que es justo lo que el Principio V prohibe.
//
//  Todo acceso esta acotado. Es la leccion directa del defecto verificado en la
//  implementacion anterior, donde un campo de longitud de 251 hacia que la
//  comprobacion diera cero y se leyeran 254 bytes de un buffer de 32.
//
//  Esta cabecera NO incluye <Arduino.h>. Ver lib/wiga_modbus/README.md.
// -----------------------------------------------------------------------------
#pragma once

#include <stdint.h>

#include "timing_config.h"

namespace wiga {
namespace modbus {

class RegisterFile {
public:
    RegisterFile();

    /// Escribe un registro. Indice fuera de rango: no hace nada. Nunca desborda.
    void set(uint16_t index, uint16_t value);

    /// Lee un registro. Indice fuera de rango: devuelve 0. Nunca desborda.
    uint16_t get(uint16_t index) const;

    /// Vista de bit del mismo estado, para FC01 y FC02. Un registro distinto de
    /// cero es un bit a uno. La conversion vive solo aqui.
    ///
    /// NO se puede llamar bit(): Arduino.h define una macro con ese nombre
    /// (#define bit(b) (1UL << (b))) y romperia la compilacion para la placa.
    bool coilBit(uint16_t index) const;

    /// Numero de registros. DERIVADO de la configuracion (Principio V).
    static constexpr uint16_t count() { return config::kRegisterCount; }

    /// true si el rango [start, start+quantity) cabe entero en el banco.
    /// La aritmetica es de 32 bits a proposito: sumar en 16 bits permitiria que
    /// un rango enorme diera la vuelta y pareciera valido.
    static bool rangeFits(uint16_t start, uint16_t quantity);

private:
    uint16_t values_[config::kRegisterCount];
};

}  // namespace modbus
}  // namespace wiga
