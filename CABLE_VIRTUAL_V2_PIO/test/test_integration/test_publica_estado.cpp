// -----------------------------------------------------------------------------
//  Integracion: el tablero publica y el maestro lee.
//  Cubre: FR-109, FR-110, FR-111, FR-112, FR-116, FR-119, FR-122
//
//  Recorre el camino completo sin hardware: pin fisico -> antirrebote ->
//  conversion de dominio -> registro publicado -> trama Modbus en el bus.
//
//  El grupo de FR-116 es el mas importante de toda la suite: un solo sondeo que
//  lea 0 mientras el riego sigue pedido cierra la valvula en campo.
// -----------------------------------------------------------------------------
#include <unity.h>

#include "fake_clock.h"
#include "fake_display.h"
#include "fake_inputs.h"
#include "fake_log.h"
#include "modbus_crc.h"
#include "modbus_slave.h"
#include "register_file.h"
#include "timing_config.h"
#include "valve_publisher.h"

using wiga::SignalLevel;
using wiga::ValveState;
using wiga::ValvePublisher;
using wiga::modbus::ModbusSlave;
using wiga::modbus::RegisterFile;
namespace config = wiga::config;

namespace {

constexpr uint32_t kT0      = 500000;
constexpr uint16_t kSilence = config::kModbusSilenceMs;

/// Monta el sistema completo, como lo hace main.cpp.
struct System {
    wiga::test::FakeClock   clock;
    wiga::test::FakeInputs  inputs;
    wiga::test::FakeDisplay display;
    wiga::test::FakeLog     log;
    RegisterFile            registers;
    ValvePublisher          publisher{clock, inputs, registers, display, log};
    ModbusSlave             slave{registers, config::kModbusSlaveId, kSilence};

    void boot(uint32_t atMs = kT0) {
        clock.set(atMs);
        publisher.begin();
    }

    /// Deja pasar el tiempo suficiente para que el antirrebote confirme.
    void settle(uint32_t ms = config::kDebounceMs * 3) {
        const uint32_t step = 5;
        for (uint32_t t = 0; t < ms; t += step) {
            clock.advance(step);
            publisher.tick();
        }
    }

    /// Sondea una valvula por FC03 y devuelve el valor leido de la respuesta.
    uint16_t pollValve(uint8_t index, uint32_t atMs) {
        uint8_t request[8] = {config::kModbusSlaveId, 0x03,
                              0x00, index, 0x00, 0x01, 0x00, 0x00};
        const uint16_t crc = wiga::modbus::crc16(request, 6);
        request[6] = static_cast<uint8_t>(crc & 0xFF);
        request[7] = static_cast<uint8_t>(crc >> 8);

        for (uint8_t i = 0; i < 8; ++i) slave.onByte(request[i], atMs);
        if (!slave.hasResponse()) return 0xFFFF;

        const uint16_t value =
            static_cast<uint16_t>((slave.response()[3] << 8) | slave.response()[4]);
        slave.responseSent();
        return value;
    }
};

// =============================================================================
//  Publicacion del estado del tablero
// =============================================================================

void test_el_arranque_siembra_desde_los_pines_fisicos(void) {
    System sys;
    sys.inputs.requestOpen(3);   // el interruptor 3 pide riego ANTES de arrancar
    sys.inputs.requestOpen(7);
    sys.boot();

    // FR-116: el primer sondeo ya devuelve el estado correcto. Si el esclavo
    // contestara antes de sembrar, devolveria 0 y el UC100 cerraria la valvula.
    TEST_ASSERT_EQUAL_UINT16(1, sys.pollValve(3, kT0));
    TEST_ASSERT_EQUAL_UINT16(1, sys.pollValve(7, kT0 + 10));
    TEST_ASSERT_EQUAL_UINT16(0, sys.pollValve(0, kT0 + 20));
}

void test_un_cambio_de_interruptor_se_publica_tras_el_antirrebote(void) {
    System sys;
    sys.boot();
    TEST_ASSERT_EQUAL_UINT16(0, sys.pollValve(5, kT0));

    sys.inputs.requestOpen(5);
    sys.settle();

    TEST_ASSERT_EQUAL_UINT16(1, sys.pollValve(5, sys.clock.nowMs()));
}

void test_solo_cambia_la_valvula_del_interruptor_movido(void) {
    System sys;
    sys.boot();
    sys.inputs.requestOpen(7);
    sys.settle();

    for (uint8_t i = 0; i < config::kValveCount; ++i) {
        const uint16_t expected = (i == 7) ? 1 : 0;
        TEST_ASSERT_EQUAL_UINT16(expected,
                                 sys.registers.get(config::valveRegister(i)));
    }
}

/// FR-112: el cableado del tablero es ACTIVO EN ALTO, verificado en banco el
/// 2026-08-27 sobre la placa real: la entrada se energiza a 3,3 V al activarse y
/// queda con pull-down externa en reposo.
///
/// Los niveles van ESCRITOS a proposito, no tomados de la configuracion: si
/// alguien invirtiera kPanelActiveLevel, una prueba que leyera la config
/// seguiria pasando y no detectaria nada.
void test_el_tablero_es_activo_en_alto(void) {
    System sys;
    sys.boot();

    // Pin ENERGIZADO (3,3 V)  ->  el operador pide riego  ->  registro 1
    sys.inputs.set(2, SignalLevel::High);
    sys.settle();
    TEST_ASSERT_EQUAL_UINT16(1, sys.registers.get(config::valveRegister(2)));

    // Pin en pull-down  ->  no se pide riego  ->  registro 0
    // El cierre llega poco despues de la apertura en el mismo UC511, asi que
    // espera su turno (spec 003, FR-204): se deja pasar el intervalo.
    sys.inputs.set(2, SignalLevel::Low);
    sys.settle(config::kCommandSpacingMs + config::kDebounceMs * 3);
    TEST_ASSERT_EQUAL_UINT16(0, sys.registers.get(config::valveRegister(2)));
}

/// El fallo observado en banco: con el tablero en reposo el sistema pedia riego
/// en las 24 valvulas. En reposo TODO debe valer 0.
void test_el_reposo_no_pide_riego_en_ninguna_valvula(void) {
    System sys;
    sys.boot();   // FakeInputs arranca en reposo

    for (uint8_t i = 0; i < config::kValveCount; ++i) {
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(
            0, sys.registers.get(config::valveRegister(i)),
            "en reposo el tablero no puede pedir riego");
    }
    sys.settle();
    for (uint8_t i = 0; i < config::kValveCount; ++i) {
        TEST_ASSERT_EQUAL_UINT16(0, sys.registers.get(config::valveRegister(i)));
    }
}

/// FR-118: las 24 valvulas, no un subconjunto. Regresion del truncamiento que
/// dejaba 12 de 24 permanentemente invisibles.
///
/// Desde la spec 003 (FR-212) una valvula de un UC511 no instalado publica
/// siempre 0, pero su registro sigue existiendo y respondiendo: lo que FR-118
/// exige es que el maestro pueda leer las 24, y eso no cambia.
void test_las_24_valvulas_se_publican(void) {
    System sys;
    sys.boot();
    for (uint8_t i = 0; i < config::kValveCount; ++i) {
        sys.inputs.requestOpen(i);
    }
    // La segunda valvula de cada UC511 sale un intervalo despues de la primera
    // (spec 003, FR-204): se deja pasar antes de comprobar las 24.
    sys.settle(config::kCommandSpacingMs + config::kDebounceMs * 3);

    for (uint8_t i = 0; i < config::kValveCount; ++i) {
        const uint16_t expected =
            config::actuatorInstalled(config::actuatorOf(i)) ? 1 : 0;
        TEST_ASSERT_EQUAL_UINT16(expected, sys.pollValve(i, sys.clock.nowMs() + i));
    }
}

// =============================================================================
//  FR-116: el registro NUNCA presenta un 0 transitorio
// =============================================================================

/// El escenario decisivo: miles de sondeos con la valvula regando, y ni una
/// sola lectura de 0. Un unico fallo aqui corta el riego en campo.
void test_riego_sostenido_nunca_devuelve_cero(void) {
    System sys;
    sys.boot();
    sys.inputs.requestOpen(5);
    sys.settle();

    uint32_t ceros = 0;
    for (uint32_t i = 0; i < 3000; ++i) {
        sys.clock.advance(5);
        sys.publisher.tick();
        if (sys.pollValve(5, sys.clock.nowMs()) != 1) {
            ++ceros;
        }
    }
    TEST_ASSERT_EQUAL_UINT32(0, ceros);
}

/// Un reinicio con la valvula regando republica 1, porque el estado se
/// reconstruye del interruptor fisico. Y durante el reinicio el esclavo no
/// responde: una expiracion NO es un 0, y el maestro no dispara cierre.
void test_un_reinicio_republica_el_riego_en_curso(void) {
    System sys;
    sys.boot();
    sys.inputs.requestOpen(6);
    sys.settle();
    TEST_ASSERT_EQUAL_UINT16(1, sys.pollValve(6, sys.clock.nowMs()));

    // Reinicio: banco a cero y siembra de nuevo, con el interruptor intacto.
    System reiniciado;
    reiniciado.inputs.requestOpen(6);
    reiniciado.boot(sys.clock.nowMs() + 2000);

    TEST_ASSERT_EQUAL_UINT16(1, reiniciado.pollValve(6, reiniciado.clock.nowMs()));
}

/// FR-116 al cruzar el desbordamiento de millis() a los 49,7 dias. Ninguna
/// suite cubria este caso de extremo a extremo.
void test_el_desbordamiento_del_reloj_no_altera_el_riego(void) {
    System sys;
    // Arranca a 30 segundos del desbordamiento de 32 bits.
    const uint32_t antesDelWrap = 0xFFFFFFFFUL - 30000UL;
    sys.boot(antesDelWrap);
    sys.inputs.requestOpen(4);
    sys.settle();
    TEST_ASSERT_EQUAL_UINT16(1, sys.registers.get(config::valveRegister(4)));

    // Cruza el desbordamiento sondeando sin parar.
    uint32_t ceros = 0;
    for (uint32_t i = 0; i < 12000; ++i) {
        sys.clock.advance(5);
        sys.publisher.tick();
        if (sys.registers.get(config::valveRegister(4)) != 1) {
            ++ceros;
        }
    }
    TEST_ASSERT_EQUAL_UINT32(0, ceros);

    // Y sigue respondiendo correctamente al otro lado del desbordamiento.
    TEST_ASSERT_EQUAL_UINT16(1, sys.pollValve(4, sys.clock.nowMs()));
}

/// El antirrebote no deja pasar un microcorte del contacto. Sin el, cualquier
/// rebote publicaria un 0 y cortaria el riego.
void test_un_rebote_del_contacto_no_corta_el_riego(void) {
    System sys;
    sys.boot();
    sys.inputs.requestOpen(7);
    sys.settle();
    TEST_ASSERT_EQUAL_UINT16(1, sys.registers.get(config::valveRegister(7)));

    // Microcorte de 10 ms, por debajo de la ventana de antirrebote de 50 ms.
    sys.inputs.requestClose(7);
    for (uint8_t i = 0; i < 2; ++i) {
        sys.clock.advance(5);
        sys.publisher.tick();
        TEST_ASSERT_EQUAL_UINT16(1, sys.registers.get(config::valveRegister(7)));
    }
    sys.inputs.requestOpen(7);
    sys.settle();

    TEST_ASSERT_EQUAL_UINT16(1, sys.registers.get(config::valveRegister(7)));
}

// =============================================================================
//  Panel y diagnostico
// =============================================================================

/// FR-114b: el panel solo muestra 0 y 1. Los simbolos de espera y fallo se
/// retiraron porque no hay confirmacion del campo que los respalde.
void test_el_panel_solo_muestra_cero_y_uno(void) {
    System sys;
    sys.boot();
    sys.inputs.requestOpen(1);
    sys.settle();

    TEST_ASSERT_EQUAL_CHAR('1', sys.display.symbol(1));
    TEST_ASSERT_EQUAL_CHAR('0', sys.display.symbol(0));
}

void test_el_diagnostico_cuenta_las_valvulas_regando(void) {
    System sys;
    sys.boot();
    // Una por UC511 instalado, para que la separacion de la spec 003 no las
    // escalone.
    sys.inputs.requestOpen(0);   // UC511 #01
    sys.inputs.requestOpen(2);   // UC511 #02
    sys.inputs.requestOpen(4);   // UC511 #03
    sys.settle();

    sys.publisher.publishDiagnostics(0, 0, 0, 0, 0);
    TEST_ASSERT_EQUAL_UINT16(3, sys.registers.get(config::diag::kValvesIrrigating));
}

}  // namespace

void run_publica_estado_tests(void) {
    RUN_TEST(test_el_arranque_siembra_desde_los_pines_fisicos);
    RUN_TEST(test_un_cambio_de_interruptor_se_publica_tras_el_antirrebote);
    RUN_TEST(test_solo_cambia_la_valvula_del_interruptor_movido);
    RUN_TEST(test_el_tablero_es_activo_en_alto);
    RUN_TEST(test_el_reposo_no_pide_riego_en_ninguna_valvula);
    RUN_TEST(test_las_24_valvulas_se_publican);
    RUN_TEST(test_riego_sostenido_nunca_devuelve_cero);
    RUN_TEST(test_un_reinicio_republica_el_riego_en_curso);
    RUN_TEST(test_el_desbordamiento_del_reloj_no_altera_el_riego);
    RUN_TEST(test_un_rebote_del_contacto_no_corta_el_riego);
    RUN_TEST(test_el_panel_solo_muestra_cero_y_uno);
    RUN_TEST(test_el_diagnostico_cuenta_las_valvulas_regando);
}
