// Cubre: FR-109, FR-110, FR-111, FR-112, FR-114b, FR-115, FR-116, FR-119, FR-122
#include "valve_publisher.h"

namespace wiga {

ValvePublisher::ValvePublisher(IClock& clock,
                               IDigitalInputPort& inputs,
                               modbus::RegisterFile& registers,
                               IDisplayPort& display,
                               ILogPort& log)
    : clock_(clock),
      inputs_(inputs),
      registers_(registers),
      display_(display),
      log_(log) {
    for (uint8_t i = 0; i < config::kValveCount; ++i) {
        shownSymbol_[i] = 0;
    }
}

// --- Siembra sin emitir ordenes (FR-116, FR-119) -----------------------------
void ValvePublisher::begin() {
    const uint32_t now = clock_.nowMs();
    inputs_.configure();

    for (uint8_t i = 0; i < config::kValveCount; ++i) {
        const SignalLevel level = inputs_.read(i);
        // FR-112: se interpreta el CABLEADO DEL TABLERO. La inversion por
        // transistor del nodo remoto es otra cosa y no interviene aqui.
        const ValveState  state = config::panelRequest(level);

        debouncers_[i].seed(level, now);
        // El banco queda sembrado ANTES de que el lazo atienda el bus, de modo
        // que el primer sondeo ya devuelva el estado fisico correcto.
        registers_.set(config::valveRegister(i), registerValueOf(state));
    }
    refreshDisplay();

    // Forzar el primer refresco de la linea de estado: al arrancar,
    // busHealthy_ y shownBusHealthy_ son iguales (ambos true), asi que
    // refreshDisplay() no escribe nada. Invertir shownBusHealthy_ obliga
    // a que la primera llamada ponga el texto en el LCD.
    shownBusHealthy_ = !busHealthy_;
    refreshDisplay();
}

void ValvePublisher::tick() {
    const uint32_t now = clock_.nowMs();
    pollInputs(now);
    refreshDisplay();
}

// --- FR-122: muestreo del tablero --------------------------------------------
void ValvePublisher::pollInputs(uint32_t nowMs) {
    for (uint8_t i = 0; i < config::kValveCount; ++i) {
        const SignalLevel raw = inputs_.read(i);
        if (!debouncers_[i].update(raw, nowMs, config::kDebounceMs)) {
            continue;  // sin cambio estable
        }

        // FR-116: el registro solo se escribe cuando el antirrebote confirma un
        // nivel DISTINTO. Nunca hay un valor intermedio, y nunca se pone a cero
        // por iniciativa propia: solo lo hace el operador con el interruptor.
        const ValveState state = config::panelRequest(debouncers_[i].stable());
        const uint16_t   value = registerValueOf(state);
        const uint16_t   reg   = config::valveRegister(i);

        if (registers_.get(reg) == value) {
            continue;
        }
        registers_.set(reg, value);
        log_.event(LogLevel::Info, logtag::kTx, i, static_cast<int32_t>(value));
    }
}

ValveState ValvePublisher::requested(uint8_t index) const {
    if (index >= config::kValveCount) {
        return ValveState::Closed;
    }
    return valveStateFromRegister(registers_.get(config::valveRegister(index)));
}

// --- FR-113, FR-114: bloque de diagnostico -----------------------------------
void ValvePublisher::publishDiagnostics(uint16_t secondsSincePoll,
                                        uint16_t crcErrors,
                                        uint16_t formatErrors,
                                        uint16_t validRequests,
                                        uint8_t  resetCause) {
    uint16_t irrigating = 0;
    for (uint8_t i = 0; i < config::kValveCount; ++i) {
        if (registers_.get(config::valveRegister(i)) != 0) ++irrigating;
    }

    registers_.set(config::diag::kSecondsSincePoll, secondsSincePoll);
    registers_.set(config::diag::kCrcErrors, crcErrors);
    registers_.set(config::diag::kFormatErrors, formatErrors);
    registers_.set(config::diag::kValidRequests, validRequests);
    registers_.set(config::diag::kResetCause, resetCause);
    registers_.set(config::diag::kValvesIrrigating, irrigating);
}

void ValvePublisher::setBusHealthy(bool healthy) { busHealthy_ = healthy; }

// --- FR-114b: el panel muestra INTENCION, no realidad confirmada -------------
char ValvePublisher::symbolFor(ValveState state) {
    return (state == ValveState::Open) ? symbol::kIrrigating
                                       : symbol::kNotIrrigating;
}

void ValvePublisher::refreshDisplay() {
    for (uint8_t i = 0; i < config::kValveCount; ++i) {
        const char s = symbolFor(requested(i));
        if (s != shownSymbol_[i]) {
            shownSymbol_[i] = s;
            display_.setValveSymbol(i, s);
        }
    }

    // La linea de estado deja claro que lo mostrado es lo PEDIDO, y avisa si el
    // maestro dejo de preguntar, que es el fallo mas probable en campo.
    if (busHealthy_ != shownBusHealthy_) {
        shownBusHealthy_ = busHealthy_;
        display_.setStatusLine(busHealthy_ ? "PEDIDO x TABLERO"
                                           : "PEDIDO - SIN BUS");
    }
}

}  // namespace wiga
