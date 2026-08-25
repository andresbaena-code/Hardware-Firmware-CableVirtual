// -----------------------------------------------------------------------------
//  Verifica: FR-006, FR-007, FR-008  (transiciones T3, T6, T7, T8, T9, T11)
//  Principio II (NO NEGOCIABLE): reintentos finitos y reposo prolongado. Cada
//  disparo latch es un pico de corriente sobre una bateria de 12 V / 9 Ah.
// -----------------------------------------------------------------------------
#include <unity.h>

#include "retry_policy.h"

using namespace wiga;

static RetryParams params() {
    RetryParams p;
    p.retryIntervalMs   = 30000;
    p.backoffIntervalMs = 3600000;
    p.maxAttempts       = 5;
    return p;
}

// T2: un cambio de solicitud deja la valvula lista para disparar.
static void test_cambio_de_solicitud_pasa_a_pending(void) {
    RetryPolicy policy(params());
    ValveRecord r;
    r.requested = ValveState::Closed;
    r.confirmed = ValveState::Closed;
    r.phase     = ValvePhase::Verified;

    policy.onRequestChanged(r, ValveState::Open);
    TEST_ASSERT_TRUE(r.phase == ValvePhase::Pending);
    TEST_ASSERT_TRUE(r.requested == ValveState::Open);
    TEST_ASSERT_EQUAL_UINT8(0, r.attempts);
}

// T11: si el operador vuelve atras antes del disparo, no hay nada que transmitir.
static void test_volver_al_estado_confirmado_cancela_la_orden(void) {
    RetryPolicy policy(params());
    ValveRecord r;
    r.requested = ValveState::Closed;
    r.confirmed = ValveState::Closed;

    policy.onRequestChanged(r, ValveState::Open);
    TEST_ASSERT_TRUE(r.phase == ValvePhase::Pending);

    policy.onRequestChanged(r, ValveState::Closed);
    TEST_ASSERT_TRUE(r.phase == ValvePhase::Verified);  // no se transmite nada
}

// FR-006: exactamente 5 intentos, separados por el intervalo corto.
static void test_cinco_intentos_separados_treinta_segundos(void) {
    RetryPolicy policy(params());
    ValveRecord r;
    r.confirmed = ValveState::Closed;
    policy.onRequestChanged(r, ValveState::Open);

    uint32_t now = 1000;
    for (uint8_t intento = 1; intento <= 5; ++intento) {
        TEST_ASSERT_TRUE(r.phase == ValvePhase::Pending);
        policy.onTransmitted(r, now);
        TEST_ASSERT_TRUE(r.phase == ValvePhase::Awaiting);
        TEST_ASSERT_EQUAL_UINT8(intento, r.attempts);

        // La espera no vence antes de tiempo.
        TEST_ASSERT_FALSE(policy.waitExpired(r, now + 29999));
        now += 30000;
        TEST_ASSERT_TRUE(policy.waitExpired(r, now));

        const bool backoff = policy.onTimeout(r, now);
        if (intento < 5) {
            TEST_ASSERT_FALSE(backoff);
        } else {
            TEST_ASSERT_TRUE(backoff);  // FR-007
        }
    }
    TEST_ASSERT_TRUE(r.phase == ValvePhase::Backoff);
    TEST_ASSERT_EQUAL_UINT16(5, r.radioFailures);
}

// FR-007: el reposo prolongado dura una hora completa.
static void test_reposo_dura_una_hora(void) {
    RetryPolicy policy(params());
    ValveRecord r;
    r.confirmed = ValveState::Closed;
    policy.onRequestChanged(r, ValveState::Open);

    uint32_t now = 0;
    for (uint8_t i = 0; i < 5; ++i) {
        policy.onTransmitted(r, now);
        now += 30000;
        policy.onTimeout(r, now);
    }
    TEST_ASSERT_TRUE(r.phase == ValvePhase::Backoff);

    TEST_ASSERT_FALSE(policy.waitExpired(r, now + 3599999));
    TEST_ASSERT_TRUE(policy.waitExpired(r, now + 3600000));
}

// FR-008: un cambio de entrada abandona el reposo de inmediato.
static void test_cambio_de_entrada_cancela_el_reposo(void) {
    RetryPolicy policy(params());
    ValveRecord r;
    r.confirmed = ValveState::Closed;
    policy.onRequestChanged(r, ValveState::Open);

    uint32_t now = 0;
    for (uint8_t i = 0; i < 5; ++i) {
        policy.onTransmitted(r, now);
        now += 30000;
        policy.onTimeout(r, now);
    }
    TEST_ASSERT_TRUE(r.phase == ValvePhase::Backoff);

    policy.onRequestChanged(r, ValveState::Closed);
    TEST_ASSERT_TRUE(r.phase == ValvePhase::Verified);   // vuelve al estado confirmado

    policy.onRequestChanged(r, ValveState::Open);
    TEST_ASSERT_TRUE(r.phase == ValvePhase::Pending);    // listo de inmediato
    TEST_ASSERT_EQUAL_UINT8(0, r.attempts);              // intentos rapidos de nuevo
}

// T8: tras el reposo, attempts se mantiene en el limite, de modo que el
// siguiente fallo vuelve a costar una hora. Es lo que garantiza SC-005.
static void test_tras_el_reposo_el_siguiente_fallo_cuesta_otra_hora(void) {
    RetryPolicy policy(params());
    ValveRecord r;
    r.confirmed = ValveState::Closed;
    policy.onRequestChanged(r, ValveState::Open);

    uint32_t now = 0;
    for (uint8_t i = 0; i < 5; ++i) {
        policy.onTransmitted(r, now);
        now += 30000;
        policy.onTimeout(r, now);
    }
    // Vence el reposo y se transmite de nuevo.
    now += 3600000;
    r.phase = ValvePhase::Pending;      // lo hace el supervisor (T8)
    policy.onTransmitted(r, now);
    TEST_ASSERT_EQUAL_UINT8(5, r.attempts);  // no crece mas alla del limite

    now += 30000;
    TEST_ASSERT_TRUE(policy.onTimeout(r, now));  // vuelve directo a reposo
    TEST_ASSERT_TRUE(r.phase == ValvePhase::Backoff);
}

// Invariante R-01: la confirmacion es la unica ruta hacia Verified.
static void test_confirmacion_es_la_unica_via_a_verified(void) {
    RetryPolicy policy(params());
    ValveRecord r;
    r.confirmed = ValveState::Closed;
    policy.onRequestChanged(r, ValveState::Open);
    policy.onTransmitted(r, 0);
    TEST_ASSERT_FALSE(r.verified());   // transmitir NO confirma (FR-005)

    policy.onConfirmed(r, ValveState::Open);
    TEST_ASSERT_TRUE(r.verified());
    TEST_ASSERT_TRUE(r.confirmed == ValveState::Open);
    TEST_ASSERT_EQUAL_UINT8(0, r.attempts);
}

void run_retry_policy_tests(void) {
    RUN_TEST(test_cambio_de_solicitud_pasa_a_pending);
    RUN_TEST(test_volver_al_estado_confirmado_cancela_la_orden);
    RUN_TEST(test_cinco_intentos_separados_treinta_segundos);
    RUN_TEST(test_reposo_dura_una_hora);
    RUN_TEST(test_cambio_de_entrada_cancela_el_reposo);
    RUN_TEST(test_tras_el_reposo_el_siguiente_fallo_cuesta_otra_hora);
    RUN_TEST(test_confirmacion_es_la_unica_via_a_verified);
}
