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

// --- Conversion del NODO REMOTO: nivel electrico <-> estado fisico -----------
//
//  ATENCION: esto describe la ETAPA DE POTENCIA DEL ACTUADOR EN CAMPO, no el
//  cableado del tablero. Son dos inversiones independientes y confundirlas ya
//  costo un defecto:
//
//    * Tablero  -> config::panelRequest(), en installation_map.h. Depende de
//      como este cableado ESTE panel. Verificado en banco: activo en ALTO.
//    * Nodo remoto -> estas funciones. Depende del actuador instalado en campo.
//
//  Con la arquitectura de lazo abierto el firmware ya no emite niveles
//  electricos al nodo -publica estado fisico en registros-, asi que estas
//  funciones ya no intervienen en el camino de produccion. Se conservan porque
//  documentan el contrato del hardware de campo y porque el dia que exista una
//  via de retorno volveran a hacer falta.
//
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

// --- Conversion hacia el registro Modbus publicado ---------------------------
//  Cubre: FR-110, FR-111
//
//  El valor publicado representa el ESTADO FISICO, no el nivel electrico:
//  1 = la valvula debe regar, 0 = no debe regar. La inversion por transistor ya
//  quedo resuelta en toValveState() al leer el pin del tablero.
//
//  Vive aqui, en el dominio, porque es el UNICO punto de conversion permitido
//  (invariantes V-01 y V-02). Ningun adaptador ni la capa de protocolo pueden
//  construir este valor a mano: hacerlo en dos sentidos distintos fue el defecto
//  mas grave de la implementacion anterior.

constexpr uint16_t registerValueOf(ValveState state) {
    return (state == ValveState::Open) ? uint16_t{1} : uint16_t{0};
}

constexpr ValveState valveStateFromRegister(uint16_t value) {
    return (value != 0) ? ValveState::Open : ValveState::Closed;
}

}  // namespace wiga
