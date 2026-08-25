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
    // No se compara contra un literal: el conteo DEBE salir del arreglo.
    TEST_ASSERT_EQUAL_UINT8(detail::kCount, kValveCount);
    TEST_ASSERT_TRUE(kValveCount > 0);
}

// --- RX-05: resolucion de valvula por (direccion, id) -----------------------
// Estas comprobaciones son independientes de la finca: recorren el mapa que
// este configurado y verifican que la busqueda lo resuelve.
static void test_toda_valvula_del_mapa_es_encontrable(void) {
    for (uint8_t i = 0; i < kValveCount; ++i) {
        const ValveMapping& m = valveMapping(i);
        TEST_ASSERT_EQUAL_UINT8(i, findValveIndex(m.nodeAddressLsb, m.valveId));
    }
}

static void test_rechaza_nodo_no_configurado(void) {
    TEST_ASSERT_EQUAL_UINT8(kNoValve, findValveIndex(0xDEADBEEFUL, 0));
}

static void test_rechaza_id_no_configurado_en_nodo_conocido(void) {
    // Un id que no existe en un nodo que si existe.
    const uint32_t nodo = valveMapping(0).nodeAddressLsb;
    uint8_t idLibre = 0xFF;
    for (uint8_t candidato = 0; candidato <= 7; ++candidato) {
        if (findValveIndex(nodo, candidato) == kNoValve) {
            idLibre = candidato;
            break;
        }
    }
    if (idLibre == 0xFF) {
        TEST_IGNORE_MESSAGE("el primer nodo usa sus 8 ids: no hay id libre que probar");
        return;
    }
    TEST_ASSERT_EQUAL_UINT8(kNoValve, findValveIndex(nodo, idLibre));
}

// -----------------------------------------------------------------------------
//  UNICA prueba atada a la instalacion concreta.
//
//  Fija el mapa de la finca actual, para que un cambio accidental salte aqui.
//  AL CONFIGURAR UNA FINCA NUEVA hay que actualizar esta prueba junto con
//  installation_map.h. Es la excepcion deliberada al criterio SC-004, y esta
//  aislada aqui a proposito: ninguna otra prueba del proyecto conoce las
//  direcciones de la instalacion.
// -----------------------------------------------------------------------------
static void test_mapa_de_la_instalacion_vigente(void) {
    TEST_ASSERT_EQUAL_UINT8(24, kValveCount);
    TEST_ASSERT_EQUAL_UINT8(0,  findValveIndex(0x42878FDBUL, 0));
    TEST_ASSERT_EQUAL_UINT8(23, findValveIndex(0x41C3F54FUL, 7));
    // Las filas 18-22 comparten nodo con ids 2..6 (pendiente de verificar
    // contra el plano, tarea T055).
    TEST_ASSERT_EQUAL_UINT8(18, findValveIndex(0x41C3F595UL, 2));
    TEST_ASSERT_EQUAL_UINT8(22, findValveIndex(0x41C3F595UL, 6));
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
    RUN_TEST(test_toda_valvula_del_mapa_es_encontrable);
    RUN_TEST(test_rechaza_nodo_no_configurado);
    RUN_TEST(test_rechaza_id_no_configurado_en_nodo_conocido);
    RUN_TEST(test_mapa_de_la_instalacion_vigente);
}
