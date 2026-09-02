// -----------------------------------------------------------------------------
//  Verificador de redundancia CRC16 del estandar Modbus.
//  Cubre: FR-104
//
//  Polinomio 0xA001 (reflejado), valor inicial 0xFFFF. En la trama el CRC viaja
//  con el byte BAJO primero, al reves que el resto de campos de 16 bits del
//  protocolo, que van en orden de red. Es una peculiaridad del estandar y una
//  fuente clasica de errores.
//
//  Esta cabecera NO incluye <Arduino.h>. Ver lib/wiga_modbus/README.md.
// -----------------------------------------------------------------------------
#pragma once

#include <stdint.h>

namespace wiga {
namespace modbus {

/// Calcula el verificador sobre `length` bytes de `data`.
///
/// Propiedad util: calcularlo sobre una trama que YA lleva su verificador al
/// final da cero. Eso permite validar sin separar el CRC del cuerpo.
///
/// Con `data` nulo o `length` cero devuelve el valor inicial 0xFFFF.
uint16_t crc16(const uint8_t* data, uint8_t length);

}  // namespace modbus
}  // namespace wiga
