// -----------------------------------------------------------------------------
//  Aritmetica temporal al desbordar millis().
//  Cubre: FR-116, NFR-002  (invariante R-04, research.md R-005)
//
//  millis() desborda cada ~49,7 dias. Con NFR-002 exigiendo 90 dias continuos,
//  el desbordamiento ocurre SIEMPRE dentro del periodo de aceptacion. Estas
//  pruebas lo fuerzan en milisegundos en lugar de esperar 49 dias.
//
//  Las tres pruebas que verificaban el desbordamiento de la espera de reintento,
//  del reposo prolongado y de la ventana de exclusion se retiraron junto con
//  esos mecanismos: al no existir confirmacion del campo, no hay nada que
//  reintentar. Su equivalente vigente es la prueba de extremo a extremo
//  test_el_desbordamiento_del_reloj_no_altera_el_riego, en test_integration.
// -----------------------------------------------------------------------------
#include <unity.h>

#include "time_math.h"

using namespace wiga;

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

    TEST_ASSERT_TRUE(now >= deadline);          // lo que diria la version ingenua
    TEST_ASSERT_FALSE(reached(now, deadline));  // lo correcto: aun no vence
}

void run_time_wraparound_tests(void) {
    RUN_TEST(test_elapsed_cruza_el_desbordamiento);
    RUN_TEST(test_reached_cruza_el_desbordamiento);
    RUN_TEST(test_la_comparacion_ingenua_fallaria_aqui);
}
