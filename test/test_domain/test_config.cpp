// -----------------------------------------------------------------------------
//  Verifica: FR-024, FR-025  (contracts/installation-map.md, MAP-V1..MAP-V6)
//  Las funciones constexpr de deteccion son las que hacen que un mapa mal
//  configurado no llegue nunca a campo. Se prueban sobre mapas construidos a
//  proposito con y sin colisiones.
// -----------------------------------------------------------------------------
#include <unity.h>

#include "installation_map.h"

using namespace wiga;
using namespace wiga::config;

// --- Mapas de ejemplo --------------------------------------------------------
constexpr ValveMapping kSano[] = {
    {0x11111111UL, 0, 10},
    {0x11111111UL, 1, 11},
    {0x22222222UL, 0, 12},
};

constexpr ValveMapping kPinDuplicado[] = {
    {0x11111111UL, 0, 10},
    {0x22222222UL, 1, 10},   // mismo pin que la fila 0
};

constexpr ValveMapping kDestinoDuplicado[] = {
    {0x11111111UL, 3, 10},
    {0x11111111UL, 3, 11},   // mismo (direccion, id) que la fila 0
};

constexpr ValveMapping kIdFueraDeRango[] = {
    {0x11111111UL, 8, 10},
};

constexpr ValveMapping kDireccionSinAsignar[] = {
    {0UL, 0, 10},
};

// --- MAP-V3 ------------------------------------------------------------------
static void test_detecta_pin_duplicado(void) {
    TEST_ASSERT_TRUE(detail::hasDuplicatePin(kPinDuplicado, 2));
    TEST_ASSERT_FALSE(detail::hasDuplicatePin(kSano, 3));
}

// --- MAP-V4 ------------------------------------------------------------------
static void test_detecta_par_direccion_id_duplicado(void) {
    TEST_ASSERT_TRUE(detail::hasDuplicateTarget(kDestinoDuplicado, 2));
    TEST_ASSERT_FALSE(detail::hasDuplicateTarget(kSano, 3));
}

// La misma direccion con distinto id es legitima: un nodo opera hasta 8 valvulas.
static void test_misma_direccion_con_distinto_id_es_valida(void) {
    TEST_ASSERT_FALSE(detail::hasDuplicateTarget(kSano, 3));
}

// --- MAP-V5 ------------------------------------------------------------------
static void test_detecta_valve_id_fuera_de_rango(void) {
    TEST_ASSERT_FALSE(detail::allValveIdsInRange(kIdFueraDeRango, 1));
    TEST_ASSERT_TRUE(detail::allValveIdsInRange(kSano, 3));
}

// --- MAP-V6 ------------------------------------------------------------------
static void test_detecta_direccion_sin_asignar(void) {
    TEST_ASSERT_FALSE(detail::allAddressesAssigned(kDireccionSinAsignar, 1));
    TEST_ASSERT_TRUE(detail::allAddressesAssigned(kSano, 3));
}

// --- FR-024: el conteo se deriva del mapa -----------------------------------
static void test_conteo_derivado_del_mapa(void) {
    TEST_ASSERT_EQUAL_UINT8(24, kValveCount);
}

// --- RX-05: resolucion de valvula por (direccion, id) -----------------------
static void test_encuentra_valvula_configurada(void) {
    // Fila 0 del mapa de la instalacion.
    TEST_ASSERT_EQUAL_UINT8(0, findValveIndex(0x42878FDBUL, 0));
    // Fila 23.
    TEST_ASSERT_EQUAL_UINT8(23, findValveIndex(0x41C3F54FUL, 7));
}

static void test_rechaza_nodo_no_configurado(void) {
    TEST_ASSERT_EQUAL_UINT8(kNoValve, findValveIndex(0xDEADBEEFUL, 0));
}

static void test_rechaza_id_no_configurado_en_nodo_conocido(void) {
    // El nodo 0x42878FDB solo tiene los ids 0..3 en este mapa.
    TEST_ASSERT_EQUAL_UINT8(kNoValve, findValveIndex(0x42878FDBUL, 7));
}

// Las funciones deben ser utilizables en contexto constexpr: es lo que permite
// que el error se detecte al compilar y no en campo.
static_assert(!detail::hasDuplicatePin(kSano, 3), "el mapa sano no tiene pines repetidos");
static_assert(detail::hasDuplicatePin(kPinDuplicado, 2), "debe detectar el pin repetido");
static_assert(detail::hasDuplicateTarget(kDestinoDuplicado, 2), "debe detectar el destino repetido");

void run_config_tests(void) {
    RUN_TEST(test_detecta_pin_duplicado);
    RUN_TEST(test_detecta_par_direccion_id_duplicado);
    RUN_TEST(test_misma_direccion_con_distinto_id_es_valida);
    RUN_TEST(test_detecta_valve_id_fuera_de_rango);
    RUN_TEST(test_detecta_direccion_sin_asignar);
    RUN_TEST(test_conteo_derivado_del_mapa);
    RUN_TEST(test_encuentra_valvula_configurada);
    RUN_TEST(test_rechaza_nodo_no_configurado);
    RUN_TEST(test_rechaza_id_no_configurado_en_nodo_conocido);
}
