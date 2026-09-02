// -----------------------------------------------------------------------------
//  Runner de la suite de dominio.
//  PlatformIO enlaza toda la carpeta test_domain en un unico binario, asi que
//  main(), setUp() y tearDown() viven aqui una sola vez.
// -----------------------------------------------------------------------------
#include <unity.h>

void run_valve_types_tests(void);
void run_config_tests(void);

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
    UNITY_BEGIN();
    run_valve_types_tests();
    run_config_tests();
    return UNITY_END();
}
