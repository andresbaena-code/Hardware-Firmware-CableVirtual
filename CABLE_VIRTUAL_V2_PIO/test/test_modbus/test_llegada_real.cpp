// -----------------------------------------------------------------------------
//  Como llegan los bytes DE VERDAD desde el cable.
//  Cubre: FR-105, FR-121
//
//  Las demas pruebas alimentan la trama entera con el MISMO instante, porque asi
//  es comodo. El hardware no funciona asi: a 9600 8N1 cada byte tarda 1,04 ms, y
//  main.cpp captura millis() UNA vez por vuelta de lazo. Si el lazo tarda mas
//  que el silencio entre tramas, dos bytes consecutivos PARECEN separados por
//  mas tiempo del real.
// -----------------------------------------------------------------------------
#include <unity.h>

#include "modbus_slave.h"
#include "register_file.h"
#include "timing_config.h"

using wiga::modbus::ModbusSlave;
using wiga::modbus::RegisterFile;
namespace config = wiga::config;

namespace {

constexpr uint8_t  kId  = config::kModbusSlaveId;
constexpr uint32_t kT0  = 100000;

// La peticion real que se envia desde Docklight: leer el registro 24.
const uint8_t kPeticion[8] = {0x01, 0x03, 0x00, 0x18, 0x00, 0x01, 0x04, 0x0D};

/// Simula el lazo: los bytes llegan separados `gapMs`, y entre byte y byte el
/// lazo gira llamando a poll() sin que haya nada nuevo.
bool entregaConLazo(ModbusSlave& slave, uint32_t gapMs) {
    uint32_t t = kT0;
    for (uint8_t i = 0; i < 8; ++i) {
        for (uint8_t k = 0; k < 3; ++k) slave.poll(t);
        slave.onByte(kPeticion[i], t);
        t += gapMs;
    }
    return slave.hasResponse();
}

/// Cadencia real del cable: 1 ms por byte, lazo rapido.
void test_lazo_rapido_un_ms_por_byte(void) {
    RegisterFile file;
    ModbusSlave slave(file, kId, config::kModbusSilenceMs);
    TEST_ASSERT_TRUE_MESSAGE(entregaConLazo(slave, 1),
                             "con lazo rapido la trama debe atenderse");
}

/// EL CASO QUE FALLA EN HARDWARE: el lazo tarda mas que el silencio entre
/// tramas. Los bytes llegaron seguidos por el cable, pero el firmware los ve
/// separados 5 ms y descarta la trama a medias.
void test_lazo_lento_no_debe_descartar_la_trama(void) {
    RegisterFile file;
    ModbusSlave slave(file, kId, config::kModbusSilenceMs);
    TEST_ASSERT_TRUE_MESSAGE(entregaConLazo(slave, 5),
                             "un lazo lento NO puede impedir atender la trama");
}

/// Y con un lazo claramente lento, como cuando el panel LCD esta escribiendo.
void test_lazo_muy_lento_no_debe_descartar_la_trama(void) {
    RegisterFile file;
    ModbusSlave slave(file, kId, config::kModbusSilenceMs);
    TEST_ASSERT_TRUE_MESSAGE(entregaConLazo(slave, 15),
                             "el refresco del panel no puede romper el bus");
}

/// Varios bytes drenados en la MISMA vuelta, que es lo que pasa cuando el lazo
/// va mas lento que la llegada: se acumulan en el buffer del puerto.
void test_varios_bytes_en_la_misma_vuelta(void) {
    RegisterFile file;
    ModbusSlave slave(file, kId, config::kModbusSilenceMs);

    uint32_t t = kT0;
    slave.poll(t);
    slave.onByte(kPeticion[0], t);
    slave.onByte(kPeticion[1], t);
    slave.onByte(kPeticion[2], t);
    t += 4;                                   // el lazo tarda 4 ms
    slave.poll(t);
    for (uint8_t i = 3; i < 8; ++i) slave.onByte(kPeticion[i], t);

    TEST_ASSERT_TRUE_MESSAGE(slave.hasResponse(),
                             "bytes acumulados en el buffer deben atenderse");
}

}  // namespace

void run_llegada_real_tests(void) {
    RUN_TEST(test_lazo_rapido_un_ms_por_byte);
    RUN_TEST(test_lazo_lento_no_debe_descartar_la_trama);
    RUN_TEST(test_lazo_muy_lento_no_debe_descartar_la_trama);
    RUN_TEST(test_varios_bytes_en_la_misma_vuelta);
}
