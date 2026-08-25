// -----------------------------------------------------------------------------
//  Verifica: FR-004  (invariantes S-01, S-02)
//  Solo UNA orden latch a la vez. El barrido circular evita que una valvula que
//  falla de forma persistente monopolice los turnos: defecto latente del
//  firmware base, que recorria siempre desde 0 y hacia break en la primera.
// -----------------------------------------------------------------------------
#include <unity.h>

#include "latch_scheduler.h"

using namespace wiga;

static constexpr uint16_t kGuard = 1500;
static constexpr uint8_t  kCount = 4;

static void allPending(ValveRecord* r, uint8_t count) {
    for (uint8_t i = 0; i < count; ++i) r[i].phase = ValvePhase::Pending;
}

// S-01: la ventana de exclusion bloquea el segundo disparo.
static void test_no_concede_dos_turnos_dentro_de_la_ventana(void) {
    LatchScheduler s(kGuard);
    ValveRecord r[kCount];
    allPending(r, kCount);

    const ValveIndex first = s.grant(r, kCount, 1000);
    TEST_ASSERT_NOT_EQUAL(kNoValve, first);
    s.markServed(first, 1000);

    TEST_ASSERT_EQUAL_UINT8(kNoValve, s.grant(r, kCount, 1000));
    TEST_ASSERT_EQUAL_UINT8(kNoValve, s.grant(r, kCount, 2499));  // 1499 ms
    TEST_ASSERT_NOT_EQUAL(kNoValve, s.grant(r, kCount, 2500));    // 1500 ms
}

// S-02: el reparto es circular, no siempre la de indice menor.
static void test_reparto_circular_entre_candidatas(void) {
    LatchScheduler s(kGuard);
    ValveRecord r[kCount];
    allPending(r, kCount);

    bool servida[kCount] = {false, false, false, false};
    uint32_t now = 0;

    for (uint8_t turno = 0; turno < kCount; ++turno) {
        const ValveIndex idx = s.grant(r, kCount, now);
        TEST_ASSERT_NOT_EQUAL(kNoValve, idx);
        TEST_ASSERT_FALSE(servida[idx]);  // ninguna se atiende dos veces
        servida[idx] = true;
        s.markServed(idx, now);
        now += kGuard;
    }
    for (uint8_t i = 0; i < kCount; ++i) {
        TEST_ASSERT_TRUE(servida[i]);  // las cuatro fueron atendidas
    }
}

// Una valvula que falla de forma persistente no puede acaparar los turnos.
static void test_valvula_persistente_no_monopoliza(void) {
    LatchScheduler s(kGuard);
    ValveRecord r[kCount];
    r[0].phase = ValvePhase::Pending;   // esta nunca se confirma
    r[1].phase = ValvePhase::Pending;
    r[2].phase = ValvePhase::Verified;
    r[3].phase = ValvePhase::Verified;

    uint32_t now = 0;
    const ValveIndex a = s.grant(r, kCount, now);
    s.markServed(a, now);
    now += kGuard;

    const ValveIndex b = s.grant(r, kCount, now);
    TEST_ASSERT_NOT_EQUAL(a, b);  // la segunda candidata obtiene su turno
}

// Solo las valvulas en Pending son candidatas.
static void test_solo_las_pendientes_son_candidatas(void) {
    LatchScheduler s(kGuard);
    ValveRecord r[kCount];
    r[0].phase = ValvePhase::Verified;
    r[1].phase = ValvePhase::Awaiting;
    r[2].phase = ValvePhase::Backoff;
    r[3].phase = ValvePhase::Pending;

    TEST_ASSERT_EQUAL_UINT8(3, s.grant(r, kCount, 0));
}

static void test_sin_candidatas_devuelve_no_valve(void) {
    LatchScheduler s(kGuard);
    ValveRecord r[kCount];  // todas Verified por defecto
    TEST_ASSERT_EQUAL_UINT8(kNoValve, s.grant(r, kCount, 0));
    TEST_ASSERT_EQUAL_UINT8(kNoValve, s.grant(nullptr, 0, 0));
}

void run_scheduler_tests(void) {
    RUN_TEST(test_no_concede_dos_turnos_dentro_de_la_ventana);
    RUN_TEST(test_reparto_circular_entre_candidatas);
    RUN_TEST(test_valvula_persistente_no_monopoliza);
    RUN_TEST(test_solo_las_pendientes_son_candidatas);
    RUN_TEST(test_sin_candidatas_devuelve_no_valve);
}
