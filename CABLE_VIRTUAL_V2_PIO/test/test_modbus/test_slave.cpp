// -----------------------------------------------------------------------------
//  Esclavo Modbus RTU: tramado, direccionamiento, lecturas y excepciones.
//  Cubre: FR-101, FR-103, FR-104, FR-105, FR-106, FR-109, FR-113, FR-118
//
//  Los vectores estan tomados literalmente de contracts/register-map.md seccion
//  6 y contracts/modbus-slave.md seccion 7, donde se calcularon y comprobaron.
// -----------------------------------------------------------------------------
#include <unity.h>

#include "modbus_slave.h"
#include "register_file.h"
#include "timing_config.h"

using wiga::modbus::ModbusSlave;
using wiga::modbus::RegisterFile;
namespace config = wiga::config;

namespace {

constexpr uint8_t  kId       = 1;
constexpr uint16_t kSilence  = 4;
constexpr uint32_t kT0       = 100000;  // lejos de cero, para no enmascarar bugs

/// Entrega una trama completa sin silencio entre bytes.
void feedFrame(ModbusSlave& slave, const uint8_t* frame, uint8_t length,
               uint32_t atMs = kT0) {
    for (uint8_t i = 0; i < length; ++i) {
        slave.onByte(frame[i], atMs);
    }
}

void assertResponse(ModbusSlave& slave, const uint8_t* expected, uint8_t length) {
    TEST_ASSERT_TRUE_MESSAGE(slave.hasResponse(), "se esperaba respuesta");
    TEST_ASSERT_EQUAL_UINT8(length, slave.responseLength());
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, slave.response(), length);
}

// =============================================================================
//  Lecturas de registro: FC03 y FC04
// =============================================================================

/// Vector del contrato: valvula 3 pidiendo riego.
void test_fc03_devuelve_el_vector_del_contrato(void) {
    RegisterFile file;
    file.set(3, 1);
    ModbusSlave slave(file, kId, kSilence);

    const uint8_t request[]  = {0x01, 0x03, 0x00, 0x03, 0x00, 0x01, 0x74, 0x0A};
    const uint8_t expected[] = {0x01, 0x03, 0x02, 0x00, 0x01, 0x79, 0x84};

    feedFrame(slave, request, sizeof(request));
    assertResponse(slave, expected, sizeof(expected));
}

void test_fc04_devuelve_lo_mismo_que_fc03(void) {
    RegisterFile file;
    file.set(3, 1);
    ModbusSlave slave(file, kId, kSilence);

    // 01 04 00 03 00 01 -> CRC C1 CA
    const uint8_t request[] = {0x01, 0x04, 0x00, 0x03, 0x00, 0x01, 0xC1, 0xCA};
    feedFrame(slave, request, sizeof(request));

    TEST_ASSERT_TRUE(slave.hasResponse());
    TEST_ASSERT_EQUAL_HEX8(0x04, slave.response()[1]);
    TEST_ASSERT_EQUAL_HEX8(0x02, slave.response()[2]);
    TEST_ASSERT_EQUAL_HEX8(0x00, slave.response()[3]);
    TEST_ASSERT_EQUAL_HEX8(0x01, slave.response()[4]);
}

/// FR-118: las 24 valvulas observables de una sola lectura. Regresion del
/// truncamiento que dejaba 12 de 24 inalcanzables.
void test_lee_las_24_valvulas_de_una_vez(void) {
    RegisterFile file;
    for (uint16_t i = 0; i < config::kValveCount; ++i) file.set(i, 1);
    ModbusSlave slave(file, kId, kSilence);

    // 01 03 00 00 00 18 -> CRC 45 C0
    const uint8_t request[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x18, 0x45, 0xC0};
    feedFrame(slave, request, sizeof(request));

    TEST_ASSERT_TRUE(slave.hasResponse());
    TEST_ASSERT_EQUAL_UINT8(48, slave.response()[2]);        // 24 * 2 bytes
    TEST_ASSERT_EQUAL_UINT8(53, slave.responseLength());     // 3 + 48 + 2
    for (uint8_t i = 0; i < 24; ++i) {
        TEST_ASSERT_EQUAL_HEX8(0x00, slave.response()[3 + i * 2]);
        TEST_ASSERT_EQUAL_HEX8(0x01, slave.response()[3 + i * 2 + 1]);
    }
}

// =============================================================================
//  Lecturas de bit: FC01 y FC02
// =============================================================================

/// Vector del contrato: 24 bobinas con las valvulas 0 y 3 activas.
void test_fc01_devuelve_el_vector_del_contrato(void) {
    RegisterFile file;
    file.set(0, 1);
    file.set(3, 1);
    ModbusSlave slave(file, kId, kSilence);

    const uint8_t request[]  = {0x01, 0x01, 0x00, 0x00, 0x00, 0x18, 0x3C, 0x00};
    const uint8_t expected[] = {0x01, 0x01, 0x03, 0x09, 0x00, 0x00, 0xEC, 0x4C};

    feedFrame(slave, request, sizeof(request));
    assertResponse(slave, expected, sizeof(expected));
}

void test_fc02_devuelve_lo_mismo_que_fc01(void) {
    RegisterFile file;
    file.set(0, 1);
    file.set(3, 1);
    ModbusSlave slave(file, kId, kSilence);

    // 01 02 00 00 00 18 -> CRC 78 00
    const uint8_t request[] = {0x01, 0x02, 0x00, 0x00, 0x00, 0x18, 0x78, 0x00};
    feedFrame(slave, request, sizeof(request));

    TEST_ASSERT_TRUE(slave.hasResponse());
    TEST_ASSERT_EQUAL_HEX8(0x02, slave.response()[1]);
    TEST_ASSERT_EQUAL_HEX8(0x03, slave.response()[2]);
    TEST_ASSERT_EQUAL_HEX8(0x09, slave.response()[3]);
}

// =============================================================================
//  Direccionamiento y verificador: el esclavo calla (MB-01, MB-02)
// =============================================================================

/// FR-103: una peticion para otro esclavo produce SILENCIO ABSOLUTO y no altera
/// ningun contador ni estado.
void test_peticion_a_otro_esclavo_produce_silencio(void) {
    RegisterFile file;
    ModbusSlave slave(file, kId, kSilence);

    // Misma peticion pero dirigida al esclavo 2: 02 03 00 00 00 01 -> CRC 84 39
    const uint8_t request[] = {0x02, 0x03, 0x00, 0x00, 0x00, 0x01, 0x84, 0x39};
    feedFrame(slave, request, sizeof(request));

    TEST_ASSERT_FALSE(slave.hasResponse());
    TEST_ASSERT_EQUAL_UINT16(0, slave.stats().validRequests);
    TEST_ASSERT_EQUAL_UINT16(0, slave.stats().crcErrors);
    TEST_ASSERT_FALSE(slave.everServed());
}

/// FR-104: verificador incorrecto -> silencio y contador.
void test_verificador_incorrecto_produce_silencio_y_cuenta(void) {
    RegisterFile file;
    ModbusSlave slave(file, kId, kSilence);

    const uint8_t request[] = {0x01, 0x03, 0x00, 0x03, 0x00, 0x01, 0x00, 0x00};
    feedFrame(slave, request, sizeof(request));

    TEST_ASSERT_FALSE(slave.hasResponse());
    TEST_ASSERT_EQUAL_UINT16(1, slave.stats().crcErrors);
    TEST_ASSERT_EQUAL_UINT16(0, slave.stats().validRequests);
}

// =============================================================================
//  Excepciones (MB-04, MB-05, MB-06)
// =============================================================================

/// Vector del contrato: direccion 32, fuera del mapa de 32 registros.
void test_direccion_fuera_del_mapa_da_excepcion_02(void) {
    RegisterFile file;
    ModbusSlave slave(file, kId, kSilence);

    const uint8_t request[]  = {0x01, 0x03, 0x00, 0x20, 0x00, 0x01, 0x85, 0xC0};
    const uint8_t expected[] = {0x01, 0x83, 0x02, 0xC0, 0xF1};

    feedFrame(slave, request, sizeof(request));
    assertResponse(slave, expected, sizeof(expected));
}

void test_rango_que_se_pasa_del_mapa_da_excepcion_02(void) {
    RegisterFile file;
    ModbusSlave slave(file, kId, kSilence);

    // Empieza en 30 y pide 5: 30+5 = 35 > 32. 01 03 00 1E 00 05 -> CRC E5 CF
    const uint8_t request[] = {0x01, 0x03, 0x00, 0x1E, 0x00, 0x05, 0xE5, 0xCF};
    feedFrame(slave, request, sizeof(request));

    TEST_ASSERT_TRUE(slave.hasResponse());
    TEST_ASSERT_EQUAL_HEX8(0x83, slave.response()[1]);
    TEST_ASSERT_EQUAL_HEX8(0x02, slave.response()[2]);
}

void test_cantidad_cero_da_excepcion_03(void) {
    RegisterFile file;
    ModbusSlave slave(file, kId, kSilence);

    // 01 03 00 00 00 00 -> CRC 45 CA
    const uint8_t request[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x00, 0x45, 0xCA};
    feedFrame(slave, request, sizeof(request));

    TEST_ASSERT_TRUE(slave.hasResponse());
    TEST_ASSERT_EQUAL_HEX8(0x83, slave.response()[1]);
    TEST_ASSERT_EQUAL_HEX8(0x03, slave.response()[2]);
}

/// MB-06: toda escritura se rechaza. El tablero es la unica fuente de verdad, y
/// ninguna trama del bus puede alterar el estado pedido.
void test_escritura_de_un_registro_da_excepcion_01(void) {
    RegisterFile file;
    ModbusSlave slave(file, kId, kSilence);

    // FC06 escribir registro 0 = 1: 01 06 00 00 00 01 -> CRC 48 0A
    const uint8_t request[] = {0x01, 0x06, 0x00, 0x00, 0x00, 0x01, 0x48, 0x0A};
    feedFrame(slave, request, sizeof(request));

    TEST_ASSERT_TRUE(slave.hasResponse());
    TEST_ASSERT_EQUAL_HEX8(0x86, slave.response()[1]);
    TEST_ASSERT_EQUAL_HEX8(0x01, slave.response()[2]);
    TEST_ASSERT_EQUAL_UINT16(0, file.get(0));  // el banco no se toco
}

void test_escritura_de_una_bobina_da_excepcion_01(void) {
    RegisterFile file;
    ModbusSlave slave(file, kId, kSilence);

    // FC05 escribir bobina 0 = ON: 01 05 00 00 FF 00 -> CRC 8C 3A
    const uint8_t request[] = {0x01, 0x05, 0x00, 0x00, 0xFF, 0x00, 0x8C, 0x3A};
    feedFrame(slave, request, sizeof(request));

    TEST_ASSERT_TRUE(slave.hasResponse());
    TEST_ASSERT_EQUAL_HEX8(0x85, slave.response()[1]);
    TEST_ASSERT_EQUAL_HEX8(0x01, slave.response()[2]);
    TEST_ASSERT_EQUAL_UINT16(0, file.get(0));
}

void test_funcion_desconocida_da_excepcion_01(void) {
    RegisterFile file;
    ModbusSlave slave(file, kId, kSilence);

    // FC42, inexistente: 01 2A 00 00 00 01 -> CRC D9 CC
    const uint8_t request[] = {0x01, 0x2A, 0x00, 0x00, 0x00, 0x01, 0xD9, 0xCC};
    feedFrame(slave, request, sizeof(request));

    TEST_ASSERT_TRUE(slave.hasResponse());
    TEST_ASSERT_EQUAL_HEX8(0xAA, slave.response()[1]);  // 0x2A | 0x80
    TEST_ASSERT_EQUAL_HEX8(0x01, slave.response()[2]);
}

// =============================================================================
//  Tramado: fragmentacion, silencio y ruido (MB-09, MB-10)
// =============================================================================

/// FR-105: una peticion entregada byte a byte produce la misma respuesta que
/// entregada de una vez.
void test_trama_fragmentada_se_reensambla(void) {
    RegisterFile file;
    file.set(3, 1);
    ModbusSlave slave(file, kId, kSilence);

    const uint8_t request[]  = {0x01, 0x03, 0x00, 0x03, 0x00, 0x01, 0x74, 0x0A};
    const uint8_t expected[] = {0x01, 0x03, 0x02, 0x00, 0x01, 0x79, 0x84};

    // Un byte por milisegundo: por debajo del silencio de 4 ms.
    for (uint8_t i = 0; i < sizeof(request); ++i) {
        slave.onByte(request[i], kT0 + i);
    }
    assertResponse(slave, expected, sizeof(expected));
}

/// El silencio descarta lo parcial sin contaminar la peticion siguiente.
void test_trama_parcial_se_descarta_tras_el_silencio(void) {
    RegisterFile file;
    file.set(3, 1);
    ModbusSlave slave(file, kId, kSilence);

    // Media peticion...
    slave.onByte(0x01, kT0);
    slave.onByte(0x03, kT0 + 1);
    slave.onByte(0x00, kT0 + 2);
    // ...y silencio.
    slave.poll(kT0 + 100);
    TEST_ASSERT_FALSE(slave.hasResponse());

    // La siguiente peticion completa se atiende con normalidad.
    const uint8_t request[]  = {0x01, 0x03, 0x00, 0x03, 0x00, 0x01, 0x74, 0x0A};
    const uint8_t expected[] = {0x01, 0x03, 0x02, 0x00, 0x01, 0x79, 0x84};
    feedFrame(slave, request, sizeof(request), kT0 + 200);
    assertResponse(slave, expected, sizeof(expected));
}

/// Ruido en la linea seguido de una peticion legitima: el ruido se descarta con
/// el silencio y la peticion se atiende.
void test_el_ruido_no_contamina_la_peticion_siguiente(void) {
    RegisterFile file;
    file.set(3, 1);
    ModbusSlave slave(file, kId, kSilence);

    const uint8_t noise[] = {0xFF, 0x00, 0xAA, 0x55, 0x13};
    feedFrame(slave, noise, sizeof(noise), kT0);
    slave.poll(kT0 + 100);
    TEST_ASSERT_FALSE(slave.hasResponse());

    const uint8_t request[]  = {0x01, 0x03, 0x00, 0x03, 0x00, 0x01, 0x74, 0x0A};
    const uint8_t expected[] = {0x01, 0x03, 0x02, 0x00, 0x01, 0x79, 0x84};
    feedFrame(slave, request, sizeof(request), kT0 + 200);
    assertResponse(slave, expected, sizeof(expected));
}

/// MB-10: una avalancha de bytes no desborda el buffer ni deja al receptor
/// sordo a la siguiente peticion legitima.
void test_avalancha_de_bytes_no_desborda_ni_deja_sordo(void) {
    RegisterFile file;
    file.set(3, 1);
    ModbusSlave slave(file, kId, kSilence);

    for (uint16_t i = 0; i < 300; ++i) {
        slave.onByte(static_cast<uint8_t>(i & 0xFF), kT0);
    }
    slave.poll(kT0 + 100);

    const uint8_t request[]  = {0x01, 0x03, 0x00, 0x03, 0x00, 0x01, 0x74, 0x0A};
    const uint8_t expected[] = {0x01, 0x03, 0x02, 0x00, 0x01, 0x79, 0x84};
    feedFrame(slave, request, sizeof(request), kT0 + 200);
    assertResponse(slave, expected, sizeof(expected));
}

// =============================================================================
//  Diagnostico y ciclo de vida de la respuesta
// =============================================================================

void test_la_respuesta_se_consume_una_sola_vez(void) {
    RegisterFile file;
    ModbusSlave slave(file, kId, kSilence);

    const uint8_t request[] = {0x01, 0x03, 0x00, 0x03, 0x00, 0x01, 0x74, 0x0A};
    feedFrame(slave, request, sizeof(request));

    TEST_ASSERT_TRUE(slave.hasResponse());
    slave.responseSent();
    TEST_ASSERT_FALSE(slave.hasResponse());
}

void test_cuenta_las_peticiones_validas_y_marca_el_instante(void) {
    RegisterFile file;
    ModbusSlave slave(file, kId, kSilence);

    TEST_ASSERT_FALSE(slave.everServed());

    const uint8_t request[] = {0x01, 0x03, 0x00, 0x03, 0x00, 0x01, 0x74, 0x0A};
    feedFrame(slave, request, sizeof(request), kT0);

    TEST_ASSERT_EQUAL_UINT16(1, slave.stats().validRequests);
    TEST_ASSERT_EQUAL_UINT32(kT0, slave.lastRequestAtMs());
    TEST_ASSERT_TRUE(slave.everServed());
}

/// MB-08: sin peticion pendiente jamas se transmite nada.
void test_sin_peticion_no_hay_nada_que_transmitir(void) {
    RegisterFile file;
    ModbusSlave slave(file, kId, kSilence);

    slave.poll(kT0);
    slave.poll(kT0 + 10000);
    TEST_ASSERT_FALSE(slave.hasResponse());
}

}  // namespace

void run_slave_tests(void) {
    RUN_TEST(test_fc03_devuelve_el_vector_del_contrato);
    RUN_TEST(test_fc04_devuelve_lo_mismo_que_fc03);
    RUN_TEST(test_lee_las_24_valvulas_de_una_vez);
    RUN_TEST(test_fc01_devuelve_el_vector_del_contrato);
    RUN_TEST(test_fc02_devuelve_lo_mismo_que_fc01);
    RUN_TEST(test_peticion_a_otro_esclavo_produce_silencio);
    RUN_TEST(test_verificador_incorrecto_produce_silencio_y_cuenta);
    RUN_TEST(test_direccion_fuera_del_mapa_da_excepcion_02);
    RUN_TEST(test_rango_que_se_pasa_del_mapa_da_excepcion_02);
    RUN_TEST(test_cantidad_cero_da_excepcion_03);
    RUN_TEST(test_escritura_de_un_registro_da_excepcion_01);
    RUN_TEST(test_escritura_de_una_bobina_da_excepcion_01);
    RUN_TEST(test_funcion_desconocida_da_excepcion_01);
    RUN_TEST(test_trama_fragmentada_se_reensambla);
    RUN_TEST(test_trama_parcial_se_descarta_tras_el_silencio);
    RUN_TEST(test_el_ruido_no_contamina_la_peticion_siguiente);
    RUN_TEST(test_avalancha_de_bytes_no_desborda_ni_deja_sordo);
    RUN_TEST(test_la_respuesta_se_consume_una_sola_vez);
    RUN_TEST(test_cuenta_las_peticiones_validas_y_marca_el_instante);
    RUN_TEST(test_sin_peticion_no_hay_nada_que_transmitir);
}

// =============================================================================
//  Deteccion de maestro ausente (FR-114)
//  Anadido en la Fase 4. Vive en la capa pura, no en main.cpp, para que sea
//  comprobable en host.
// =============================================================================

namespace {

void test_sin_ninguna_peticion_el_maestro_se_considera_ausente(void) {
    RegisterFile file;
    ModbusSlave slave(file, kId, kSilence);

    TEST_ASSERT_TRUE(slave.masterSilent(kT0, 30000));
    TEST_ASSERT_EQUAL_UINT16(0xFFFF, slave.secondsSinceRequest(kT0));
}

void test_tras_una_peticion_el_maestro_esta_presente(void) {
    RegisterFile file;
    ModbusSlave slave(file, kId, kSilence);

    const uint8_t request[] = {0x01, 0x03, 0x00, 0x03, 0x00, 0x01, 0x74, 0x0A};
    feedFrame(slave, request, sizeof(request), kT0);

    TEST_ASSERT_FALSE(slave.masterSilent(kT0, 30000));
    TEST_ASSERT_FALSE(slave.masterSilent(kT0 + 29999, 30000));
    TEST_ASSERT_EQUAL_UINT16(0, slave.secondsSinceRequest(kT0));
    TEST_ASSERT_EQUAL_UINT16(29, slave.secondsSinceRequest(kT0 + 29999));
}

void test_pasado_el_umbral_el_maestro_vuelve_a_estar_ausente(void) {
    RegisterFile file;
    ModbusSlave slave(file, kId, kSilence);

    const uint8_t request[] = {0x01, 0x03, 0x00, 0x03, 0x00, 0x01, 0x74, 0x0A};
    feedFrame(slave, request, sizeof(request), kT0);

    TEST_ASSERT_TRUE(slave.masterSilent(kT0 + 30000, 30000));
    TEST_ASSERT_TRUE(slave.masterSilent(kT0 + 120000, 30000));
    TEST_ASSERT_EQUAL_UINT16(120, slave.secondsSinceRequest(kT0 + 120000));
}

/// La deteccion debe seguir siendo correcta al desbordar millis() a los 49,7
/// dias. Una comparacion ingenua daria "ausente" durante 24 dias seguidos.
void test_la_deteccion_sobrevive_al_desbordamiento_del_reloj(void) {
    RegisterFile file;
    ModbusSlave slave(file, kId, kSilence);

    // Ultima peticion justo antes del desbordamiento de 32 bits.
    const uint32_t antesDelWrap = 0xFFFFFFFFUL - 5000UL;
    const uint8_t request[] = {0x01, 0x03, 0x00, 0x03, 0x00, 0x01, 0x74, 0x0A};
    feedFrame(slave, request, sizeof(request), antesDelWrap);

    // 10 segundos despues, ya al otro lado del desbordamiento.
    const uint32_t despuesDelWrap = antesDelWrap + 10000UL;  // da la vuelta
    TEST_ASSERT_FALSE(slave.masterSilent(despuesDelWrap, 30000));
    TEST_ASSERT_EQUAL_UINT16(10, slave.secondsSinceRequest(despuesDelWrap));

    // Y pasado el umbral, ausente de nuevo.
    TEST_ASSERT_TRUE(slave.masterSilent(antesDelWrap + 40000UL, 30000));
}

}  // namespace

void run_bus_health_tests(void) {
    RUN_TEST(test_sin_ninguna_peticion_el_maestro_se_considera_ausente);
    RUN_TEST(test_tras_una_peticion_el_maestro_esta_presente);
    RUN_TEST(test_pasado_el_umbral_el_maestro_vuelve_a_estar_ausente);
    RUN_TEST(test_la_deteccion_sobrevive_al_desbordamiento_del_reloj);
}
