// -----------------------------------------------------------------------------
//  Mapa de instalacion de la finca. UNICO archivo a editar para poner en marcha
//  una instalacion nueva.
//  Cubre: FR-023, FR-024, FR-025  (contracts/installation-map.md)
//         FR-201, FR-202, FR-203  (specs/003-retardo-apertura-cancelable)
//
//  Toda inconsistencia del mapa se detecta AL COMPILAR, no en campo. Ver los
//  static_assert MAP-V1..MAP-V17 al final del archivo.
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
    uint8_t  actuator;        ///< UC511 que la acciona, 1..kActuatorCount (FR-201)
    uint8_t  output;          ///< salida de ese UC511, 1 o 2 (FR-201)
};

/// Un UC511 de la finca. Su numero es su posicion en kActuators + 1.
struct ActuatorConfig {
    bool installed;  ///< el equipo existe en campo (FR-203)
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
//
//  EQUIPO Y SALIDA (spec 003, FR-201): el UC511 que acciona cada valvula y en
//  que salida. Sigue el reparto de docs/piloto-8-valvulas-d2d.md: pares
//  consecutivos, V01/V02 en el UC511 #01, V03/V04 en el #02, y asi. Las filas
//  8-23 se asignan al plan de 24 valvulas (#05..#12) aunque esos equipos aun no
//  existan: ver kActuators mas abajo.
//
//  Si el cableado real de un par no coincide, la separacion de comandos protege
//  al equipo equivocado. Verificar contra el campo en la puesta en marcha.
// -----------------------------------------------------------------------------
constexpr ValveMapping kMap[] = {
    //  idx    direccion LSB   id  pin  UC511 salida   pin original
    /*  0 */ { 0x42878FDBUL,   0,  54,   1,   1 },   // A0
    /*  1 */ { 0x42878FDBUL,   1,  55,   1,   2 },   // A1
    /*  2 */ { 0x42878FDBUL,   2,  56,   2,   1 },   // A2
    /*  3 */ { 0x42878FDBUL,   3,  57,   2,   2 },   // A3
    /*  4 */ { 0x41C3F584UL,   4,  58,   3,   1 },   // A4
    /*  5 */ { 0x41C3F584UL,   5,  59,   3,   2 },   // A5
    /*  6 */ { 0x41C3F549UL,   6,  60,   4,   1 },   // A6
    /*  7 */ { 0x41C3F549UL,   7,  61,   4,   2 },   // A7
    /*  8 */ { 0x41C3F568UL,   0,  62,   5,   1 },   // A8
    /*  9 */ { 0x41C3F568UL,   1,  27,   5,   2 },
    /* 10 */ { 0x41C3F57AUL,   2,   5,   6,   1 },
    /* 11 */ { 0x41C3F57AUL,   3,   8,   6,   2 },
    /* 12 */ { 0x41C3F544UL,   4,  46,   7,   1 },
    /* 13 */ { 0x41C3F544UL,   5,  44,   7,   2 },
    /* 14 */ { 0x41C3F58BUL,   6,  11,   8,   1 },
    /* 15 */ { 0x41C3F58BUL,   7,  12,   8,   2 },
    /* 16 */ { 0x41C3F658UL,   0,  10,   9,   1 },
    /* 17 */ { 0x41C3F658UL,   1,  45,   9,   2 },
    /* 18 */ { 0x41C3F595UL,   2,   9,  10,   1 },
    /* 19 */ { 0x41C3F595UL,   3,  63,  10,   2 },   // A9
    /* 20 */ { 0x41C3F595UL,   4,  66,  11,   1 },   // A12
    /* 21 */ { 0x41C3F595UL,   5,  68,  11,   2 },   // A14
    /* 22 */ { 0x41C3F595UL,   6,  67,  12,   1 },   // A13
    /* 23 */ { 0x41C3F54FUL,   7,  69,  12,   2 },   // A15
};

constexpr uint8_t kCount = sizeof(kMap) / sizeof(kMap[0]);
static_assert(kCount <= 255, "kCount overflow: demasiadas valvulas para uint8_t");

// -----------------------------------------------------------------------------
//  EQUIPOS UC511 DE LA FINCA  (spec 003, FR-203)
//
//  La posicion en el arreglo + 1 es el numero de equipo. Un equipo NO instalado
//  publica siempre 0 en todas sus valvulas, pida lo que pida el tablero: si al
//  crecer alguien olvida marcarlo como instalado, la valvula no abre y se nota
//  en la puesta en marcha. El olvido falla hacia cerrado, nunca hacia una
//  valvula sin separacion de comandos.
//
//  Piloto (2026-10): un UC100 con los UC511 #01..#04.
// -----------------------------------------------------------------------------
constexpr ActuatorConfig kActuators[] = {
    /* #01 */ { true  },
    /* #02 */ { true  },
    /* #03 */ { true  },
    /* #04 */ { true  },
    /* #05 */ { false },
    /* #06 */ { false },
    /* #07 */ { false },
    /* #08 */ { false },
    /* #09 */ { false },
    /* #10 */ { false },
    /* #11 */ { false },
    /* #12 */ { false },
};

constexpr uint8_t kActuatorTotal = sizeof(kActuators) / sizeof(kActuators[0]);

/// Salidas fisicas de un UC511.
constexpr uint8_t kOutputsPerActuator = 2;

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

// --- MAP-V13..MAP-V17: mapa de equipos (FR-202) ------------------------------

constexpr bool allActuatorsInRange(const ValveMapping* map, uint8_t count,
                                   uint8_t actuatorCount) {
    for (uint8_t i = 0; i < count; ++i)
        if (map[i].actuator < 1 || map[i].actuator > actuatorCount) return false;
    return true;
}

constexpr bool allOutputsValid(const ValveMapping* map, uint8_t count) {
    for (uint8_t i = 0; i < count; ++i)
        if (map[i].output < 1 || map[i].output > kOutputsPerActuator) return false;
    return true;
}

constexpr bool hasDuplicateActuatorOutput(const ValveMapping* map, uint8_t count) {
    for (uint8_t i = 0; i < count; ++i)
        for (uint8_t j = static_cast<uint8_t>(i + 1); j < count; ++j)
            if (map[i].actuator == map[j].actuator &&
                map[i].output == map[j].output) return true;
    return false;
}

constexpr uint8_t valvesOnActuator(const ValveMapping* map, uint8_t count,
                                   uint8_t actuator) {
    uint8_t n = 0;
    for (uint8_t i = 0; i < count; ++i)
        if (map[i].actuator == actuator) ++n;
    return n;
}

/// Ningun equipo con mas valvulas que salidas. Con MAP-V14 y MAP-V15 ya es
/// imposible, pero se comprueba aparte para que el mensaje diga lo que pasa.
constexpr bool noActuatorOverloaded(const ValveMapping* map, uint8_t count,
                                    uint8_t actuatorCount) {
    for (uint8_t a = 1; a <= actuatorCount; ++a)
        if (valvesOnActuator(map, count, a) > kOutputsPerActuator) return false;
    return true;
}

/// Un equipo declarado sin valvulas es casi siempre un error de transcripcion.
constexpr bool everyActuatorUsed(const ValveMapping* map, uint8_t count,
                                 uint8_t actuatorCount) {
    for (uint8_t a = 1; a <= actuatorCount; ++a)
        if (valvesOnActuator(map, count, a) == 0) return false;
    return true;
}

}  // namespace detail

/// Numero de valvulas del sistema. DERIVADO del mapa (MAP-01, FR-024).
constexpr uint8_t kValveCount = detail::kCount;

/// Numero de UC511 de la finca. DERIVADO de kActuators (Principio V).
constexpr uint8_t kActuatorCount = detail::kActuatorTotal;

/// UC511 que acciona una valvula (1..kActuatorCount), o 0 si el indice no existe.
constexpr uint8_t actuatorOf(uint8_t index) {
    return (index < kValveCount) ? detail::kMap[index].actuator : uint8_t{0};
}

/// Salida del UC511 (1 o 2), o 0 si el indice no existe.
constexpr uint8_t outputOf(uint8_t index) {
    return (index < kValveCount) ? detail::kMap[index].output : uint8_t{0};
}

/// true si el equipo existe en campo. Un numero fuera de rango nunca lo esta.
constexpr bool actuatorInstalled(uint8_t actuator) {
    return actuator >= 1 && actuator <= kActuatorCount &&
           detail::kActuators[actuator - 1].installed;
}

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

// --- MAP-V13..MAP-V17: el mapa de equipos invalido no compila (FR-202) -------
static_assert(kActuatorCount > 0,
              "MAP-V13: no hay ningun UC511 declarado en kActuators");
static_assert(detail::allActuatorsInRange(detail::kMap, detail::kCount, kActuatorCount),
              "MAP-V13: una valvula apunta a un UC511 que no existe en kActuators");
static_assert(detail::allOutputsValid(detail::kMap, detail::kCount),
              "MAP-V14: salida de UC511 invalida (solo 1 o 2)");
static_assert(!detail::hasDuplicateActuatorOutput(detail::kMap, detail::kCount),
              "MAP-V15: dos valvulas en la misma salida del mismo UC511");
static_assert(detail::noActuatorOverloaded(detail::kMap, detail::kCount, kActuatorCount),
              "MAP-V16: un UC511 tiene mas de 2 valvulas");
static_assert(detail::everyActuatorUsed(detail::kMap, detail::kCount, kActuatorCount),
              "MAP-V17: un UC511 declarado no tiene ninguna valvula");

}  // namespace config
}  // namespace wiga
