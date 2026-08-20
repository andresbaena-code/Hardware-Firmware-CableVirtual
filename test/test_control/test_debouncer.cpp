// -----------------------------------------------------------------------------
//  Verifica: FR-002  (invariantes D-01, D-02)
//  El firmware base no tenia antirebote: cualquier microcorte del contacto del
//  tablero disparaba una orden latch con su pico de corriente sobre la bateria.
// -----------------------------------------------------------------------------
#include <unity.h>

#include "debouncer.h"

using namespace wiga;

static constexpr uint16_t kWindow = 50;

// D-01: un cambio sostenido durante toda la ventana se acepta.
static void test_cambio_sostenido_se_acepta(void) {
    Debouncer d;
    d.seed(SignalLevel::High, 1000);

    TEST_ASSERT_FALSE(d.update(SignalLevel::Low, 1000, kWindow));  // aparece candidato
    TEST_ASSERT_FALSE(d.update(SignalLevel::Low, 1049, kWindow));  // aun no cumple
    TEST_ASSERT_TRUE(d.update(SignalLevel::Low, 1050, kWindow));   // ventana completa
    TEST_ASSERT_TRUE(d.stable() == SignalLevel::Low);
}

// D-01: una oscilacion mas corta que la ventana se rechaza y reinicia el conteo.
static void test_oscilacion_corta_se_rechaza(void) {
    Debouncer d;
    d.seed(SignalLevel::High, 0);

    TEST_ASSERT_FALSE(d.update(SignalLevel::Low, 0, kWindow));
    TEST_ASSERT_FALSE(d.update(SignalLevel::Low, 30, kWindow));
    TEST_ASSERT_FALSE(d.update(SignalLevel::High, 31, kWindow));  // rebota de vuelta
    TEST_ASSERT_FALSE(d.update(SignalLevel::Low, 32, kWindow));   // vuelve: reinicia
    TEST_ASSERT_FALSE(d.update(SignalLevel::Low, 81, kWindow));   // 49 ms desde 32
    TEST_ASSERT_TRUE(d.update(SignalLevel::Low, 82, kWindow));    // 50 ms desde 32
    TEST_ASSERT_TRUE(d.stable() == SignalLevel::Low);
}

// Un tren de rebotes no debe producir NINGUNA transicion mientras dure.
static void test_tren_de_rebotes_no_produce_transicion(void) {
    Debouncer d;
    d.seed(SignalLevel::High, 0);

    uint32_t t = 0;
    for (int i = 0; i < 20; ++i) {
        const SignalLevel level = (i % 2 == 0) ? SignalLevel::Low : SignalLevel::High;
        t += 10;  // conmuta cada 10 ms, por debajo de la ventana de 50 ms
        TEST_ASSERT_FALSE(d.update(level, t, kWindow));
    }
    TEST_ASSERT_TRUE(d.stable() == SignalLevel::High);
}

// Mantener el nivel ya estable nunca genera transicion.
static void test_nivel_estable_no_genera_transicion(void) {
    Debouncer d;
    d.seed(SignalLevel::High, 0);
    for (uint32_t t = 0; t < 1000; t += 15) {
        TEST_ASSERT_FALSE(d.update(SignalLevel::High, t, kWindow));
    }
}

// seed() fija el nivel inicial sin generar transicion (arranque, FR-010).
static void test_seed_no_genera_transicion(void) {
    Debouncer d;
    d.seed(SignalLevel::Low, 500);
    TEST_ASSERT_TRUE(d.stable() == SignalLevel::Low);
    TEST_ASSERT_FALSE(d.update(SignalLevel::Low, 600, kWindow));
}

void run_debouncer_tests(void) {
    RUN_TEST(test_cambio_sostenido_se_acepta);
    RUN_TEST(test_oscilacion_corta_se_rechaza);
    RUN_TEST(test_tren_de_rebotes_no_produce_transicion);
    RUN_TEST(test_nivel_estable_no_genera_transicion);
    RUN_TEST(test_seed_no_genera_transicion);
}
