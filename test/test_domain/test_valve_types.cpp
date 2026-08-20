// -----------------------------------------------------------------------------
//  Verifica: FR-021, FR-021b  (invariantes V-01, V-02)
//  La logica inversa por transistor es una caracteristica del hardware, no un
//  defecto. Estas pruebas existen para que nadie la "corrija".
// -----------------------------------------------------------------------------
#include <unity.h>

#include "valve_types.h"

using namespace wiga;

// FR-021: tabla de inversion del contrato (application-protocol.md seccion 4).
static void test_nivel_alto_es_valvula_cerrada(void) {
    TEST_ASSERT_TRUE(toValveState(SignalLevel::High) == ValveState::Closed);
}

static void test_nivel_bajo_es_valvula_abierta(void) {
    TEST_ASSERT_TRUE(toValveState(SignalLevel::Low) == ValveState::Open);
}

static void test_valvula_cerrada_pide_nivel_alto(void) {
    TEST_ASSERT_TRUE(toSignalLevel(ValveState::Closed) == SignalLevel::High);
}

static void test_valvula_abierta_pide_nivel_bajo(void) {
    TEST_ASSERT_TRUE(toSignalLevel(ValveState::Open) == SignalLevel::Low);
}

// V-01: ida y vuelta en ambos sentidos.
static void test_conversion_ida_y_vuelta(void) {
    TEST_ASSERT_TRUE(toSignalLevel(toValveState(SignalLevel::Low)) == SignalLevel::Low);
    TEST_ASSERT_TRUE(toSignalLevel(toValveState(SignalLevel::High)) == SignalLevel::High);
    TEST_ASSERT_TRUE(toValveState(toSignalLevel(ValveState::Open)) == ValveState::Open);
    TEST_ASSERT_TRUE(toValveState(toSignalLevel(ValveState::Closed)) == ValveState::Closed);
}

static void test_conversion_desde_byte_crudo(void) {
    TEST_ASSERT_TRUE(levelFromRaw(0) == SignalLevel::Low);
    TEST_ASSERT_TRUE(levelFromRaw(1) == SignalLevel::High);
    TEST_ASSERT_EQUAL_UINT8(0, rawOf(SignalLevel::Low));
    TEST_ASSERT_EQUAL_UINT8(1, rawOf(SignalLevel::High));
}

// La conversion es constexpr: se resuelve en compilacion, coste cero en la placa.
static_assert(toValveState(SignalLevel::High) == ValveState::Closed,
              "la inversion por transistor debe resolverse en compilacion");

void run_valve_types_tests(void) {
    RUN_TEST(test_nivel_alto_es_valvula_cerrada);
    RUN_TEST(test_nivel_bajo_es_valvula_abierta);
    RUN_TEST(test_valvula_cerrada_pide_nivel_alto);
    RUN_TEST(test_valvula_abierta_pide_nivel_bajo);
    RUN_TEST(test_conversion_ida_y_vuelta);
    RUN_TEST(test_conversion_desde_byte_crudo);
}
