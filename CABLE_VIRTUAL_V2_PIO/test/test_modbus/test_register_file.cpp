// -----------------------------------------------------------------------------
//  Banco de registros: acceso acotado y geometria derivada.
//  Cubre: FR-107, FR-109, FR-118
//
//  El grupo de acceso fuera de rango es regresion directa del defecto verificado
//  en la implementacion anterior, donde un indice sin acotar leia fuera del
//  buffer de pila.
// -----------------------------------------------------------------------------
#include <unity.h>

#include "register_file.h"
#include "timing_config.h"

using wiga::modbus::RegisterFile;
namespace config = wiga::config;

namespace {

// --- Geometria derivada ------------------------------------------------------

void test_el_banco_cubre_las_24_valvulas_y_el_diagnostico(void) {
    TEST_ASSERT_EQUAL_UINT16(24, config::kValveCount);
    TEST_ASSERT_EQUAL_UINT16(32, RegisterFile::count());
    TEST_ASSERT_EQUAL_UINT16(24, config::kDiagRegisterBase);
}

void test_arranca_todo_a_cero(void) {
    RegisterFile file;
    for (uint16_t i = 0; i < RegisterFile::count(); ++i) {
        TEST_ASSERT_EQUAL_UINT16(0, file.get(i));
    }
}

// --- Lectura y escritura -----------------------------------------------------

void test_escribe_y_lee_cada_valvula(void) {
    RegisterFile file;
    for (uint16_t i = 0; i < config::kValveCount; ++i) {
        file.set(config::kValveRegisterBase + i, 1);
    }
    for (uint16_t i = 0; i < config::kValveCount; ++i) {
        TEST_ASSERT_EQUAL_UINT16(1, file.get(config::kValveRegisterBase + i));
    }
}

/// FR-118: las 24 valvulas deben ser observables. Regresion del truncamiento
/// que dejaba 12 de 24 inalcanzables.
void test_las_24_valvulas_son_direccionables_de_forma_independiente(void) {
    RegisterFile file;
    for (uint16_t i = 0; i < config::kValveCount; ++i) {
        file.set(config::kValveRegisterBase + i, static_cast<uint16_t>(i + 100));
    }
    for (uint16_t i = 0; i < config::kValveCount; ++i) {
        TEST_ASSERT_EQUAL_UINT16(i + 100, file.get(config::kValveRegisterBase + i));
    }
}

void test_la_vista_de_bit_sigue_al_registro(void) {
    RegisterFile file;
    file.set(3, 1);
    TEST_ASSERT_TRUE(file.coilBit(3));
    TEST_ASSERT_FALSE(file.coilBit(4));
    file.set(3, 0);
    TEST_ASSERT_FALSE(file.coilBit(3));
}

// --- Acceso fuera de rango: nunca desborda -----------------------------------

void test_leer_fuera_de_rango_devuelve_cero(void) {
    RegisterFile file;
    TEST_ASSERT_EQUAL_UINT16(0, file.get(RegisterFile::count()));
    TEST_ASSERT_EQUAL_UINT16(0, file.get(1000));
    TEST_ASSERT_EQUAL_UINT16(0, file.get(0xFFFF));
}

void test_escribir_fuera_de_rango_no_altera_el_banco(void) {
    RegisterFile file;
    file.set(RegisterFile::count(), 0xBEEF);
    file.set(0xFFFF, 0xBEEF);
    for (uint16_t i = 0; i < RegisterFile::count(); ++i) {
        TEST_ASSERT_EQUAL_UINT16(0, file.get(i));
    }
}

void test_la_vista_de_bit_fuera_de_rango_es_falsa(void) {
    RegisterFile file;
    TEST_ASSERT_FALSE(file.coilBit(RegisterFile::count()));
    TEST_ASSERT_FALSE(file.coilBit(0xFFFF));
}

// --- Validacion de rangos: la aritmetica no puede dar la vuelta --------------

void test_acepta_los_rangos_que_caben(void) {
    TEST_ASSERT_TRUE(RegisterFile::rangeFits(0, 1));
    TEST_ASSERT_TRUE(RegisterFile::rangeFits(0, RegisterFile::count()));
    TEST_ASSERT_TRUE(RegisterFile::rangeFits(RegisterFile::count() - 1, 1));
}

void test_rechaza_cantidad_cero(void) {
    TEST_ASSERT_FALSE(RegisterFile::rangeFits(0, 0));
}

void test_rechaza_los_rangos_que_se_pasan(void) {
    TEST_ASSERT_FALSE(RegisterFile::rangeFits(0, RegisterFile::count() + 1));
    TEST_ASSERT_FALSE(RegisterFile::rangeFits(RegisterFile::count(), 1));
    TEST_ASSERT_FALSE(RegisterFile::rangeFits(RegisterFile::count() - 1, 2));
}

/// El caso que la aritmetica de 16 bits dejaria pasar: start + quantity da la
/// vuelta y parece un rango pequeno. Es la misma familia de defecto que el
/// (uint8_t)(3 + byteCount + 2) de la implementacion anterior.
void test_rechaza_el_rango_que_desbordaria_en_16_bits(void) {
    TEST_ASSERT_FALSE(RegisterFile::rangeFits(0xFFFF, 1));
    TEST_ASSERT_FALSE(RegisterFile::rangeFits(0xFFF0, 0x0020));
    TEST_ASSERT_FALSE(RegisterFile::rangeFits(0x8000, 0x8000));
}

}  // namespace

void run_register_file_tests(void) {
    RUN_TEST(test_el_banco_cubre_las_24_valvulas_y_el_diagnostico);
    RUN_TEST(test_arranca_todo_a_cero);
    RUN_TEST(test_escribe_y_lee_cada_valvula);
    RUN_TEST(test_las_24_valvulas_son_direccionables_de_forma_independiente);
    RUN_TEST(test_la_vista_de_bit_sigue_al_registro);
    RUN_TEST(test_leer_fuera_de_rango_devuelve_cero);
    RUN_TEST(test_escribir_fuera_de_rango_no_altera_el_banco);
    RUN_TEST(test_la_vista_de_bit_fuera_de_rango_es_falsa);
    RUN_TEST(test_acepta_los_rangos_que_caben);
    RUN_TEST(test_rechaza_cantidad_cero);
    RUN_TEST(test_rechaza_los_rangos_que_se_pasan);
    RUN_TEST(test_rechaza_el_rango_que_desbordaria_en_16_bits);
}
