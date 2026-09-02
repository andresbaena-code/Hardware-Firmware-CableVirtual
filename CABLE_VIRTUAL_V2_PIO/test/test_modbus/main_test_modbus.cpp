// -----------------------------------------------------------------------------
//  Runner de la suite del protocolo Modbus.
//  PlatformIO enlaza toda la carpeta test_modbus en un unico binario, asi que
//  main(), setUp() y tearDown() viven aqui una sola vez.
//
//  Esta suite existe porque su ausencia fue la causa raiz de los 25 defectos de
//  la implementacion anterior: el protocolo vivia en lib/wiga_hal, que el
//  entorno de pruebas excluye con lib_ignore, y ninguna de las 73 pruebas del
//  proyecto lo tocaba.
// -----------------------------------------------------------------------------
#include <unity.h>

void run_crc_tests(void);
void run_register_file_tests(void);
void run_slave_tests(void);
void run_regresion_auditoria_tests(void);
void run_bus_health_tests(void);
void run_llegada_real_tests(void);

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
    UNITY_BEGIN();
    run_crc_tests();
    run_register_file_tests();
    run_slave_tests();
    run_regresion_auditoria_tests();
    run_bus_health_tests();
    run_llegada_real_tests();
    return UNITY_END();
}
