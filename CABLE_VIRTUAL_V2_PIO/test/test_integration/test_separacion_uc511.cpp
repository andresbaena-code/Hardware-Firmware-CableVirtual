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

// --- US2: equipos distintos --------------------------------------------------
void test_FR208_del_pin_al_registro_cambio_v02_a_v03_sin_espera(void) {
    System sys;
    sys.boot();
    sys.inputs.requestOpen(1);
    TEST_ASSERT_NOT_EQUAL(0, sys.runUntil(1, 1, 1000));
    sys.run(60000);

    sys.inputs.requestClose(1);  // UC511 #01
    sys.inputs.requestOpen(2);   // UC511 #02
    const uint32_t closedAt = sys.runUntil(1, 0, 1000);
    TEST_ASSERT_NOT_EQUAL(0, closedAt);
    TEST_ASSERT_EQUAL_UINT16(1, sys.reg(2));  // en la misma vuelta
    TEST_ASSERT_EQUAL_UINT16(0, sys.log.countOf(logtag::kWait));
}

// --- US5: reinicio -----------------------------------------------------------
void test_FR211_reinicio_con_v01_regando_registro_en_1_antes_del_bus(void) {
    System sys;
    sys.inputs.requestOpen(0);
    sys.boot();
    // Sin una sola vuelta del lazo: es lo que el primer sondeo leeria.
    TEST_ASSERT_EQUAL_UINT16(1, sys.reg(0));
}

void test_FR211_cambio_tras_arranque_respeta_el_intervalo(void) {
    System sys;
    sys.inputs.requestOpen(0);
    sys.boot();

    sys.run(2000);
    sys.inputs.requestOpen(1);  // mismo UC511 que la valvula sembrada en 1
    const uint32_t openedAt = sys.runUntil(1, 1, config::kCommandSpacingMs + 1000);
    TEST_ASSERT_NOT_EQUAL(0, openedAt);
    TEST_ASSERT_EQUAL_UINT32(config::kCommandSpacingMs, openedAt - kT0);
}

// --- US3: pedido revertido ---------------------------------------------------
void test_FR206_del_pin_al_registro_pedido_revertido_no_publica(void) {
    System sys;
    sys.boot();
    sys.inputs.requestOpen(0);
    TEST_ASSERT_NOT_EQUAL(0, sys.runUntil(0, 1, 1000));

    // V02 se pide 3 s, con el UC511 #01 aun ocupado, y se retira.
    sys.inputs.requestOpen(1);
    for (uint32_t t = 0; t < 3000; t += kStep) {
        sys.clock.advance(kStep);
        sys.publisher.tick();
        TEST_ASSERT_EQUAL_UINT16(0, sys.reg(1));
    }
    sys.inputs.requestClose(1);
    for (uint32_t t = 0; t < 20000; t += kStep) {
        sys.clock.advance(kStep);
        sys.publisher.tick();
        TEST_ASSERT_EQUAL_UINT16(0, sys.reg(1));
    }
    TEST_ASSERT_EQUAL_UINT16(1, sys.log.countOf(logtag::kCancel, 1));
}

// --- US4: equipos no instalados (V09 = indice 8, UC511 #05) ------------------
constexpr uint8_t kNotInstalledValve = 8;

void test_FR212_valvula_de_equipo_no_instalado_publica_siempre_cero(void) {
    TEST_ASSERT_FALSE(config::actuatorInstalled(config::actuatorOf(kNotInstalledValve)));
    System sys;
    sys.boot();
    sys.inputs.requestOpen(kNotInstalledValve);
    for (uint32_t t = 0; t < 20000; t += kStep) {
        sys.clock.advance(kStep);
        sys.publisher.tick();
        TEST_ASSERT_EQUAL_UINT16(0, sys.reg(kNotInstalledValve));
    }
}

void test_FR213_aviso_noinst_una_vez_por_pedido(void) {
    System sys;
    sys.boot();
    sys.inputs.requestOpen(kNotInstalledValve);
    sys.run(5000);  // pedido sostenido: un solo aviso
    TEST_ASSERT_EQUAL_UINT16(1, sys.log.countOf(logtag::kNotInstalled, kNotInstalledValve));

    sys.inputs.requestClose(kNotInstalledValve);
    sys.run(500);
    sys.inputs.requestOpen(kNotInstalledValve);
    sys.run(500);  // segundo pedido: segundo aviso
    TEST_ASSERT_EQUAL_UINT16(2, sys.log.countOf(logtag::kNotInstalled, kNotInstalledValve));
}

void test_FR215_panel_muestra_guion_en_equipo_no_instalado(void) {
    System sys;
    sys.boot();
    TEST_ASSERT_EQUAL_CHAR('-', sys.display.symbol(kNotInstalledValve));
    TEST_ASSERT_EQUAL_CHAR('-', sys.display.symbol(config::kValveCount - 1));
    TEST_ASSERT_EQUAL_CHAR('0', sys.display.symbol(0));  // instalado, en reposo
}

void test_FR212_siembra_de_equipo_no_instalado_queda_en_cero(void) {
    System sys;
    sys.inputs.requestOpen(kNotInstalledValve);  // pedido ANTES de arrancar
    sys.boot();
    TEST_ASSERT_EQUAL_UINT16(0, sys.reg(kNotInstalledValve));
    TEST_ASSERT_EQUAL_UINT16(1, sys.log.countOf(logtag::kNotInstalled, kNotInstalledValve));
}

void run_separacion_uc511_tests(void) {
    RUN_TEST(test_FR204_del_pin_al_registro_cambio_v01_a_v02);
    RUN_TEST(test_FR208_del_pin_al_registro_cambio_v02_a_v03_sin_espera);
    RUN_TEST(test_FR211_reinicio_con_v01_regando_registro_en_1_antes_del_bus);
    RUN_TEST(test_FR211_cambio_tras_arranque_respeta_el_intervalo);
    RUN_TEST(test_FR206_del_pin_al_registro_pedido_revertido_no_publica);
    RUN_TEST(test_FR212_valvula_de_equipo_no_instalado_publica_siempre_cero);
    RUN_TEST(test_FR213_aviso_noinst_una_vez_por_pedido);
    RUN_TEST(test_FR215_panel_muestra_guion_en_equipo_no_instalado);
    RUN_TEST(test_FR212_siembra_de_equipo_no_instalado_queda_en_cero);
}
