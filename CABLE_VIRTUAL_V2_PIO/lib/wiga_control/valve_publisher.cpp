// Cubre: FR-109, FR-110, FR-111, FR-112, FR-114b, FR-115, FR-116, FR-119, FR-122
//        FR-204, FR-205, FR-209, FR-211, FR-212, FR-213, FR-215, FR-216  (spec 003)
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

    ValveState seeded[config::kValveCount];
    for (uint8_t i = 0; i < config::kValveCount; ++i) {
        const SignalLevel level = inputs_.read(i);
        // FR-112: se interpreta el CABLEADO DEL TABLERO. La inversion por
        // transistor del nodo remoto es otra cosa y no interviene aqui.
        const ValveState  request = config::panelRequest(level);
        const ValveState  state   = desiredFor(i, request);
        if (request == ValveState::Open && !installed(i)) {
            log_.event(LogLevel::Warn, logtag::kNotInstalled, i,
                       static_cast<int32_t>(config::actuatorOf(i)));
        }

        debouncers_[i].seed(level, now);
        // El banco queda sembrado ANTES de que el lazo atienda el bus, de modo
        // que el primer sondeo ya devuelva el estado fisico correcto.
        registers_.set(config::valveRegister(i), registerValueOf(state));
        seeded[i] = state;
    }
    // El separador parte del mismo estado que el banco: si no, veria como
    // cambio pendiente lo que ya esta publicado.
    spacer_.begin(seeded, now);
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
    // Valvulas cuyo cambio quedo pendiente EN ESTA vuelta, para avisar una sola
    // vez de la espera y no en cada vuelta mientras dura.
    static_assert(config::kValveCount <= 32, "la mascara de pendientes es de 32 bits");
    uint32_t queuedNow = 0;

    for (uint8_t i = 0; i < config::kValveCount; ++i) {
        const SignalLevel raw  = inputs_.read(i);
        const bool        edge = debouncers_[i].update(raw, nowMs, config::kDebounceMs);
        const ValveState  request = config::panelRequest(debouncers_[i].stable());

        // FR-213: el aviso sale del flanco del antirrebote, asi que es uno por
        // pedido sin guardar nada mas.
        if (edge && request == ValveState::Open && !installed(i)) {
            log_.event(LogLevel::Warn, logtag::kNotInstalled, i,
                       static_cast<int32_t>(config::actuatorOf(i)));
        }

        // El deseado se informa en CADA vuelta, no solo en el flanco: un cambio
        // retenido por su UC511 se publica varias vueltas despues.
        switch (spacer_.update(i, desiredFor(i, request), nowMs)) {
            case CommandSpacer::Outcome::Queued:
                queuedNow |= (uint32_t{1} << i);
                break;
            case CommandSpacer::Outcome::Cancelled:
                log_.event(LogLevel::Info, logtag::kCancel, i,
                           static_cast<int32_t>(config::actuatorOf(i)));
                break;
            case CommandSpacer::Outcome::Unchanged:
                break;
        }
    }

    // FR-116, FR-209: el registro SOLO se escribe con lo que autoriza el
    // separador, y lo que autoriza es siempre lo que el tablero pide. Nunca hay
    // un valor intermedio ni un cero por iniciativa propia.
    ValveIndex index;
    ValveState state;
    while (spacer_.next(nowMs, index, state)) {
        const uint16_t value = registerValueOf(state);
        registers_.set(config::valveRegister(index), value);
        log_.event(LogLevel::Info, logtag::kTx, index, static_cast<int32_t>(value));
    }

    for (uint8_t i = 0; i < config::kValveCount; ++i) {
        if ((queuedNow & (uint32_t{1} << i)) != 0 && spacer_.pending(i)) {
            log_.event(LogLevel::Info, logtag::kWait, i,
                       static_cast<int32_t>(config::actuatorOf(i)));
        }
    }
}

// --- FR-212: equipos no instalados -------------------------------------------
bool ValvePublisher::installed(uint8_t index) {
    return config::actuatorInstalled(config::actuatorOf(index));
}

/// Una valvula de un UC511 no instalado publica siempre 0: si alguien olvida
/// marcar el equipo al crecer, la valvula no abre y se nota en la puesta en
/// marcha. El olvido falla hacia cerrado.
ValveState ValvePublisher::desiredFor(uint8_t index, ValveState request) {
    return installed(index) ? request : ValveState::Closed;
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
        // FR-215: '-' es configuracion ("este UC511 no existe"), no un estado
        // del campo, asi que no rompe la honestidad de la interfaz.
        const char s = installed(i) ? symbolFor(requested(i)) : symbol::kNotInstalled;
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
