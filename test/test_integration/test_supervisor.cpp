// -----------------------------------------------------------------------------
//  Verifica: FR-001, FR-003, FR-004, FR-005, FR-016, FR-017, FR-018, FR-020,
//            FR-021, SC-005  (transiciones T2..T11 de data-model.md)
// -----------------------------------------------------------------------------
#include <unity.h>

#include "fixture.h"

using namespace wiga;
using namespace wiga::test;

// Fila 0 del mapa de instalacion.
static constexpr uint32_t kNode0 = 0x42878FDBUL;
static constexpr uint8_t  kId0   = 0;
// Fila 4: nodo distinto, para probar aislamiento entre canales.
static constexpr uint32_t kNode4 = 0x41C3F584UL;
static constexpr uint8_t  kId4   = 4;

// --- T2 + T3: la entrada cambia y se transmite la orden correcta ------------
static void test_cambio_de_entrada_transmite_la_orden(void) {
    Bench b;
    b.supervisor.begin();
    b.radio.clearSent();

    b.inputs.requestOpen(0);
    b.settleInput();

    TEST_ASSERT_EQUAL_UINT16(1, b.radio.sentCount());
    TEST_ASSERT_EQUAL_HEX32(kNode0, b.radio.lastSent().nodeAddressLsb);
    TEST_ASSERT_EQUAL_UINT8(kId0, b.radio.lastSent().valveId);
    // Abrir = nivel electrico bajo (logica inversa por transistor).
    TEST_ASSERT_TRUE(b.radio.lastSent().level == SignalLevel::Low);
    TEST_ASSERT_TRUE(b.supervisor.record(0).phase == ValvePhase::Awaiting);
}

// --- T4: la confirmacion del nodo es la unica via a Verified (FR-005) -------
static void test_confirmacion_valida_cierra_el_ciclo(void) {
    Bench b;
    b.supervisor.begin();
    b.inputs.requestOpen(0);
    b.settleInput();
    TEST_ASSERT_FALSE(b.supervisor.record(0).verified());

    b.radio.enqueue(kNode0, kId0, SignalLevel::Low);  // el nodo confirma abierta
    b.step(10);

    TEST_ASSERT_TRUE(b.supervisor.record(0).verified());
    TEST_ASSERT_TRUE(b.supervisor.record(0).confirmed == ValveState::Open);
    TEST_ASSERT_EQUAL_UINT8(0, b.supervisor.record(0).attempts);
    TEST_ASSERT_EQUAL_UINT16(1, b.log.countOf(logtag::kAck, 0));
}

// --- T5: una confirmacion que no coincide NO confirma -----------------------
static void test_confirmacion_con_nivel_distinto_no_confirma(void) {
    Bench b;
    b.supervisor.begin();
    b.inputs.requestOpen(0);
    b.settleInput();

    // El nodo responde "cerrada" cuando se le pidio abrir.
    b.radio.enqueue(kNode0, kId0, SignalLevel::High);
    b.step(10);

    TEST_ASSERT_FALSE(b.supervisor.record(0).verified());
    TEST_ASSERT_TRUE(b.supervisor.record(0).phase == ValvePhase::Awaiting);
}

// --- T10 / RX-05: confirmacion de un nodo no configurado --------------------
static void test_confirmacion_de_nodo_desconocido_se_descarta(void) {
    Bench b;
    b.supervisor.begin();
    b.inputs.requestOpen(0);
    b.settleInput();

    b.radio.enqueue(0xDEADBEEFUL, 0, SignalLevel::Low);
    b.step(10);

    TEST_ASSERT_FALSE(b.supervisor.record(0).verified());
    TEST_ASSERT_TRUE(b.log.countOf(logtag::kDrop) >= 1);
}

// --- T11: volver atras antes del turno no gasta un ciclo latch --------------
static void test_cambio_revertido_antes_del_turno_no_transmite(void) {
    Bench b;
    b.supervisor.begin();

    // La valvula 1 toma el turno y abre la ventana de exclusion.
    b.inputs.requestOpen(1);
    b.settleInput();
    TEST_ASSERT_EQUAL_UINT16(1, b.radio.sentCount());

    // Mientras la ventana sigue vigente, la valvula 0 cambia y vuelve atras.
    b.inputs.requestOpen(0);
    b.settleInput();
    TEST_ASSERT_TRUE(b.supervisor.record(0).phase == ValvePhase::Pending);

    b.inputs.requestClose(0);
    b.settleInput();
    TEST_ASSERT_TRUE(b.supervisor.record(0).phase == ValvePhase::Verified);

    // Aunque venza la ventana, la valvula 0 no transmite nada.
    b.step(config::kLatchGuardMs);
    TEST_ASSERT_EQUAL_UINT16(1, b.radio.sentCount());
}

// --- FR-004: una sola orden a la vez ----------------------------------------
static void test_disparo_secuencial_respeta_la_ventana(void) {
    Bench b;
    b.supervisor.begin();
    b.radio.clearSent();

    // Cinco entradas cambian a la vez.
    for (uint8_t i = 0; i < 5; ++i) b.inputs.requestOpen(i);
    b.settleInput();

    TEST_ASSERT_EQUAL_UINT16(1, b.radio.sentCount());  // solo una transmite

    // Dentro de la ventana no se transmite ninguna mas.
    b.step(config::kLatchGuardMs - 1);
    TEST_ASSERT_EQUAL_UINT16(1, b.radio.sentCount());

    // Al vencer, entra la siguiente.
    b.step(1);
    TEST_ASSERT_EQUAL_UINT16(2, b.radio.sentCount());
    TEST_ASSERT_NOT_EQUAL(b.radio.sent(0).valveId, b.radio.sent(1).valveId);
}

// --- SC-005 / NFR-003: proteccion de bateria ante fallo persistente ---------
static void test_maximo_cinco_disparos_en_la_primera_media_hora(void) {
    Bench b;
    b.supervisor.begin();
    b.radio.clearSent();

    b.inputs.requestOpen(0);
    b.settleInput();  // primer disparo

    // 35 minutos a cadencia real sin que el nodo responda jamas.
    b.run(35u * 60u * 1000u);

    TEST_ASSERT_EQUAL_UINT16(5, b.radio.sentCount());
    TEST_ASSERT_TRUE(b.supervisor.record(0).phase == ValvePhase::Backoff);
    TEST_ASSERT_EQUAL_UINT16(1, b.log.countOf(logtag::kBackoff, 0));
}

// --- FR-008: el cambio de entrada abandona el reposo de inmediato -----------
static void test_cambio_de_entrada_saca_del_reposo(void) {
    Bench b;
    b.supervisor.begin();
    b.radio.clearSent();

    b.inputs.requestOpen(0);
    b.settleInput();
    b.run(10u * 60u * 1000u);
    TEST_ASSERT_TRUE(b.supervisor.record(0).phase == ValvePhase::Backoff);

    const uint16_t antes = b.radio.sentCount();
    b.inputs.requestClose(0);
    b.settleInput();

    // Vuelve al estado confirmado: no hay nada que transmitir, pero salio del reposo.
    TEST_ASSERT_TRUE(b.supervisor.record(0).phase == ValvePhase::Verified);
    TEST_ASSERT_EQUAL_UINT8(0, b.supervisor.record(0).attempts);

    // Y ahora si cambia de verdad, dispara de inmediato.
    b.inputs.requestOpen(0);
    b.settleInput();
    TEST_ASSERT_EQUAL_UINT16(antes + 1, b.radio.sentCount());
}

// --- FR-018: una confirmacion recibida nunca se descarta --------------------
static void test_confirmacion_recibida_evita_el_reintento(void) {
    Bench b;
    b.supervisor.begin();
    b.radio.clearSent();

    b.inputs.requestOpen(0);
    b.settleInput();
    TEST_ASSERT_EQUAL_UINT16(1, b.radio.sentCount());

    b.radio.enqueue(kNode0, kId0, SignalLevel::Low);
    b.step(10);

    // Media hora despues no debe haber ni un solo reintento ni un TMO.
    b.run(30u * 60u * 1000u);
    TEST_ASSERT_EQUAL_UINT16(1, b.radio.sentCount());
    TEST_ASSERT_EQUAL_UINT16(0, b.log.countOf(logtag::kTimeout, 0));
}

// --- FR-017: varias confirmaciones acumuladas se procesan todas -------------
static void test_varias_confirmaciones_en_la_misma_vuelta(void) {
    Bench b;
    b.supervisor.begin();

    b.inputs.requestOpen(0);
    b.settleInput();
    b.inputs.requestOpen(4);
    b.step(config::kLatchGuardMs);
    b.settleInput();

    b.radio.enqueue(kNode0, kId0, SignalLevel::Low);
    b.radio.enqueue(kNode4, kId4, SignalLevel::Low);
    b.step(10);

    TEST_ASSERT_TRUE(b.supervisor.record(0).verified());
    TEST_ASSERT_TRUE(b.supervisor.record(4).verified());
}

// --- FR-020, FR-021: simbolos del panel -------------------------------------
static void test_simbolos_del_panel(void) {
    Bench b;
    b.supervisor.begin();

    // Arranque: todas cerradas.
    TEST_ASSERT_EQUAL_CHAR(symbol::kClosed, b.display.symbol(0));

    b.inputs.requestOpen(0);
    b.settleInput();
    TEST_ASSERT_EQUAL_CHAR(symbol::kPending, b.display.symbol(0));  // orden en curso

    b.radio.enqueue(kNode0, kId0, SignalLevel::Low);
    b.step(10);
    TEST_ASSERT_EQUAL_CHAR(symbol::kOpen, b.display.symbol(0));     // abierta

    // Fallo persistente en la valvula 4.
    b.inputs.requestOpen(4);
    b.settleInput();
    b.run(10u * 60u * 1000u);
    TEST_ASSERT_EQUAL_CHAR(symbol::kFailed, b.display.symbol(4));
}

// --- RD-01: si el radio rechaza la trama, no se consume intento -------------
static void test_fallo_de_envio_no_consume_intento(void) {
    Bench b;
    b.supervisor.begin();
    b.radio.clearSent();

    b.radio.failNextSend();
    b.inputs.requestOpen(0);
    b.settleInput();

    TEST_ASSERT_EQUAL_UINT16(0, b.radio.sentCount());
    TEST_ASSERT_TRUE(b.supervisor.record(0).phase == ValvePhase::Pending);
    TEST_ASSERT_EQUAL_UINT8(0, b.supervisor.record(0).attempts);

    // En la siguiente vuelta lo vuelve a intentar y ahora si sale.
    b.step(1);
    TEST_ASSERT_EQUAL_UINT16(1, b.radio.sentCount());
    TEST_ASSERT_EQUAL_UINT8(1, b.supervisor.record(0).attempts);
}

void run_supervisor_tests(void) {
    RUN_TEST(test_cambio_de_entrada_transmite_la_orden);
    RUN_TEST(test_confirmacion_valida_cierra_el_ciclo);
    RUN_TEST(test_confirmacion_con_nivel_distinto_no_confirma);
    RUN_TEST(test_confirmacion_de_nodo_desconocido_se_descarta);
    RUN_TEST(test_cambio_revertido_antes_del_turno_no_transmite);
    RUN_TEST(test_disparo_secuencial_respeta_la_ventana);
    RUN_TEST(test_maximo_cinco_disparos_en_la_primera_media_hora);
    RUN_TEST(test_cambio_de_entrada_saca_del_reposo);
    RUN_TEST(test_confirmacion_recibida_evita_el_reintento);
    RUN_TEST(test_varias_confirmaciones_en_la_misma_vuelta);
    RUN_TEST(test_simbolos_del_panel);
    RUN_TEST(test_fallo_de_envio_no_consume_intento);
}
