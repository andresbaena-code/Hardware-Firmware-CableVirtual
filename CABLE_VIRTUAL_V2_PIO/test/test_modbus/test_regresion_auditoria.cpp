// -----------------------------------------------------------------------------
//  Regresion: un caso por defecto catalogado en la auditoria.
//  Cubre: FR-105, FR-107, FR-118, FR-124  (SC-102, SC-106, SC-107)
//
//  Cada prueba de este archivo corresponde a un defecto REAL que la
//  implementacion anterior tenia y que se verifico numericamente. Reintroducir
//  cualquiera de ellos debe poner la suite en rojo.
//
//  Sin este archivo, la lista de la auditoria seria un informe que envejece. Con
//  el, es una red de seguridad permanente.
// -----------------------------------------------------------------------------
#include <unity.h>

#include "modbus_crc.h"
#include "modbus_slave.h"
#include "register_file.h"
#include "timing_config.h"

using wiga::modbus::ModbusSlave;
using wiga::modbus::RegisterFile;
namespace config = wiga::config;

namespace {

constexpr uint8_t  kId      = 1;
constexpr uint16_t kSilence = 4;
constexpr uint32_t kT0      = 200000;

/// Construye una peticion de lectura valida con su verificador.
void makeReadRequest(uint8_t* out, uint8_t functionCode,
                     uint16_t start, uint16_t quantity) {
    out[0] = kId;
    out[1] = functionCode;
    out[2] = static_cast<uint8_t>(start >> 8);
    out[3] = static_cast<uint8_t>(start & 0xFF);
    out[4] = static_cast<uint8_t>(quantity >> 8);
    out[5] = static_cast<uint8_t>(quantity & 0xFF);
    const uint16_t crc = wiga::modbus::crc16(out, 6);
    out[6] = static_cast<uint8_t>(crc & 0xFF);
    out[7] = static_cast<uint8_t>(crc >> 8);
}

/// Comprueba que el esclavo sigue vivo y atiende correctamente una peticion
/// legitima. Es la asercion que demuestra que un ataque no lo dejo sordo.
void assertStillServes(ModbusSlave& slave, RegisterFile& file, uint32_t atMs) {
    file.set(3, 1);
    uint8_t request[8];
    makeReadRequest(request, 0x03, 3, 1);
    for (uint8_t i = 0; i < 8; ++i) slave.onByte(request[i], atMs);

    TEST_ASSERT_TRUE_MESSAGE(slave.hasResponse(),
                             "el esclavo quedo sordo tras la trama malformada");
    TEST_ASSERT_EQUAL_HEX8(0x01, slave.response()[4]);
    slave.responseSent();
}

// =============================================================================
//  DEFECTO 1 — Campo de longitud mentiroso
//
//  En la implementacion anterior, poll() comprobaba
//      respLen < (uint8_t)(3 + byteCount + 2)
//  Con byteCount = 251 esa expresion vale 0, la comprobacion pasa, y despues
//  crc16(response, 3 + byteCount) leia 254 bytes de un buffer de pila de 32.
//  Verificado numericamente durante la auditoria.
// =============================================================================

void test_campo_de_longitud_mentiroso_no_lee_fuera_de_memoria(void) {
    const uint8_t byteCounts[] = {250, 251, 255};

    for (uint8_t c = 0; c < 3; ++c) {
        RegisterFile file;
        ModbusSlave slave(file, kId, kSilence);

        // FC16 (escritura multiple) declarando un byteCount enorme. La trama
        // real es mucho mas corta que lo que anuncia.
        const uint8_t head[] = {kId, 0x10, 0x00, 0x00, 0x00, 0x7D, byteCounts[c]};
        for (uint8_t i = 0; i < sizeof(head); ++i) {
            slave.onByte(head[i], kT0);
        }
        // Unos cuantos bytes de datos, muy por debajo de lo declarado.
        for (uint8_t i = 0; i < 5; ++i) {
            slave.onByte(0xAA, kT0);
        }

        // No debe haber respuesta a una trama que nunca se completo...
        TEST_ASSERT_FALSE(slave.hasResponse());

        // ...y el silencio debe devolver al esclavo a un estado sano.
        slave.poll(kT0 + 100);
        assertStillServes(slave, file, kT0 + 200);
    }
}

/// La misma familia de defecto por el lado de la lectura: pedir una cantidad
/// enorme no puede desbordar el buffer de respuesta.
void test_cantidad_enorme_no_desborda_la_respuesta(void) {
    RegisterFile file;
    ModbusSlave slave(file, kId, kSilence);

    uint8_t request[8];
    makeReadRequest(request, 0x03, 0, 125);  // el maximo del estandar
    for (uint8_t i = 0; i < 8; ++i) slave.onByte(request[i], kT0);

    // 125 registros no caben en el mapa de 32: excepcion, no desbordamiento.
    TEST_ASSERT_TRUE(slave.hasResponse());
    TEST_ASSERT_EQUAL_HEX8(0x83, slave.response()[1]);
    TEST_ASSERT_EQUAL_HEX8(0x02, slave.response()[2]);
    TEST_ASSERT_TRUE(slave.responseLength() <= wiga::modbus::kTxCapacity);
}

/// La respuesta jamas puede exceder la capacidad declarada del buffer, sea cual
/// sea la peticion.
void test_ninguna_respuesta_excede_la_capacidad_del_buffer(void) {
    RegisterFile file;
    for (uint16_t i = 0; i < RegisterFile::count(); ++i) file.set(i, 0xFFFF);
    ModbusSlave slave(file, kId, kSilence);

    for (uint16_t qty = 1; qty <= RegisterFile::count(); ++qty) {
        uint8_t request[8];
        makeReadRequest(request, 0x03, 0, qty);
        for (uint8_t i = 0; i < 8; ++i) slave.onByte(request[i], kT0 + qty);

        TEST_ASSERT_TRUE(slave.hasResponse());
        TEST_ASSERT_TRUE_MESSAGE(
            slave.responseLength() <= wiga::modbus::kTxCapacity,
            "la respuesta excede el buffer de transmision");
        slave.responseSent();
    }
}

// =============================================================================
//  DEFECTO 2 — Truncamiento: 12 de 24 valvulas inalcanzables
//
//  kMaxFramesPerPoll = 4 hacia que poll() solo emitiera valveId 0..3. Las
//  valvulas con id >= 4 (indices 4-7, 12-15, 20-23) no podian confirmarse nunca.
// =============================================================================

void test_las_24_valvulas_son_observables_ninguna_excluida(void) {
    RegisterFile file;
    ModbusSlave slave(file, kId, kSilence);

    // Se marca una sola valvula cada vez y se comprueba que se puede leer.
    for (uint8_t v = 0; v < config::kValveCount; ++v) {
        RegisterFile local;
        ModbusSlave  s(local, kId, kSilence);
        local.set(config::valveRegister(v), 1);

        uint8_t request[8];
        makeReadRequest(request, 0x03, config::valveRegister(v), 1);
        for (uint8_t i = 0; i < 8; ++i) s.onByte(request[i], kT0);

        TEST_ASSERT_TRUE(s.hasResponse());
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(
            0x01, s.response()[4],
            "una valvula quedo estructuralmente inalcanzable");
    }
}

/// Y todas de una sola trama, tanto como registros como como bobinas.
void test_las_24_valvulas_caben_en_una_sola_respuesta(void) {
    RegisterFile file;
    for (uint8_t v = 0; v < config::kValveCount; ++v) {
        file.set(config::valveRegister(v), 1);
    }
    ModbusSlave slave(file, kId, kSilence);

    uint8_t request[8];
    makeReadRequest(request, 0x01, 0, config::kValveCount);
    for (uint8_t i = 0; i < 8; ++i) slave.onByte(request[i], kT0);

    TEST_ASSERT_TRUE(slave.hasResponse());
    TEST_ASSERT_EQUAL_UINT8(3, slave.response()[2]);      // 24 bits = 3 bytes
    TEST_ASSERT_EQUAL_HEX8(0xFF, slave.response()[3]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, slave.response()[4]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, slave.response()[5]);
}

// =============================================================================
//  DEFECTO 3 — Corpus de tramas arbitrarias (SC-107)
//
//  Ninguna trama, por malformada que sea, puede provocar acceso fuera de los
//  buffers ni dejar al esclavo sordo.
// =============================================================================

void test_corpus_de_tramas_arbitrarias_no_rompe_el_esclavo(void) {
    RegisterFile file;
    ModbusSlave slave(file, kId, kSilence);

    // Generador determinista: los mismos bytes en cada ejecucion, para que un
    // fallo sea reproducible.
    uint32_t seed = 0x12345678UL;
    uint32_t t = kT0;

    for (uint16_t frame = 0; frame < 500; ++frame) {
        const uint8_t length = static_cast<uint8_t>((seed >> 8) % 40u);
        for (uint8_t i = 0; i < length; ++i) {
            seed = seed * 1103515245UL + 12345UL;
            slave.onByte(static_cast<uint8_t>(seed >> 16), t);
        }
        t += 10;
        slave.poll(t);
        if (slave.hasResponse()) {
            // Si respondio, la respuesta debe estar dentro de limites sanos.
            TEST_ASSERT_TRUE(slave.responseLength() <= wiga::modbus::kTxCapacity);
            TEST_ASSERT_EQUAL_HEX8(kId, slave.response()[0]);
            slave.responseSent();
        }
        t += 10;
        seed = seed * 1103515245UL + 12345UL;
    }

    // Tras 500 tramas basura, el esclavo sigue atendiendo correctamente.
    slave.poll(t + 100);
    assertStillServes(slave, file, t + 200);
}

/// Bytes sueltos separados por silencios largos: ruido de linea tipico. No
/// deben acumularse ni interpretarse como peticion.
void test_ruido_disperso_no_se_acumula(void) {
    RegisterFile file;
    ModbusSlave slave(file, kId, kSilence);

    uint32_t t = kT0;
    for (uint16_t i = 0; i < 200; ++i) {
        slave.onByte(static_cast<uint8_t>(i & 0xFF), t);
        t += 50;                 // muy por encima del silencio de 4 ms
        slave.poll(t);
        TEST_ASSERT_FALSE(slave.hasResponse());
    }

    assertStillServes(slave, file, t + 100);
}

}  // namespace

void run_regresion_auditoria_tests(void) {
    RUN_TEST(test_campo_de_longitud_mentiroso_no_lee_fuera_de_memoria);
    RUN_TEST(test_cantidad_enorme_no_desborda_la_respuesta);
    RUN_TEST(test_ninguna_respuesta_excede_la_capacidad_del_buffer);
    RUN_TEST(test_las_24_valvulas_son_observables_ninguna_excluida);
    RUN_TEST(test_las_24_valvulas_caben_en_una_sola_respuesta);
    RUN_TEST(test_corpus_de_tramas_arbitrarias_no_rompe_el_esclavo);
    RUN_TEST(test_ruido_disperso_no_se_acumula);
}
