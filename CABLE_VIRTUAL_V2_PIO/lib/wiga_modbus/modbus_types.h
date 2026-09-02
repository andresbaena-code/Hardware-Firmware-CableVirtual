// -----------------------------------------------------------------------------
//  Tipos y constantes del protocolo Modbus RTU.
//  Cubre: FR-101, FR-106  (contracts/modbus-slave.md)
//
//  El controlador es ESCLAVO: nunca inicia una transaccion, solo responde a las
//  peticiones dirigidas a su propio identificador. Por eso aqui solo aparecen
//  funciones de LECTURA: toda escritura se rechaza con excepcion, porque el
//  tablero es la unica fuente de verdad del estado pedido.
//
//  Esta cabecera NO incluye <Arduino.h>. Ver lib/wiga_modbus/README.md.
// -----------------------------------------------------------------------------
#pragma once

#include <stdint.h>

namespace wiga {
namespace modbus {

/// Codigos de funcion soportados. Las cuatro exponen el mismo estado: FC01 y
/// FC02 como bits, FC03 y FC04 como registros de 16 bits. Soportar las cuatro
/// permite que el instalador elija cualquier tipo de canal del UC100 sin
/// recompilar el firmware (research.md R-104).
namespace fc {
constexpr uint8_t kReadCoils            = 0x01;
constexpr uint8_t kReadDiscreteInputs   = 0x02;
constexpr uint8_t kReadHoldingRegisters = 0x03;
constexpr uint8_t kReadInputRegisters   = 0x04;
}  // namespace fc

/// Bit que marca una respuesta de excepcion: el codigo de funcion vuelve con el
/// bit alto puesto (0x03 -> 0x83).
constexpr uint8_t kExceptionFlag = 0x80;

/// Codigos de excepcion del estandar.
namespace exc {
constexpr uint8_t kIllegalFunction    = 0x01;  ///< funcion no soportada, toda escritura
constexpr uint8_t kIllegalDataAddress = 0x02;  ///< direccion o rango fuera del mapa
constexpr uint8_t kIllegalDataValue   = 0x03;  ///< cantidad cero o fuera del limite
}  // namespace exc

// --- Limites del estandar ----------------------------------------------------
constexpr uint16_t kMaxReadRegisters = 125;   ///< maximo de FC03/FC04
constexpr uint16_t kMaxReadBits      = 2000;  ///< maximo de FC01/FC02

// --- Geometria de las tramas -------------------------------------------------
/// Toda peticion de lectura mide lo mismo: id, funcion, direccion (2), cantidad
/// (2) y verificador (2). Conocer la longitud de antemano permite delimitar por
/// longitud y dejar el silencio solo como resincronizador (research.md R-102).
constexpr uint8_t kReadRequestLength = 8;

constexpr uint8_t kCrcLength         = 2;
constexpr uint8_t kExceptionLength   = 5;  ///< id, funcion|0x80, codigo, verificador (2)

// --- Rango valido de identificador de esclavo --------------------------------
constexpr uint8_t kMinSlaveId = 1;
constexpr uint8_t kMaxSlaveId = 247;

/// Resultado del intento de atender una peticion. Distingue las tres salidas que
/// el contrato exige: responder, responder excepcion, o callar.
enum class Outcome : uint8_t {
    Silence,    ///< no es para nosotros, o llego corrupta: MB-01, MB-02, MB-03
    Response,   ///< peticion valida atendida
    Exception,  ///< peticion valida en forma, pero no atendible
};

}  // namespace modbus
}  // namespace wiga
