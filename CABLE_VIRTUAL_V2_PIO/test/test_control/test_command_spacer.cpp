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

// =============================================================================
//  US2: equipos distintos no se esperan (FR-208)
// =============================================================================

// --- Vector 4 ----------------------------------------------------------------
void test_FR208_equipos_distintos_no_se_esperan(void) {
    Spacer sp;
    sp.request(kV02, ValveState::Open, kT);

    const uint32_t t = kT + 60000;
    sp.s.update(kV02, ValveState::Closed, t);  // UC511 #01
    sp.s.update(kV03, ValveState::Open, t);    // UC511 #02
    TEST_ASSERT_EQUAL_UINT8(2, sp.drain(t));
    TEST_ASSERT_TRUE(sp.s.published(kV02) == ValveState::Closed);
    TEST_ASSERT_TRUE(sp.s.published(kV03) == ValveState::Open);
}

void test_FR208_espera_en_un_equipo_no_retrasa_otro(void) {
    Spacer sp;
    sp.request(kV01, ValveState::Open, kT);  // ocupa el UC511 #01

    sp.s.update(kV02, ValveState::Open, kT + 1000);
    sp.s.update(kV03, ValveState::Open, kT + 1000);
    TEST_ASSERT_EQUAL_UINT8(1, sp.drain(kT + 1000));
    TEST_ASSERT_TRUE(sp.s.published(kV03) == ValveState::Open);
    TEST_ASSERT_TRUE(sp.s.pending(kV02));
}

// =============================================================================
//  US5: el arranque no espera (FR-211)
// =============================================================================

namespace {
void seedWithV01Open(CommandSpacer& s, uint32_t atMs) {
    ValveState seeded[config::kValveCount];
    for (uint8_t i = 0; i < config::kValveCount; ++i) seeded[i] = ValveState::Closed;
    seeded[kV01] = ValveState::Open;
    s.begin(seeded, atMs);
}
}  // namespace

void test_FR211_siembra_publica_sin_esperar(void) {
    Spacer sp;
    seedWithV01Open(sp.s, kT);
    TEST_ASSERT_TRUE(sp.s.published(kV01) == ValveState::Open);
    TEST_ASSERT_FALSE(sp.s.pending(kV01));
    TEST_ASSERT_TRUE(sp.s.update(kV01, ValveState::Open, kT) ==
                     CommandSpacer::Outcome::Unchanged);
    TEST_ASSERT_EQUAL_UINT8(0, sp.drain(kT));
}

// --- Vector 10 ---------------------------------------------------------------
void test_FR211_siembra_ocupa_solo_equipos_con_valvula_abierta(void) {
    Spacer sp;
    seedWithV01Open(sp.s, kT);

    sp.s.update(kV02, ValveState::Open, kT + 2000);  // mismo equipo que V01
    sp.s.update(kV03, ValveState::Open, kT + 2000);  // equipo sembrado cerrado
    TEST_ASSERT_EQUAL_UINT8(1, sp.drain(kT + 2000));
    TEST_ASSERT_TRUE(sp.s.published(kV03) == ValveState::Open);
    TEST_ASSERT_TRUE(sp.s.published(kV02) == ValveState::Closed);

    TEST_ASSERT_EQUAL_UINT8(0, sp.drain(kT + kSpacing - 1));
    TEST_ASSERT_EQUAL_UINT8(1, sp.drain(kT + kSpacing));
    TEST_ASSERT_TRUE(sp.s.published(kV02) == ValveState::Open);
}

// =============================================================================
//  US3: pedidos breves o revertidos (FR-204, FR-206, FR-207)
// =============================================================================

// --- Vector 5 ----------------------------------------------------------------
void test_FR206_apertura_pendiente_se_cancela(void) {
    Spacer sp;
    sp.request(kV01, ValveState::Open, kT);

    TEST_ASSERT_TRUE(sp.s.update(kV02, ValveState::Open, kT + 1000) ==
                     CommandSpacer::Outcome::Queued);
    TEST_ASSERT_TRUE(sp.s.update(kV02, ValveState::Closed, kT + 3000) ==
                     CommandSpacer::Outcome::Cancelled);
    TEST_ASSERT_FALSE(sp.s.pending(kV02));
    TEST_ASSERT_EQUAL_UINT8(0, sp.drain(kT + kSpacing));
    TEST_ASSERT_TRUE(sp.s.published(kV02) == ValveState::Closed);
}

// --- Vector 7 ----------------------------------------------------------------
void test_FR206_cierre_pendiente_se_cancela_sin_pasar_por_cero(void) {
    Spacer sp;
    sp.request(kV01, ValveState::Open, kT);

    sp.s.update(kV01, ValveState::Closed, kT + 1000);
    TEST_ASSERT_TRUE(sp.s.published(kV01) == ValveState::Open);
    TEST_ASSERT_TRUE(sp.s.update(kV01, ValveState::Open, kT + 2000) ==
                     CommandSpacer::Outcome::Cancelled);

    TEST_ASSERT_EQUAL_UINT8(0, sp.drain(kT + kSpacing));
    TEST_ASSERT_TRUE(sp.s.published(kV01) == ValveState::Open);
}

void test_FR206_cancelado_no_consume_turno(void) {
    Spacer sp;
    sp.request(kV01, ValveState::Open, kT);

    sp.s.update(kV02, ValveState::Open, kT + 1000);
    sp.s.update(kV02, ValveState::Closed, kT + 2000);  // cancelado

    // El equipo queda libre al cumplirse el intervalo desde la apertura de V01,
    // no desde la cancelacion.
    sp.s.update(kV02, ValveState::Open, kT + kSpacing);
    TEST_ASSERT_EQUAL_UINT8(1, sp.drain(kT + kSpacing));
    TEST_ASSERT_TRUE(sp.s.published(kV02) == ValveState::Open);
}

void test_FR206_nuevo_pedido_tras_cancelar_cuenta_desde_cero(void) {
    // Equipo ocupado con las dos valvulas cerradas: V01 abre y cierra.
    Spacer sp;
    sp.request(kV01, ValveState::Open, kT);
    sp.request(kV01, ValveState::Closed, kT + kSpacing);  // ocupa hasta kT + 16 s

    const uint32_t t = kT + kSpacing;
    sp.s.update(kV02, ValveState::Open, t + 1000);
    sp.s.update(kV02, ValveState::Closed, t + 2000);   // cancelado
    sp.s.update(kV01, ValveState::Open, t + 2500);
    sp.s.update(kV02, ValveState::Open, t + 3000);     // nuevo pedido

    // Si la antiguedad de V02 no se reiniciara (t + 1000), ganaria V02. Gana V01
    // porque el pedido vigente de V02 es de t + 3000.
    TEST_ASSERT_EQUAL_UINT8(1, sp.drain(t + kSpacing));
    TEST_ASSERT_EQUAL_UINT8(kV01, sp.lastIndex);
    TEST_ASSERT_TRUE(sp.s.pending(kV02));
}

// --- Vector 6: abrir y cerrar a los 3 s --------------------------------------
void test_FR204_cierre_pedido_3s_despues_de_abrir_se_publica_a_los_8s(void) {
    Spacer sp;
    sp.request(kV01, ValveState::Open, kT);

    sp.s.update(kV01, ValveState::Closed, kT + 3000);
    TEST_ASSERT_EQUAL_UINT8(0, sp.drain(kT + 3000));
    TEST_ASSERT_EQUAL_UINT8(0, sp.drain(kT + kSpacing - 1));
    TEST_ASSERT_EQUAL_UINT8(1, sp.drain(kT + kSpacing));
    TEST_ASSERT_TRUE(sp.s.published(kV01) == ValveState::Closed);
}

// --- Vector 8: las dos del mismo equipo a la vez -----------------------------
void test_FR207_dos_aperturas_simultaneas_primero_menor_indice(void) {
    Spacer sp;
    sp.s.update(kV02, ValveState::Open, kT);  // informada primero, mismo instante
    sp.s.update(kV01, ValveState::Open, kT);
    TEST_ASSERT_EQUAL_UINT8(1, sp.drain(kT));
    TEST_ASSERT_EQUAL_UINT8(kV01, sp.lastIndex);

    TEST_ASSERT_EQUAL_UINT8(1, sp.drain(kT + kSpacing));
    TEST_ASSERT_EQUAL_UINT8(kV02, sp.lastIndex);
}

// --- Vector 12: el peor caso documentado -------------------------------------
/// Un cierre puede adelantar a una apertura que ya esperaba detras de otro
/// cambio: la apertura sale tres turnos despues de pedirse, cerca de 3
/// intervalos. Es el peor caso posible con dos valvulas por equipo.
void test_FR207_peor_caso_un_cierre_adelanta_dos_veces(void) {
    Spacer sp;
    sp.request(kV01, ValveState::Open, kT);                       // turno en kT
    sp.s.update(kV02, ValveState::Open, kT + 1000);              // espera
    sp.s.update(kV01, ValveState::Closed, kT + kSpacing - 100);  // cierre: prioridad
    TEST_ASSERT_EQUAL_UINT8(1, sp.drain(kT + kSpacing));         // sale el cierre de V01
    TEST_ASSERT_EQUAL_UINT8(kV01, sp.lastIndex);

    const uint32_t v01Asks = kT + kSpacing + 1000;               // V01 vuelve a pedir
    sp.s.update(kV01, ValveState::Open, v01Asks);
    TEST_ASSERT_EQUAL_UINT8(1, sp.drain(kT + 2 * kSpacing));
    TEST_ASSERT_EQUAL_UINT8(kV02, sp.lastIndex);                 // V02 era mas antigua

    sp.s.update(kV02, ValveState::Closed, kT + 2 * kSpacing + 1000);  // cierre
    TEST_ASSERT_EQUAL_UINT8(1, sp.drain(kT + 3 * kSpacing));
    TEST_ASSERT_EQUAL_UINT8(kV02, sp.lastIndex);                 // adelanta a V01

    TEST_ASSERT_EQUAL_UINT8(1, sp.drain(kT + 4 * kSpacing));
    TEST_ASSERT_EQUAL_UINT8(kV01, sp.lastIndex);
    // V01 espero casi 3 intervalos, pero no los alcanzo.
    const uint32_t waited = kT + 4 * kSpacing - v01Asks;
    TEST_ASSERT_TRUE(waited > 2 * kSpacing);
    TEST_ASSERT_TRUE(waited < 3 * kSpacing);
}

/// Una hora simulada con el tablero cambiando al azar las dos valvulas del
/// UC511 #01. Comprueba los dos invariantes que importan en campo:
///   SP-I1  dos cambios publicados del equipo nunca a menos de un intervalo
///   SP-I3  ningun pendiente espera 3 intervalos o mas
void test_FR207_espera_maxima_bajo_pedidos_al_azar(void) {
    Spacer sp;
    uint32_t   rng = 12345;
    uint32_t   queuedAt[2] = {0, 0};
    ValveState desired[2]  = {ValveState::Closed, ValveState::Closed};
    bool       hasLast = false;
    uint32_t   lastPublish = 0;
    uint32_t   worstWait = 0;
    uint16_t   publications = 0;

    const uint32_t start = 0xFFF00000UL;  // cruza el desbordamiento a proposito
    for (uint32_t step = 0; step < 240000; ++step) {   // 1 h en vueltas de 15 ms
        const uint32_t now = start + step * 15;

        rng = rng * 1103515245UL + 12345UL;
        if ((rng >> 16) % 400 == 0) {                  // ~1 cambio cada 6 s
            const uint8_t v = static_cast<uint8_t>((rng >> 8) & 1);
            desired[v] = (desired[v] == ValveState::Open) ? ValveState::Closed
                                                          : ValveState::Open;
        }

        for (uint8_t v = 0; v < 2; ++v) {
            if (sp.s.update(v, desired[v], now) == CommandSpacer::Outcome::Queued) {
                queuedAt[v] = now;
            }
        }

        ValveIndex i;
        ValveState st;
        while (sp.s.next(now, i, st)) {
            TEST_ASSERT_TRUE(st == desired[i]);                       // SP-06
            if (hasLast) {
                TEST_ASSERT_TRUE(now - lastPublish >= kSpacing);      // SP-I1
            }
            const uint32_t wait = now - queuedAt[i];
            if (wait > worstWait) worstWait = wait;
            hasLast     = true;
            lastPublish = now;
            ++publications;
        }
    }
    TEST_ASSERT_TRUE(publications > 100);       // la prueba de verdad ejercito turnos
    TEST_ASSERT_TRUE(worstWait < 3 * kSpacing);  // SP-I3
}

void run_command_spacer_tests(void) {
    RUN_TEST(test_FR208_equipos_distintos_no_se_esperan);
    RUN_TEST(test_FR208_espera_en_un_equipo_no_retrasa_otro);
    RUN_TEST(test_FR211_siembra_publica_sin_esperar);
    RUN_TEST(test_FR211_siembra_ocupa_solo_equipos_con_valvula_abierta);
    RUN_TEST(test_FR206_apertura_pendiente_se_cancela);
    RUN_TEST(test_FR206_cierre_pendiente_se_cancela_sin_pasar_por_cero);
    RUN_TEST(test_FR206_cancelado_no_consume_turno);
    RUN_TEST(test_FR206_nuevo_pedido_tras_cancelar_cuenta_desde_cero);
    RUN_TEST(test_FR204_cierre_pedido_3s_despues_de_abrir_se_publica_a_los_8s);
    RUN_TEST(test_FR207_dos_aperturas_simultaneas_primero_menor_indice);
    RUN_TEST(test_FR207_peor_caso_un_cierre_adelanta_dos_veces);
    RUN_TEST(test_FR207_espera_maxima_bajo_pedidos_al_azar);
    RUN_TEST(test_FR205_cambio_con_equipo_libre_se_publica_en_la_misma_vuelta);
    RUN_TEST(test_FR204_apertura_en_el_mismo_equipo_espera_el_intervalo);
    RUN_TEST(test_FR207_cierre_sale_antes_que_apertura_pendiente);
    RUN_TEST(test_FR210_intervalo_correcto_con_desbordamiento_del_reloj);
    RUN_TEST(test_FR204_maximo_un_cambio_por_equipo_y_vuelta);
    RUN_TEST(test_FR209_sin_pedido_no_hay_cambio);
}
