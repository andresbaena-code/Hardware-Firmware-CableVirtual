// Runner de la suite de integracion.
#include <unity.h>

void run_supervisor_tests(void);
void run_no_burst_on_boot_tests(void);
void run_fault_isolation_tests(void);

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
    UNITY_BEGIN();
    run_supervisor_tests();
    run_no_burst_on_boot_tests();
    run_fault_isolation_tests();
    return UNITY_END();
}
