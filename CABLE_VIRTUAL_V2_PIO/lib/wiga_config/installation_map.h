// -----------------------------------------------------------------------------
//  Mapa de instalacion de la finca. UNICO archivo a editar para poner en marcha
//  una instalacion nueva.
//  Cubre: FR-023, FR-024, FR-025  (contracts/installation-map.md)
//
//  Toda inconsistencia del mapa se detecta AL COMPILAR, no en campo. Ver los
//  static_assert MAP-V1..MAP-V6 al final del archivo.
// -----------------------------------------------------------------------------
#pragma once

#include <stdint.h>

#include "valve_types.h"

namespace wiga {
namespace config {

struct ValveMapping {
    uint32_t nodeAddressLsb;  ///< 32 bits bajos de la direccion de 64 bits del nodo
    uint8_t  valveId;         ///< 0..7, posicion de la valvula dentro del nodo
    uint8_t  inputPin;        ///< pin del Mega conectado a la salida del tablero
};

namespace detail {

// -----------------------------------------------------------------------------
//  MAPA DE LA INSTALACION
//  El indice de cada fila es el numero de valvula del sistema y determina su
//  posicion en el panel LCD (filas de ocho, en orden).
//  Transcrito de CABLE_VIRTUAL___LATCH_V1.ino sin modificar.
//
//  Los pines analogicos se escriben por su numero de pin digital equivalente en
//  el Mega 2560 (A0 = 54 ... A15 = 69) para que el mapa sea identico en la placa
//  y en las pruebas de host, donde los nombres A0..A15 no existen.
//
//  PENDIENTE DE VERIFICACION CONTRA EL PLANO DE LA FINCA (tarea T055):
//  las filas 18-22 comparten el nodo 0x41C3F595 con ids 2..6, patron distinto
//  al del resto de nodos.
// -----------------------------------------------------------------------------
constexpr ValveMapping kMap[] = {
    //  idx    direccion LSB   id  pin        pin original
    /*  0 */ { 0x42878FDBUL,   0,  54 },   // A0
    /*  1 */ { 0x42878FDBUL,   1,  55 },   // A1
    /*  2 */ { 0x42878FDBUL,   2,  56 },   // A2
    /*  3 */ { 0x42878FDBUL,   3,  57 },   // A3
    /*  4 */ { 0x41C3F584UL,   4,  58 },   // A4
    /*  5 */ { 0x41C3F584UL,   5,  59 },   // A5
    /*  6 */ { 0x41C3F549UL,   6,  60 },   // A6
    /*  7 */ { 0x41C3F549UL,   7,  61 },   // A7
    /*  8 */ { 0x41C3F568UL,   0,  62 },   // A8
    /*  9 */ { 0x41C3F568UL,   1,  27 },
    /* 10 */ { 0x41C3F57AUL,   2,   5 },
    /* 11 */ { 0x41C3F57AUL,   3,   8 },
    /* 12 */ { 0x41C3F544UL,   4,  46 },
    /* 13 */ { 0x41C3F544UL,   5,  44 },
    /* 14 */ { 0x41C3F58BUL,   6,  11 },
    /* 15 */ { 0x41C3F58BUL,   7,  12 },
    /* 16 */ { 0x41C3F658UL,   0,  10 },
    /* 17 */ { 0x41C3F658UL,   1,  45 },
    /* 18 */ { 0x41C3F595UL,   2,   9 },
    /* 19 */ { 0x41C3F595UL,   3,  63 },   // A9
    /* 20 */ { 0x41C3F595UL,   4,  66 },   // A12
    /* 21 */ { 0x41C3F595UL,   5,  68 },   // A14
    /* 22 */ { 0x41C3F595UL,   6,  67 },   // A13
    /* 23 */ { 0x41C3F54FUL,   7,  69 },   // A15
};

constexpr uint8_t kCount = sizeof(kMap) / sizeof(kMap[0]);

// --- Funciones de validacion en tiempo de compilacion ------------------------

constexpr bool hasDuplicatePin(const ValveMapping* map, uint8_t count) {
    for (uint8_t i = 0; i < count; ++i)
        for (uint8_t j = static_cast<uint8_t>(i + 1); j < count; ++j)
            if (map[i].inputPin == map[j].inputPin) return true;
    return false;
}

constexpr bool hasDuplicateTarget(const ValveMapping* map, uint8_t count) {
    for (uint8_t i = 0; i < count; ++i)
        for (uint8_t j = static_cast<uint8_t>(i + 1); j < count; ++j)
            if (map[i].nodeAddressLsb == map[j].nodeAddressLsb &&
                map[i].valveId == map[j].valveId) return true;
    return false;
}

constexpr bool allValveIdsInRange(const ValveMapping* map, uint8_t count) {
    for (uint8_t i = 0; i < count; ++i)
        if (map[i].valveId > 7) return false;
    return true;
}

constexpr bool allAddressesAssigned(const ValveMapping* map, uint8_t count) {
    for (uint8_t i = 0; i < count; ++i)
        if (map[i].nodeAddressLsb == 0UL) return false;
    return true;
}

}  // namespace detail

/// Numero de valvulas del sistema. DERIVADO del mapa (MAP-01, FR-024).
constexpr uint8_t kValveCount = detail::kCount;

/// Acceso a una fila del mapa. El llamador garantiza index < kValveCount.
const ValveMapping& valveMapping(uint8_t index);

/// Busca el indice de sistema de una valvula por (direccion, id).
/// Devuelve kNoValve si no esta configurada. Regla RX-05.
uint8_t findValveIndex(uint32_t nodeAddressLsb, uint8_t valveId);

// --- MAP-V1..MAP-V6: el mapa invalido no compila -----------------------------
static_assert(detail::kCount > 0,
              "MAP-V1: mapa de instalacion vacio");
static_assert(detail::kCount <= 96,
              "MAP-V2: demasiadas valvulas para el panel");
static_assert(!detail::hasDuplicatePin(detail::kMap, detail::kCount),
              "MAP-V3: pin de entrada duplicado en el mapa");
static_assert(!detail::hasDuplicateTarget(detail::kMap, detail::kCount),
              "MAP-V4: par direccion/id duplicado en el mapa");
static_assert(detail::allValveIdsInRange(detail::kMap, detail::kCount),
              "MAP-V5: valveId fuera de rango (0..7)");
static_assert(detail::allAddressesAssigned(detail::kMap, detail::kCount),
              "MAP-V6: direccion de nodo sin asignar");

}  // namespace config
}  // namespace wiga
