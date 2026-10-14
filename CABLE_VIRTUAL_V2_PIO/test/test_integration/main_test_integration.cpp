// Runner de la suite de integracion.
#include <unity.h>

void run_publica_estado_tests(void);
void run_separacion_uc511_tests(void);

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
    UNITY_BEGIN();
    run_publica_estado_tests();
    run_separacion_uc511_tests();
    return UNITY_END();
}
