// -----------------------------------------------------------------------------
//  Integracion: separacion de comandos por UC511, del pin al registro.
//  Verifica: FR-204..FR-215  (specs/003-retardo-apertura-cancelable)
//
//  Recorre el camino real: interruptor -> antirrebote -> separador -> registro.
//  Las valvulas 0 y 1 comparten el UC511 #01.
// -----------------------------------------------------------------------------
#include <unity.h>

#include "fake_clock.h"
#include "fake_display.h"
#include "fake_inputs.h"
#include "fake_log.h"
#include "register_file.h"
#include "timing_config.h"
#include "valve_publisher.h"

using wiga::ValvePublisher;
using wiga::modbus::RegisterFile;
namespace config = wiga::config;
namespace logtag = wiga::logtag;

namespace {

constexpr uint32_t kT0   = 500000;
constexpr uint32_t kStep = 5;

struct System {
    wiga::test::FakeClock   clock;
    wiga::test::FakeInputs  inputs;
    wiga::test::FakeDisplay display;
    wiga::test::FakeLog     log;
    RegisterFile            registers;
    ValvePublisher          publisher{clock, inputs, registers, display, log};

    void boot(uint32_t atMs = kT0) {
        clock.set(atMs);
        publisher.begin();
    }

    uint16_t reg(uint8_t index) const {
        return registers.get(config::valveRegister(index));
    }

    /// Avanza el reloj en vueltas de 5 ms durante `ms`.
    void run(uint32_t ms) {
        for (uint32_t t = 0; t < ms; t += kStep) {
            clock.advance(kStep);
            publisher.tick();
        }
    }

    /// Avanza vuelta a vuelta hasta que el registro tome el valor. Devuelve el
    /// instante en que lo tomo, o 0 si no lo tomo dentro de `limitMs`.
    uint32_t runUntil(uint8_t index, uint16_t value, uint32_t limitMs) {
        for (uint32_t t = 0; t < limitMs; t += kStep) {
            clock.advance(kStep);
            publisher.tick();
            if (reg(index) == value) return clock.nowMs();
        }
        return 0;
    }
};

}  // namespace

// --- Vector 2, del pin al registro -------------------------------------------
void test_FR204_del_pin_al_registro_cambio_v01_a_v02(void) {
    System sys;
    sys.boot();

    sys.inputs.requestOpen(0);
    TEST_ASSERT_NOT_EQUAL(0, sys.runUntil(0, 1, 1000));
    sys.run(60000);  // V01 riega un minuto

    // El tablero pasa de V01 a V02 en el mismo instante.
    sys.log.clear();
    sys.inputs.requestClose(0);
    sys.inputs.requestOpen(1);
    const uint32_t closedAt = sys.runUntil(0, 0, 1000);
    TEST_ASSERT_NOT_EQUAL(0, closedAt);

    // En la vuelta del cierre, V02 queda retenida y se avisa por consola.
    TEST_ASSERT_EQUAL_UINT16(0, sys.reg(1));
    TEST_ASSERT_EQUAL_UINT16(1, sys.log.countOf(logtag::kWait, 1));

    // V02 abre exactamente un intervalo despues del cierre, nunca antes.
    const uint32_t openedAt = sys.runUntil(1, 1, config::kCommandSpacingMs + 1000);
    TEST_ASSERT_NOT_EQUAL(0, openedAt);
    TEST_ASSERT_EQUAL_UINT32(config::kCommandSpacingMs, openedAt - closedAt);

    // La espera no se repite en el log mientras dura.
    TEST_ASSERT_EQUAL_UINT16(1, sys.log.countOf(logtag::kWait, 1));
}

void run_separacion_uc511_tests(void) {
    RUN_TEST(test_FR204_del_pin_al_registro_cambio_v01_a_v02);
}
