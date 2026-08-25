// -----------------------------------------------------------------------------
//  Verifica: FR-009
//  El fallo de una valvula no puede degradar el servicio de las otras 23.
// -----------------------------------------------------------------------------
#include <unity.h>

#include "fixture.h"

using namespace wiga;
using namespace wiga::test;

// Destinos derivados del mapa: ver fixture.h.

// Una valvula en reposo prolongado no bloquea a las demas.
static void test_valvula_en_reposo_no_bloquea_a_las_demas(void) {
    Bench b;
    b.supervisor.begin();
    b.radio.clearSent();

    // La valvula 0 falla de forma persistente hasta entrar en reposo.
    b.inputs.requestOpen(0);
    b.settleInput();
    b.run(10u * 60u * 1000u);   // 10 minutos a cadencia real
    TEST_ASSERT_TRUE(b.supervisor.record(0).phase == ValvePhase::Backoff);

    const uint16_t antes = b.radio.sentCount();

    // La valvula 4, de otro nodo, opera con normalidad.
    b.inputs.requestOpen(4);
    b.settleInput();
    TEST_ASSERT_EQUAL_UINT16(antes + 1, b.radio.sentCount());

    b.radio.enqueue(nodeOf(4), idOf(4), SignalLevel::Low);
    b.step(10);
    TEST_ASSERT_TRUE(b.supervisor.record(4).verified());

    // Y la valvula averiada sigue en reposo, sin consumir bateria.
    TEST_ASSERT_TRUE(b.supervisor.record(0).phase == ValvePhase::Backoff);
}

// Varias valvulas averiadas no impiden atender a una sana.
static void test_varias_averiadas_no_impiden_atender_una_sana(void) {
    Bench b;
    b.supervisor.begin();

    for (uint8_t i = 0; i < kFailingCount; ++i) b.inputs.requestOpen(i);
    b.settleInput();
    b.run(20u * 60u * 1000u);   // 20 minutos a cadencia real

    for (uint8_t i = 0; i < kFailingCount; ++i) {
        TEST_ASSERT_TRUE(b.supervisor.record(i).phase == ValvePhase::Backoff);
    }

    const uint16_t antes = b.radio.sentCount();
    b.inputs.requestOpen(kLastValve);
    b.settleInput();
    TEST_ASSERT_EQUAL_UINT16(antes + 1, b.radio.sentCount());
    TEST_ASSERT_EQUAL_HEX32(nodeOf(kLastValve), b.radio.lastSent().nodeAddressLsb);
    TEST_ASSERT_EQUAL_UINT8(idOf(kLastValve), b.radio.lastSent().valveId);
}

// El contador de fallos es por valvula, no global (FR-028).
static void test_contador_de_fallos_es_por_valvula(void) {
    Bench b;
    b.supervisor.begin();

    b.inputs.requestOpen(0);
    b.settleInput();
    b.run(10u * 60u * 1000u);

    TEST_ASSERT_TRUE(b.supervisor.record(0).radioFailures > 0);
    TEST_ASSERT_EQUAL_UINT16(0, b.supervisor.record(1).radioFailures);
    TEST_ASSERT_EQUAL_UINT16(0, b.supervisor.record(kLastValve).radioFailures);
}

void run_fault_isolation_tests(void) {
    RUN_TEST(test_valvula_en_reposo_no_bloquea_a_las_demas);
    RUN_TEST(test_varias_averiadas_no_impiden_atender_una_sana);
    RUN_TEST(test_contador_de_fallos_es_por_valvula);
}
