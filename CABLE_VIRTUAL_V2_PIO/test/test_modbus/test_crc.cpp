// -----------------------------------------------------------------------------
//  Verificador de redundancia CRC16 Modbus.
//  Cubre: FR-104
//
//  Los vectores no son inventados: el primero es el de referencia del estandar
//  Modbus, y el resto estan calculados y comprobados en
//  contracts/register-map.md seccion 6, de donde se toman literalmente.
// -----------------------------------------------------------------------------
#include <unity.h>

#include "modbus_crc.h"

using wiga::modbus::crc16;

namespace {

// --- Vector de referencia del estandar ---------------------------------------
// Si este falla, la implementacion no es CRC16 Modbus y nada de lo demas vale.
void test_vector_de_referencia_del_estandar(void) {
    const uint8_t frame[] = {0x01, 0x04, 0x00, 0x01, 0x00, 0x01};
    TEST_ASSERT_EQUAL_HEX16(0x0A60, crc16(frame, sizeof(frame)));
}

// --- Vectores del contrato ---------------------------------------------------

void test_peticion_fc03_valvula_3(void) {
    // 01 03 00 03 00 01  ->  CRC lo=74 hi=0A
    const uint8_t frame[] = {0x01, 0x03, 0x00, 0x03, 0x00, 0x01};
    const uint16_t crc = crc16(frame, sizeof(frame));
    TEST_ASSERT_EQUAL_HEX8(0x74, static_cast<uint8_t>(crc & 0xFF));
    TEST_ASSERT_EQUAL_HEX8(0x0A, static_cast<uint8_t>(crc >> 8));
}

void test_respuesta_fc03_valor_uno(void) {
    // 01 03 02 00 01  ->  CRC lo=79 hi=84
    const uint8_t frame[] = {0x01, 0x03, 0x02, 0x00, 0x01};
    const uint16_t crc = crc16(frame, sizeof(frame));
    TEST_ASSERT_EQUAL_HEX8(0x79, static_cast<uint8_t>(crc & 0xFF));
    TEST_ASSERT_EQUAL_HEX8(0x84, static_cast<uint8_t>(crc >> 8));
}

void test_peticion_fc01_24_bobinas(void) {
    // 01 01 00 00 00 18  ->  CRC lo=3C hi=00
    const uint8_t frame[] = {0x01, 0x01, 0x00, 0x00, 0x00, 0x18};
    const uint16_t crc = crc16(frame, sizeof(frame));
    TEST_ASSERT_EQUAL_HEX8(0x3C, static_cast<uint8_t>(crc & 0xFF));
    TEST_ASSERT_EQUAL_HEX8(0x00, static_cast<uint8_t>(crc >> 8));
}

void test_respuesta_fc01_valvulas_0_y_3(void) {
    // 01 01 03 09 00 00  ->  CRC lo=EC hi=4C
    const uint8_t frame[] = {0x01, 0x01, 0x03, 0x09, 0x00, 0x00};
    const uint16_t crc = crc16(frame, sizeof(frame));
    TEST_ASSERT_EQUAL_HEX8(0xEC, static_cast<uint8_t>(crc & 0xFF));
    TEST_ASSERT_EQUAL_HEX8(0x4C, static_cast<uint8_t>(crc >> 8));
}

void test_excepcion_direccion_fuera_de_rango(void) {
    // 01 83 02  ->  CRC lo=C0 hi=F1
    const uint8_t frame[] = {0x01, 0x83, 0x02};
    const uint16_t crc = crc16(frame, sizeof(frame));
    TEST_ASSERT_EQUAL_HEX8(0xC0, static_cast<uint8_t>(crc & 0xFF));
    TEST_ASSERT_EQUAL_HEX8(0xF1, static_cast<uint8_t>(crc >> 8));
}

// --- Propiedades -------------------------------------------------------------

/// Anadir el verificador al final de la trama y recalcular sobre el conjunto da
/// cero. Es la propiedad que permite validar sin separar el CRC del cuerpo.
void test_trama_con_su_verificador_da_cero(void) {
    const uint8_t frame[] = {0x01, 0x03, 0x00, 0x03, 0x00, 0x01, 0x74, 0x0A};
    TEST_ASSERT_EQUAL_HEX16(0x0000, crc16(frame, sizeof(frame)));
}

void test_longitud_cero_devuelve_el_valor_inicial(void) {
    const uint8_t frame[] = {0x00};
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, crc16(frame, 0));
}

/// Un solo bit distinto tiene que cambiar el verificador. Sin esto, una trama
/// corrupta podria pasar por buena.
void test_un_bit_distinto_cambia_el_verificador(void) {
    const uint8_t a[] = {0x01, 0x03, 0x00, 0x03, 0x00, 0x01};
    const uint8_t b[] = {0x01, 0x03, 0x00, 0x03, 0x00, 0x03};
    TEST_ASSERT_NOT_EQUAL(crc16(a, sizeof(a)), crc16(b, sizeof(b)));
}

}  // namespace

void run_crc_tests(void) {
    RUN_TEST(test_vector_de_referencia_del_estandar);
    RUN_TEST(test_peticion_fc03_valvula_3);
    RUN_TEST(test_respuesta_fc03_valor_uno);
    RUN_TEST(test_peticion_fc01_24_bobinas);
    RUN_TEST(test_respuesta_fc01_valvulas_0_y_3);
    RUN_TEST(test_excepcion_direccion_fuera_de_rango);
    RUN_TEST(test_trama_con_su_verificador_da_cero);
    RUN_TEST(test_longitud_cero_devuelve_el_valor_inicial);
    RUN_TEST(test_un_bit_distinto_cambia_el_verificador);
}
