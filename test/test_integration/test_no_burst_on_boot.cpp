// -----------------------------------------------------------------------------
//  Verifica: FR-010, Principio II
//
//  Tras un reset por watchdog el firmware NO puede emitir una rafaga de ordenes
//  sobre la bateria de 12 V. El estado se reconstruye leyendo las entradas.
// -----------------------------------------------------------------------------
#include <unity.h>

#include "fixture.h"

using namespace wiga;
using namespace wiga::test;

static void test_arranque_con_todo_cerrado_no_transmite(void) {
    Bench b;
    b.supervisor.begin();
    TEST_ASSERT_EQUAL_UINT16(0, b.radio.sentCount());
}

// El caso peligroso: reinicio con la mitad de las valvulas abiertas.
static void test_arranque_con_valvulas_abiertas_no_transmite(void) {
    Bench b;
    for (uint8_t i = 0; i < config::kValveCount; i += 2) {
        b.inputs.requestOpen(i);
    }
    b.supervisor.begin();

    TEST_ASSERT_EQUAL_UINT16(0, b.radio.sentCount());
    for (uint8_t i = 0; i < config::kValveCount; ++i) {
        TEST_ASSERT_TRUE(b.supervisor.record(i).verified());
    }
}

// Ni siquiera en las vueltas siguientes, mientras las entradas no cambien.
static void test_sin_cambios_no_transmite_nunca(void) {
    Bench b;
    for (uint8_t i = 0; i < config::kValveCount; i += 3) {
        b.inputs.requestOpen(i);
    }
    b.supervisor.begin();

    for (int i = 0; i < 500; ++i) {
        b.step(config::kLoopPeriodMs);
    }
    TEST_ASSERT_EQUAL_UINT16(0, b.radio.sentCount());
}

// El arranque siembra el estado fisico correcto, con la inversion aplicada.
static void test_arranque_siembra_el_estado_fisico(void) {
    Bench b;
    b.inputs.requestOpen(7);     // nivel bajo
    b.inputs.requestClose(8);    // nivel alto
    b.supervisor.begin();

    TEST_ASSERT_TRUE(b.supervisor.record(7).confirmed == ValveState::Open);
    TEST_ASSERT_TRUE(b.supervisor.record(8).confirmed == ValveState::Closed);
    TEST_ASSERT_TRUE(b.inputs.configured());
}

void run_no_burst_on_boot_tests(void) {
    RUN_TEST(test_arranque_con_todo_cerrado_no_transmite);
    RUN_TEST(test_arranque_con_valvulas_abiertas_no_transmite);
    RUN_TEST(test_sin_cambios_no_transmite_nunca);
    RUN_TEST(test_arranque_siembra_el_estado_fisico);
}
