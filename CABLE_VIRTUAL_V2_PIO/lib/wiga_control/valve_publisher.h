// -----------------------------------------------------------------------------
//  Publicador de valvulas: lee el tablero y publica lo que pide.
//  Cubre: FR-109, FR-110, FR-111, FR-112, FR-114b, FR-115, FR-116, FR-119, FR-122
//
//  Sustituye a ValveSupervisor. Al no existir confirmacion del campo -el
//  protocolo de radio no acusa recibo- desaparecen la politica de reintentos,
//  el planificador de disparo y las fases de espera. Lo que queda es:
//
//      leer 24 interruptores -> antirrebote -> publicar 24 registros
//
//  FR-116 ES EL REQUISITO DOMINANTE: el registro de una valvula que riega no
//  puede presentar JAMAS un 0 transitorio. Un solo sondeo que lea 0 cierra la
//  valvula en campo. Por eso el registro solo se escribe cuando el antirrebote
//  confirma un nivel DISTINTO, y nunca se pone a cero por iniciativa propia.
//
//  Esta clase NO conoce ningun tipo concreto de hardware: solo puertos y el
//  banco de registros, que es codigo puro (Principios III y IV).
// -----------------------------------------------------------------------------
#pragma once

#include <stdint.h>

#include "debouncer.h"
#include "i_clock.h"
#include "i_digital_input_port.h"
#include "i_display_port.h"
#include "i_log_port.h"
#include "installation_map.h"
#include "register_file.h"
#include "timing_config.h"
#include "valve_types.h"

namespace wiga {

/// Simbolos del panel. Solo dos, porque sin confirmacion del campo no hay mas
/// que decir: representan lo que el TABLERO PIDE, no un estado verificado
/// (FR-114b, regla de honestidad de la interfaz del Principio II v2.0.0).
namespace symbol {
constexpr char kNotIrrigating = '0';
constexpr char kIrrigating    = '1';
}  // namespace symbol

class ValvePublisher {
public:
    ValvePublisher(IClock& clock,
                   IDigitalInputPort& inputs,
                   modbus::RegisterFile& registers,
                   IDisplayPort& display,
                   ILogPort& log);

    /// Siembra el banco desde las entradas fisicas. NO transmite ninguna orden:
    /// publicar no es emitir. Debe llamarse ANTES de atender el bus, o el primer
    /// sondeo devolveria 0 para una valvula que si riega (FR-116, FR-119).
    void begin();

    /// Una vuelta del lazo de control. No bloquea (FR-121).
    void tick();

    /// Refresca el bloque de diagnostico publicado (FR-113, FR-114).
    void publishDiagnostics(uint16_t secondsSincePoll,
                            uint16_t crcErrors,
                            uint16_t formatErrors,
                            uint16_t validRequests,
                            uint8_t  resetCause);

    /// Senala en el panel si el maestro lleva demasiado tiempo sin preguntar.
    void setBusHealthy(bool healthy);

    // --- Introspeccion para pruebas y diagnostico ---
    uint8_t valveCount() const { return config::kValveCount; }
    ValveState requested(uint8_t index) const;

private:
    void pollInputs(uint32_t nowMs);
    void refreshDisplay();
    static char symbolFor(ValveState state);

    IClock&               clock_;
    IDigitalInputPort&    inputs_;
    modbus::RegisterFile& registers_;
    IDisplayPort&         display_;
    ILogPort&             log_;

    Debouncer debouncers_[config::kValveCount];
    char      shownSymbol_[config::kValveCount];
    bool      busHealthy_ = true;
    bool      shownBusHealthy_ = true;
};

}  // namespace wiga
