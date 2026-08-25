// -----------------------------------------------------------------------------
//  Protocolo de aplicacion con los nodos remotos.
//  Cubre: FR-014, FR-015, FR-016  (contracts/application-protocol.md)
//
//  Formato CONGELADO: los nodos ya estan desplegados en campo y no se modifican.
//  Cualquier cambio aqui exige intervencion de hardware en la finca.
// -----------------------------------------------------------------------------
#pragma once

#include <stdint.h>

#include "valve_types.h"

namespace wiga {
namespace proto {

constexpr uint8_t kHeader          = 0xAA;
constexpr uint8_t kOpcodeValve     = 0x14;
constexpr uint8_t kCommandLength   = 5;  ///< orden saliente: siempre 5 bytes (TX-01)
constexpr uint8_t kMinReportLength = 4;  ///< confirmacion: 4 o 5 bytes (RX-03)
constexpr uint8_t kMaxValveId      = 7;

/// Orden saliente hacia una valvula de un nodo.
struct ValveCommand {
    uint32_t    nodeAddressLsb;
    uint8_t     valveId;
    SignalLevel level;
};

/// Confirmacion entrante desde un nodo.
struct ValveReport {
    uint32_t    nodeAddressLsb;
    uint8_t     valveId;
    SignalLevel level;
};

/// Motivo por el que una trama recibida fue descartada. Se registra en la
/// consola de diagnostico (evento DROP, FR-027).
enum class DecodeError : uint8_t {
    None = 0,
    TooShort,      ///< RX-01
    BadHeader,     ///< RX-02
    BadOpcode,     ///< RX-02
    BadChecksum,   ///< RX-03
    BadLevel,      ///< RX-04
    BadValveId,    ///< RX-04
};

/// Digito verificador del payload: 0xFF - (suma de los cuatro primeros bytes).
constexpr uint8_t checksumOf(uint8_t header, uint8_t opcode,
                             uint8_t valveId, uint8_t level) {
    return static_cast<uint8_t>(
        0xFF - static_cast<uint8_t>(header + opcode + valveId + level));
}

/// Serializa una orden. Devuelve los bytes escritos, o 0 si no cabe o la orden
/// es invalida. Regla TX-01.
uint8_t encodeCommand(const ValveCommand& command, uint8_t* out, uint8_t capacity);

/// Valida y decodifica una confirmacion. La direccion de origen NO viaja en el
/// payload: la aporta la capa de transporte. Reglas RX-01..RX-04.
DecodeError decodeReport(const uint8_t* data, uint8_t length,
                         uint32_t sourceAddressLsb, ValveReport* out);

}  // namespace proto
}  // namespace wiga
