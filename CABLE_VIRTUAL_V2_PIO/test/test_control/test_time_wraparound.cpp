// -----------------------------------------------------------------------------
//  Verifica: FR-013, NFR-002  (invariante R-04, research.md R-005)
//
//  millis() desborda cada ~49,7 dias. Con NFR-002 exigiendo 90 dias continuos,
//  el desbordamiento ocurre SIEMPRE dentro del periodo de aceptacion. Estas
//  pruebas lo fuerzan en milisegundos en lugar de esperar 49 dias.
// -----------------------------------------------------------------------------
#include <unity.h>

#include "latch_scheduler.h"
#include "retry_policy.h"
#include "time_math.h"

using namespace wiga;

static constexpr uint32_t kNearMax = 0xFFFFFF00UL;  // ~256 ms antes del desborde

// La comparacion de intervalo sobrevive al desbordamiento.
static void test_elapsed_cruza_el_desbordamiento(void) {
    const uint32_t since = 0xFFFFFFF0UL;
    const uint32_t now   = 0x00000010UL;  // 32 ms despues, ya desbordado
    TEST_ASSERT_TRUE(elapsed(now, since, 32));
    TEST_ASSERT_FALSE(elapsed(now, since, 33));
}

// La comparacion de plazo sobrevive al desbordamiento en ambos sentidos.
static void test_reached_cruza_el_desbordamiento(void) {
    // Plazo ya desbordado, reloj todavia no.
    TEST_ASSERT_FALSE(reached(0xFFFFFFF0UL, 0x00000010UL));
    // Reloj ya desbordado, plazo tambien: se alcanzo.
    TEST_ASSERT_TRUE(reached(0x00000020UL, 0x00000010UL));
    // Reloj justo antes del plazo.
    TEST_ASSERT_FALSE(reached(0x0000000FUL, 0x00000010UL));
}

// El caso que rompe la comparacion ingenua: con `now >= deadline`, un plazo que
// desbordo se daria por vencido de inmediato.
static void test_la_comparacion_ingenua_fallaria_aqui(void) {
    const uint32_t now      = 0xFFFFFF01UL;  // reloj alto, sin desbordar
    const uint32_t deadline = 0x00000064UL;  // plazo ya desbordado, 100 ms

    TEST_ASSERT_TRUE(now >= deadline);        // lo que diria la version ingenua
    TEST_ASSERT_FALSE(reached(now, deadline));  // lo correcto: aun no vence
}

// FR-013 sobre la politica de reintentos: la espera de 30 s cruza el desborde.
static void test_espera_de_reintento_cruza_el_desbordamiento(void) {
    RetryParams p;
    p.retryIntervalMs   = 30000;
    p.backoffIntervalMs = 3600000;
    p.maxAttempts       = 5;
    RetryPolicy policy(p);

    ValveRecord r;
    r.confirmed = ValveState::Closed;
    policy.onRequestChanged(r, ValveState::Open);

    policy.onTransmitted(r, kNearMax);  // el plazo cae despues del desborde

    // Instante intermedio: el reloj ya desbordo, el plazo aun no vence.
    const uint32_t mid = static_cast<uint32_t>(kNearMax + 300);
    TEST_ASSERT_FALSE(policy.waitExpired(r, mid));

    const uint32_t casi = static_cast<uint32_t>(kNearMax + 29999);
    TEST_ASSERT_FALSE(policy.waitExpired(r, casi));

    const uint32_t justo = static_cast<uint32_t>(kNearMax + 30000);
    TEST_ASSERT_TRUE(policy.waitExpired(r, justo));
}

// FR-013 sobre el reposo de una hora.
static void test_reposo_de_una_hora_cruza_el_desbordamiento(void) {
    RetryParams p;
    p.retryIntervalMs   = 30000;
    p.backoffIntervalMs = 3600000;
    p.maxAttempts       = 1;
    RetryPolicy policy(p);

    ValveRecord r;
    r.confirmed = ValveState::Closed;
    policy.onRequestChanged(r, ValveState::Open);
    policy.onTransmitted(r, kNearMax);
    TEST_ASSERT_TRUE(policy.onTimeout(r, static_cast<uint32_t>(kNearMax + 30000)));

    const uint32_t base = static_cast<uint32_t>(kNearMax + 30000);
    TEST_ASSERT_FALSE(policy.waitExpired(r, static_cast<uint32_t>(base + 3599999)));
    TEST_ASSERT_TRUE(policy.waitExpired(r, static_cast<uint32_t>(base + 3600000)));
}

// FR-013 sobre la ventana de exclusion de disparo.
static void test_ventana_de_exclusion_cruza_el_desbordamiento(void) {
    LatchScheduler s(1500);
    ValveRecord r[2];
    r[0].phase = ValvePhase::Pending;
    r[1].phase = ValvePhase::Pending;

    const ValveIndex first = s.grant(r, 2, kNearMax);
    TEST_ASSERT_NOT_EQUAL(kNoValve, first);
    s.markServed(first, kNearMax);

    TEST_ASSERT_EQUAL_UINT8(kNoValve, s.grant(r, 2, static_cast<uint32_t>(kNearMax + 300)));
    TEST_ASSERT_EQUAL_UINT8(kNoValve, s.grant(r, 2, static_cast<uint32_t>(kNearMax + 1499)));
    TEST_ASSERT_NOT_EQUAL(kNoValve, s.grant(r, 2, static_cast<uint32_t>(kNearMax + 1500)));
}

void run_time_wraparound_tests(void) {
    RUN_TEST(test_elapsed_cruza_el_desbordamiento);
    RUN_TEST(test_reached_cruza_el_desbordamiento);
    RUN_TEST(test_la_comparacion_ingenua_fallaria_aqui);
    RUN_TEST(test_espera_de_reintento_cruza_el_desbordamiento);
    RUN_TEST(test_reposo_de_una_hora_cruza_el_desbordamiento);
    RUN_TEST(test_ventana_de_exclusion_cruza_el_desbordamiento);
}
