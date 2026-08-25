// -----------------------------------------------------------------------------
//  Tipos fundamentales del dominio.
//  Cubre: FR-021, FR-021b  (data-model.md 1.1-1.4, invariantes V-01..V-03)
//
//  ATENCION: la etapa de potencia de los nodos remotos opera con LOGICA INVERSA
//  POR TRANSISTOR. Nivel electrico alto (1) = valvula CERRADA. Esto NO es un
//  defecto: es una caracteristica del hardware instalado en campo, confirmada
//  por el responsable tecnico. Ver contracts/application-protocol.md seccion 4.
//  Si alguien "corrige" esta inversion, rompe todas las instalaciones.
// -----------------------------------------------------------------------------
#pragma once

#include <stdint.h>

namespace wiga {

/// Nivel electrico crudo: lo que hay en el pin o en el byte del protocolo.
enum class SignalLevel : uint8_t { Low = 0, High = 1 };

/// Estado fisico de la valvula: lo que realmente hace en campo.
enum class ValveState : uint8_t { Closed = 0, Open = 1 };

using ValveIndex = uint8_t;

/// Indice invalido / ausencia de valvula. Invariante V-03.
constexpr ValveIndex kNoValve = 0xFF;

// --- Unico punto de conversion entre nivel electrico y estado fisico ---------
// Invariantes V-01 (ida y vuelta) y V-02 (ninguna otra funcion convierte).

constexpr ValveState toValveState(SignalLevel level) {
    return (level == SignalLevel::High) ? ValveState::Closed : ValveState::Open;
}

constexpr SignalLevel toSignalLevel(ValveState state) {
    return (state == ValveState::Closed) ? SignalLevel::High : SignalLevel::Low;
}

// --- Conversion desde/hacia el byte crudo del protocolo ----------------------

constexpr uint8_t rawOf(SignalLevel level) {
    return static_cast<uint8_t>(level);
}

constexpr SignalLevel levelFromRaw(uint8_t raw) {
    return (raw != 0) ? SignalLevel::High : SignalLevel::Low;
}

}  // namespace wiga
