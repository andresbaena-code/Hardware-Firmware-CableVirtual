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
    uint32_t nodeAddressLsb;  ///< HISTORICO: direccion XBee de la instalacion original
    uint8_t  valveId;         ///< HISTORICO: posicion dentro del nodo XBee original
    uint8_t  inputPin;        ///< pin del Mega conectado a la salida del tablero
};

// -----------------------------------------------------------------------------
//  BUS RS485: pines de ESTA placa
//
//  Verificado contra el hardware el 2026-08-27. El transceptor cuelga del
//  SEGUNDO puerto serie del ATmega2560, no del primero:
//
//      TXD2  = PH1 = pin 16
//      RXD2  = PH0 = pin 17
//      DE/RE = PH3 = pin  6
//
//  El firmware asumia Serial1 (pines 18/19). Con esa suposicion las tramas
//  salian por pines desconectados y el bus quedaba en silencio absoluto,
//  indistinguible de una averia de cableado.
//
//  SOBRE EL PIN DE DE/RE, Y POR QUE ES FACIL EQUIVOCARSE
//
//  DE y RE estan unidos en el pin 6. Sin nadie que lo gobierne el pin queda en
//  ALTO, y eso deja el transceptor en un estado enganoso:
//
//      DE  alto  -> el driver TRANSMITE, aunque nadie haya tocado el pin
//      RE  alto  -> el receptor esta DESACTIVADO
//
//  Consecuencia: cualquier prueba que SOLO TRANSMITA funciona con el pin 6 sin
//  configurar, e incluso funciona apuntando a un pin equivocado. Durante la
//  puesta en marcha eso llevo a fijar aqui el pin 2 -tomado de un sketch que
//  solo emitia texto- y el resultado fue una placa que transmitia
//  perfectamente y estaba SORDA: el saludo de banco salia al cable, pero
//  ninguna peticion Modbus se llegaba a leer jamas.
//
//  La prueba que si discrimina es la que RECIBE. Verificado el 2026-08-27 con
//  un sketch que lee la trama entrante y la devuelve: solo funciona con el 6.
// -----------------------------------------------------------------------------

/// Pin de control de direccion del MAX485 (DE y RE unidos). PH3 en esta placa.
/// Debe quedar en BAJO en reposo, o el receptor no escucha.
constexpr uint8_t kRs485DeRePin = 6;

/// Pines del puerto serie del bus. No se usan desde el firmware -de eso se
/// encarga HardwareSerial- pero se declaran para que MAP-V9 pueda comprobar que
/// ningun pin de entrada del tablero los pise.
constexpr uint8_t kRs485TxPin = 16;  // PH1 / TXD2
constexpr uint8_t kRs485RxPin = 17;  // PH0 / RXD2

// -----------------------------------------------------------------------------
//  CABLEADO DEL TABLERO  (FR-112)
//
//  Verificado en banco el 2026-08-27 sobre la placa real: al activar una entrada
//  el contacto ENERGIZA el pin a 3,3 V, y en reposo la entrada queda sujeta por
//  una resistencia de pull-down externa. El tablero es por tanto ACTIVO EN ALTO.
//
//  Esto NO es la inversion por transistor del nodo remoto. Son dos cosas
//  distintas que la version anterior confundia en una sola funcion del dominio:
//
//    * Cableado del tablero  -> depende de como esta hecho ESTE panel. Vive aqui.
//    * Etapa de potencia del nodo -> depende del actuador en campo. Vive en
//      valve_types.h y no se toca desde aqui.
//
//  Si un panel futuro se cablea al reves -contacto a masa con pull-up-, basta
//  cambiar estas dos constantes: ningun otro archivo se entera.
// -----------------------------------------------------------------------------

/// Nivel en el pin cuando el operador PIDE riego para esa valvula.
constexpr SignalLevel kPanelActiveLevel = SignalLevel::High;

/// Nivel en reposo. Derivado del anterior, nunca escrito dos veces.
constexpr SignalLevel kPanelIdleLevel =
    (kPanelActiveLevel == SignalLevel::High) ? SignalLevel::Low : SignalLevel::High;

/// El tablero ya lleva pull-down externa. Activar ademas el pull-up interno del
/// AVR formaria un divisor de tension con ella: el pin se quedaria en una zona
/// intermedia que, segun el valor de la resistencia externa, puede llegar a
/// leerse como ALTO de forma permanente y dejar el tablero ilegible.
constexpr bool kPanelUsesInternalPullup = false;

/// Traduce el nivel crudo del pin del tablero a lo que el operador esta pidiendo.
/// UNICO punto donde se interpreta el cableado del panel.
constexpr ValveState panelRequest(SignalLevel raw) {
    return (raw == kPanelActiveLevel) ? ValveState::Open : ValveState::Closed;
}

// El nivel activo y el de reposo no pueden ser el mismo, o el tablero seria
// indistinguible de si mismo.
static_assert(kPanelActiveLevel != kPanelIdleLevel,
              "MAP-V10: el nivel activo del tablero no puede igualar al de reposo");
static_assert(panelRequest(kPanelActiveLevel) == ValveState::Open,
              "MAP-V11: el nivel activo debe traducirse a peticion de riego");
static_assert(panelRequest(kPanelIdleLevel) == ValveState::Closed,
              "MAP-V12: el reposo debe traducirse a NO regar");

// -----------------------------------------------------------------------------
//  NOTA SOBRE LOS CAMPOS HISTORICOS (FR-120)
//
//  nodeAddressLsb y valveId provienen de la instalacion XBee original y ya NO
//  participan en ningun direccionamiento. Se conservan para poder contrastar el
//  mapa contra el plano de la finca.
//
//  El direccionamiento vigente es Modbus: el controlador es un unico esclavo y
//  publica UN REGISTRO POR VALVULA. La direccion del registro COINCIDE con el
//  indice de sistema de la valvula (contracts/register-map.md seccion 2), asi
//  que se DERIVA con valveRegister() en vez de almacenarse: guardarla seria
//  duplicar el indice y permitir que ambos se desincronizasen (Principio V).
// -----------------------------------------------------------------------------

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
static_assert(kCount <= 255, "kCount overflow: demasiadas valvulas para uint8_t");

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

/// Tope historico del identificador dentro de un nodo XBee. Se conserva como
/// comprobacion de coherencia del mapa heredado, aunque el campo ya no
/// participe en ningun direccionamiento.
constexpr uint8_t kMaxHistoricValveId = 7;

constexpr bool allValveIdsInRange(const ValveMapping* map, uint8_t count) {
    for (uint8_t i = 0; i < count; ++i)
        if (map[i].valveId > kMaxHistoricValveId) return false;
    return true;
}

constexpr bool allAddressesAssigned(const ValveMapping* map, uint8_t count) {
    for (uint8_t i = 0; i < count; ++i)
        if (map[i].nodeAddressLsb == uint32_t{0}) return false;
    return true;
}

/// MAP-V9: ningun pin de entrada puede pisar los pines del bus RS485.
///
/// Si uno pisara DE/RE, leer un interruptor conmutaria el bus a transmision y el
/// controlador quedaria mudo de forma intermitente. Si pisara TXD2 o RXD2, el
/// pinMode del tablero desconfiguraria el puerto serie y no habria bus en
/// absoluto. Ambos fallos se manifiestan como silencio, que es justo lo mas
/// dificil de diagnosticar en campo.
constexpr bool anyPinCollidesWithBus(const ValveMapping* map, uint8_t count) {
    for (uint8_t i = 0; i < count; ++i) {
        const uint8_t p = map[i].inputPin;
        if (p == kRs485DeRePin || p == kRs485TxPin || p == kRs485RxPin) return true;
    }
    return false;
}

}  // namespace detail

/// Numero de valvulas del sistema. DERIVADO del mapa (MAP-01, FR-024).
constexpr uint8_t kValveCount = detail::kCount;

/// Acceso a una fila del mapa. El llamador garantiza index < kValveCount.
const ValveMapping& valveMapping(uint8_t index);

/// Busca el indice de sistema de una valvula por (direccion, id).
/// Devuelve kNoValve si no esta configurada. Regla RX-05.
uint8_t findValveIndex(uint32_t nodeAddressLsb, uint8_t valveId);

/// Direccion del registro Modbus de una valvula (FR-119).
/// DERIVADA del indice, no almacenada: ver la nota sobre campos historicos.
constexpr uint16_t valveRegister(uint8_t index) {
    return static_cast<uint16_t>(index);
}

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
static_assert(!detail::anyPinCollidesWithBus(detail::kMap, detail::kCount),
              "MAP-V9: un pin de entrada pisa el bus RS485 (DE/RE, TXD2 o RXD2)");

}  // namespace config
}  // namespace wiga
