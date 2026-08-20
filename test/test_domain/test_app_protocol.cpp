// -----------------------------------------------------------------------------
//  Verifica: FR-015, FR-016  (contracts/application-protocol.md, P-01, P-02)
//  Los ejemplos normativos vienen del contrato y estan calculados a mano.
// -----------------------------------------------------------------------------
#include <unity.h>

#include "app_protocol.h"

using namespace wiga;
using namespace wiga::proto;

// --- FR-015: codificacion de la orden ---------------------------------------

// Ejemplo normativo 1: abrir la valvula 3 -> AA 14 03 00 3E
static void test_ejemplo_normativo_abrir_valvula_3(void) {
    ValveCommand cmd;
    cmd.nodeAddressLsb = 0x42878FDBUL;
    cmd.valveId        = 3;
    cmd.level          = SignalLevel::Low;   // nivel bajo = abrir

    uint8_t buffer[8] = {0};
    TEST_ASSERT_EQUAL_UINT8(5, encodeCommand(cmd, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_HEX8(0xAA, buffer[0]);
    TEST_ASSERT_EQUAL_HEX8(0x14, buffer[1]);
    TEST_ASSERT_EQUAL_HEX8(0x03, buffer[2]);
    TEST_ASSERT_EQUAL_HEX8(0x00, buffer[3]);
    TEST_ASSERT_EQUAL_HEX8(0x3E, buffer[4]);
}

// Ejemplo normativo 2: cerrar la valvula 7 -> AA 14 07 01 39
static void test_ejemplo_normativo_cerrar_valvula_7(void) {
    ValveCommand cmd;
    cmd.nodeAddressLsb = 0x41C3F549UL;
    cmd.valveId        = 7;
    cmd.level          = SignalLevel::High;  // nivel alto = cerrar

    uint8_t buffer[8] = {0};
    TEST_ASSERT_EQUAL_UINT8(5, encodeCommand(cmd, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_HEX8(0xAA, buffer[0]);
    TEST_ASSERT_EQUAL_HEX8(0x14, buffer[1]);
    TEST_ASSERT_EQUAL_HEX8(0x07, buffer[2]);
    TEST_ASSERT_EQUAL_HEX8(0x01, buffer[3]);
    TEST_ASSERT_EQUAL_HEX8(0x39, buffer[4]);
}

static void test_encode_rechaza_buffer_insuficiente(void) {
    ValveCommand cmd;
    cmd.nodeAddressLsb = 1;
    cmd.valveId        = 0;
    cmd.level          = SignalLevel::Low;
    uint8_t small[4] = {0};
    TEST_ASSERT_EQUAL_UINT8(0, encodeCommand(cmd, small, sizeof(small)));
}

static void test_encode_rechaza_valve_id_fuera_de_rango(void) {
    ValveCommand cmd;
    cmd.nodeAddressLsb = 1;
    cmd.valveId        = 8;  // maximo valido es 7
    cmd.level          = SignalLevel::Low;
    uint8_t buffer[8] = {0};
    TEST_ASSERT_EQUAL_UINT8(0, encodeCommand(cmd, buffer, sizeof(buffer)));
}

// --- FR-016: validacion en recepcion (RX-01..RX-04) -------------------------

static void test_decode_acepta_trama_de_5_bytes(void) {
    const uint8_t frame[] = {0xAA, 0x14, 0x03, 0x00, 0x3E};
    ValveReport report;
    TEST_ASSERT_TRUE(decodeReport(frame, 5, 0x42878FDBUL, &report) == DecodeError::None);
    TEST_ASSERT_EQUAL_HEX32(0x42878FDBUL, report.nodeAddressLsb);
    TEST_ASSERT_EQUAL_UINT8(3, report.valveId);
    TEST_ASSERT_TRUE(report.level == SignalLevel::Low);
}

// RX-03: tolerancia deliberada con nodos de firmware antiguo.
static void test_decode_acepta_trama_de_4_bytes_sin_verificador(void) {
    const uint8_t frame[] = {0xAA, 0x14, 0x02, 0x01};
    ValveReport report;
    TEST_ASSERT_TRUE(decodeReport(frame, 4, 0x41C3F595UL, &report) == DecodeError::None);
    TEST_ASSERT_EQUAL_UINT8(2, report.valveId);
    TEST_ASSERT_TRUE(report.level == SignalLevel::High);
}

// RX-01 / P-02: el firmware base leia data[0..3] sin comprobar la longitud.
static void test_decode_rechaza_trama_corta(void) {
    const uint8_t frame[] = {0xAA, 0x14, 0x00};
    ValveReport report;
    TEST_ASSERT_TRUE(decodeReport(frame, 3, 1, &report) == DecodeError::TooShort);
}

static void test_decode_rechaza_encabezado_invalido(void) {
    const uint8_t frame[] = {0xBB, 0x14, 0x00, 0x00, 0x00};
    ValveReport report;
    TEST_ASSERT_TRUE(decodeReport(frame, 5, 1, &report) == DecodeError::BadHeader);
}

static void test_decode_rechaza_opcode_invalido(void) {
    const uint8_t frame[] = {0xAA, 0x15, 0x00, 0x00, 0x00};
    ValveReport report;
    TEST_ASSERT_TRUE(decodeReport(frame, 5, 1, &report) == DecodeError::BadOpcode);
}

// RX-03
static void test_decode_rechaza_verificador_incorrecto(void) {
    const uint8_t frame[] = {0xAA, 0x14, 0x03, 0x00, 0x3F};  // deberia ser 0x3E
    ValveReport report;
    TEST_ASSERT_TRUE(decodeReport(frame, 5, 1, &report) == DecodeError::BadChecksum);
}

// RX-04
static void test_decode_rechaza_nivel_fuera_de_rango(void) {
    const uint8_t level = 0x02;
    const uint8_t check = static_cast<uint8_t>(0xFF - static_cast<uint8_t>(0xAA + 0x14 + 0x00 + level));
    const uint8_t frame[] = {0xAA, 0x14, 0x00, level, check};
    ValveReport report;
    TEST_ASSERT_TRUE(decodeReport(frame, 5, 1, &report) == DecodeError::BadLevel);
}

static void test_decode_rechaza_valve_id_fuera_de_rango(void) {
    const uint8_t id = 0x09;
    const uint8_t check = static_cast<uint8_t>(0xFF - static_cast<uint8_t>(0xAA + 0x14 + id + 0x00));
    const uint8_t frame[] = {0xAA, 0x14, id, 0x00, check};
    ValveReport report;
    TEST_ASSERT_TRUE(decodeReport(frame, 5, 1, &report) == DecodeError::BadValveId);
}

static void test_decode_rechaza_punteros_nulos(void) {
    ValveReport report;
    TEST_ASSERT_TRUE(decodeReport(nullptr, 5, 1, &report) == DecodeError::TooShort);
    const uint8_t frame[] = {0xAA, 0x14, 0x00, 0x00, 0x41};
    TEST_ASSERT_TRUE(decodeReport(frame, 5, 1, nullptr) == DecodeError::TooShort);
}

// El verificador debe ser calculable en compilacion.
static_assert(checksumOf(0xAA, 0x14, 0x03, 0x00) == 0x3E, "ejemplo normativo 1");
static_assert(checksumOf(0xAA, 0x14, 0x07, 0x01) == 0x39, "ejemplo normativo 2");

void run_app_protocol_tests(void) {
    RUN_TEST(test_ejemplo_normativo_abrir_valvula_3);
    RUN_TEST(test_ejemplo_normativo_cerrar_valvula_7);
    RUN_TEST(test_encode_rechaza_buffer_insuficiente);
    RUN_TEST(test_encode_rechaza_valve_id_fuera_de_rango);
    RUN_TEST(test_decode_acepta_trama_de_5_bytes);
    RUN_TEST(test_decode_acepta_trama_de_4_bytes_sin_verificador);
    RUN_TEST(test_decode_rechaza_trama_corta);
    RUN_TEST(test_decode_rechaza_encabezado_invalido);
    RUN_TEST(test_decode_rechaza_opcode_invalido);
    RUN_TEST(test_decode_rechaza_verificador_incorrecto);
    RUN_TEST(test_decode_rechaza_nivel_fuera_de_rango);
    RUN_TEST(test_decode_rechaza_valve_id_fuera_de_rango);
    RUN_TEST(test_decode_rechaza_punteros_nulos);
}
