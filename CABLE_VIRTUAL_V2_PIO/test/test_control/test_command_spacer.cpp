// -----------------------------------------------------------------------------
//  Verifica: FR-204..FR-211  (specs/003-retardo-apertura-cancelable,
//  contracts/command-spacer.md seccion 5)
//
//  Usa el mapa de la instalacion: las valvulas 0 y 1 comparten el UC511 #01 y
//  la 2 cuelga del #02. El antirrebote ya esta resuelto: aqui solo hay turnos.
// -----------------------------------------------------------------------------
#include <unity.h>

#include "command_spacer.h"
#include "installation_map.h"
#include "timing_config.h"

using wiga::CommandSpacer;
using wiga::ValveIndex;
using wiga::ValveState;
namespace config = wiga::config;

namespace {

constexpr uint32_t   kT       = 100000;
constexpr uint32_t   kSpacing = config::kCommandSpacingMs;
constexpr ValveIndex kV01     = 0;  // UC511 #01, salida 1
constexpr ValveIndex kV02     = 1;  // UC511 #01, salida 2
constexpr ValveIndex kV03     = 2;  // UC511 #02, salida 1

/// Un separador arrancado con todas las valvulas cerradas.
struct Spacer {
    CommandSpacer s;
    ValveIndex    lastIndex = wiga::kNoValve;
    ValveState    lastState = ValveState::Closed;

    explicit Spacer(uint32_t bootMs = 0) {
        ValveState closed[config::kValveCount];
        for (uint8_t i = 0; i < config::kValveCount; ++i) closed[i] = ValveState::Closed;
        s.begin(closed, bootMs);
    }

    /// Drena los cambios autorizados en una vuelta. Devuelve cuantos hubo.
    uint8_t drain(uint32_t nowMs) {
        uint8_t n = 0;
        ValveIndex i;
        ValveState st;
        while (s.next(nowMs, i, st)) {
            lastIndex = i;
            lastState = st;
            ++n;
            if (n > config::kValveCount) break;  // red de seguridad de la prueba
        }
        return n;
    }

    /// Pide y drena en la misma vuelta, como hace el publicador.
    void request(ValveIndex index, ValveState desired, uint32_t nowMs) {
        s.update(index, desired, nowMs);
        drain(nowMs);
    }
};

}  // namespace

// --- Vector 1 ----------------------------------------------------------------
void test_FR205_cambio_con_equipo_libre_se_publica_en_la_misma_vuelta(void) {
    Spacer sp;
    TEST_ASSERT_TRUE(sp.s.update(kV01, ValveState::Open, kT) ==
                     CommandSpacer::Outcome::Queued);
    TEST_ASSERT_EQUAL_UINT8(1, sp.drain(kT));
    TEST_ASSERT_EQUAL_UINT8(kV01, sp.lastIndex);
    TEST_ASSERT_TRUE(sp.lastState == ValveState::Open);
    TEST_ASSERT_TRUE(sp.s.published(kV01) == ValveState::Open);
    TEST_ASSERT_FALSE(sp.s.pending(kV01));
}

// --- Vector 2: el caso del piloto --------------------------------------------
void test_FR204_apertura_en_el_mismo_equipo_espera_el_intervalo(void) {
    Spacer sp;
    sp.request(kV01, ValveState::Open, kT);

    // A los 60 s el tablero cierra V01 y abre V02, del mismo UC511, a la vez.
    const uint32_t tc = kT + 60000;
    sp.s.update(kV01, ValveState::Closed, tc);
    sp.s.update(kV02, ValveState::Open, tc);
    TEST_ASSERT_EQUAL_UINT8(1, sp.drain(tc));
    TEST_ASSERT_TRUE(sp.s.published(kV01) == ValveState::Closed);
    TEST_ASSERT_TRUE(sp.s.published(kV02) == ValveState::Closed);
    TEST_ASSERT_TRUE(sp.s.pending(kV02));

    // Un milisegundo antes del intervalo, nada.
    sp.s.update(kV02, ValveState::Open, tc + kSpacing - 1);
    TEST_ASSERT_EQUAL_UINT8(0, sp.drain(tc + kSpacing - 1));
    TEST_ASSERT_TRUE(sp.s.published(kV02) == ValveState::Closed);

    // Justo al cumplirse, la apertura.
    sp.s.update(kV02, ValveState::Open, tc + kSpacing);
    TEST_ASSERT_EQUAL_UINT8(1, sp.drain(tc + kSpacing));
    TEST_ASSERT_TRUE(sp.s.published(kV02) == ValveState::Open);
}

// --- Vector 3: el cierre adelanta a una apertura mas antigua -----------------
void test_FR207_cierre_sale_antes_que_apertura_pendiente(void) {
    Spacer sp;
    sp.request(kV01, ValveState::Open, kT);  // ocupa el UC511 #01 hasta kT + 8 s

    sp.s.update(kV02, ValveState::Open, kT + 3000);    // la apertura llega antes
    sp.s.update(kV01, ValveState::Closed, kT + 3015);  // el cierre, una vuelta despues
    TEST_ASSERT_EQUAL_UINT8(0, sp.drain(kT + 3015));

    // Primer turno: el cierre, aunque la apertura sea mas antigua.
    TEST_ASSERT_EQUAL_UINT8(1, sp.drain(kT + kSpacing));
    TEST_ASSERT_EQUAL_UINT8(kV01, sp.lastIndex);
    TEST_ASSERT_TRUE(sp.lastState == ValveState::Closed);
    TEST_ASSERT_TRUE(sp.s.published(kV02) == ValveState::Closed);

    // Segundo turno, un intervalo despues: la apertura.
    TEST_ASSERT_EQUAL_UINT8(0, sp.drain(kT + 2 * kSpacing - 1));
    TEST_ASSERT_EQUAL_UINT8(1, sp.drain(kT + 2 * kSpacing));
    TEST_ASSERT_EQUAL_UINT8(kV02, sp.lastIndex);
    TEST_ASSERT_TRUE(sp.s.published(kV02) == ValveState::Open);
}

// --- Vector 9: el intervalo cruza el desbordamiento de millis() ----------------
void test_FR210_intervalo_correcto_con_desbordamiento_del_reloj(void) {
    const uint32_t base = 0xFFFFF000UL;
    Spacer sp;
    sp.request(kV01, ValveState::Open, base);

    sp.s.update(kV02, ValveState::Open, base + 1000);
    TEST_ASSERT_EQUAL_UINT8(0, sp.drain(base + 1000));

    const uint32_t justBefore = base + kSpacing - 1;  // ya desbordado
    const uint32_t due        = base + kSpacing;
    TEST_ASSERT_TRUE(due < base);  // la prueba de verdad cruza el desbordamiento
    TEST_ASSERT_EQUAL_UINT8(0, sp.drain(justBefore));
    TEST_ASSERT_EQUAL_UINT8(1, sp.drain(due));
    TEST_ASSERT_TRUE(sp.s.published(kV02) == ValveState::Open);
}

// --- SP-04: un cambio por equipo y vuelta ------------------------------------
void test_FR204_maximo_un_cambio_por_equipo_y_vuelta(void) {
    Spacer sp;
    sp.s.update(kV01, ValveState::Open, kT);
    sp.s.update(kV02, ValveState::Open, kT);
    TEST_ASSERT_EQUAL_UINT8(1, sp.drain(kT));
    TEST_ASSERT_EQUAL_UINT8(1, (sp.s.published(kV01) == ValveState::Open) +
                               (sp.s.published(kV02) == ValveState::Open));
}

// --- SP-06: nunca se publica algo que el tablero no pide ---------------------
void test_FR209_sin_pedido_no_hay_cambio(void) {
    Spacer sp;
    for (uint8_t i = 0; i < config::kValveCount; ++i) {
        TEST_ASSERT_TRUE(sp.s.update(i, ValveState::Closed, kT) ==
                         CommandSpacer::Outcome::Unchanged);
    }
    TEST_ASSERT_EQUAL_UINT8(0, sp.drain(kT));
    (void)kV03;
}

void run_command_spacer_tests(void) {
    RUN_TEST(test_FR205_cambio_con_equipo_libre_se_publica_en_la_misma_vuelta);
    RUN_TEST(test_FR204_apertura_en_el_mismo_equipo_espera_el_intervalo);
    RUN_TEST(test_FR207_cierre_sale_antes_que_apertura_pendiente);
    RUN_TEST(test_FR210_intervalo_correcto_con_desbordamiento_del_reloj);
    RUN_TEST(test_FR204_maximo_un_cambio_por_equipo_y_vuelta);
    RUN_TEST(test_FR209_sin_pedido_no_hay_cambio);
}
