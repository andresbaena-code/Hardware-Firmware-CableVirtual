// Cubre: FR-027, FR-028, FR-029
#include "serial_log.h"

#include <Arduino.h>

#include "timing_config.h"

namespace wiga {
namespace hal {

void SerialLog::begin() {
#if WIGA_LOG_LEVEL > 0
    Serial.begin(config::kConsoleBaudRate);
#endif
}

void SerialLog::event(LogLevel level, const char* tag,
                      ValveIndex index, int32_t value) {
#if WIGA_LOG_LEVEL > 0
    if (!enabled(level)) return;

    // LG-01: si no cabe en el buffer de transmision, la traza se pierde. Jamas
    // se espera a que se vacie: eso bloquearia el lazo de control.
    if (Serial.availableForWrite() < 24) return;

    // LG-04: formato de campos fijos, legible por maquina.
    Serial.print(millis());
    Serial.print(F(" ["));
    Serial.print(tag);
    Serial.print(F("] v="));
    if (index == kNoValve) {
        Serial.print(F("--"));
    } else {
        Serial.print(index);
    }
    Serial.print(F(" x="));
    Serial.println(value);
#else
    (void)level; (void)tag; (void)index; (void)value;
#endif
}

void SerialLog::message(LogLevel level, const char* text) {
#if WIGA_LOG_LEVEL > 0
    if (!enabled(level)) return;
    if (Serial.availableForWrite() < 24) return;
    Serial.print(millis());
    Serial.print(F(" "));
    Serial.println(text);
#else
    (void)level; (void)text;
#endif
}

}  // namespace hal
}  // namespace wiga
