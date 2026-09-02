// Runner de la suite de control. PlatformIO enlaza la carpeta en un unico
// binario, asi que main(), setUp() y tearDown() viven aqui una sola vez.
#include <unity.h>

void run_debouncer_tests(void);
void run_time_wraparound_tests(void);

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
    UNITY_BEGIN();
    run_debouncer_tests();
    run_time_wraparound_tests();
    return UNITY_END();
}
