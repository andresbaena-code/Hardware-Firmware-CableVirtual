# Tasks: Separación de comandos por UC511

**Input**: documentos de diseño en `specs/003-retardo-apertura-cancelable/`
**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md),
[data-model.md](data-model.md), [contracts/command-spacer.md](contracts/command-spacer.md),
[quickstart.md](quickstart.md)

**Tests**: OBLIGATORIAS. Las pide FR-217 y el Principio IV (NO NEGOCIABLE). Cada prueba
se escribe **antes** que el código que verifica y debe fallar primero. Toda prueba lleva
el `FR-2xx` en su nombre. Vectores = tabla 5 de [contracts/command-spacer.md](contracts/command-spacer.md).

**Comandos** (en esta máquina `pio` no está en el PATH):

```text
PIO = C:/Users/USUARIO/.platformio/penv/Scripts/platformio.exe
$PIO test -e native
$PIO run -e megaatmega2560
```

**Organización**: por historia de usuario de [spec.md](spec.md). Orden de ejecución:
US1 → US2 → US5 (las tres P1) → US3 → US4 (P2).

## Format: `[ID] [P?] [Story] Description`

- **[P]**: se puede hacer en paralelo (archivos distintos, sin dependencias pendientes)
- **[Story]**: historia de usuario a la que pertenece (US1..US5)

---

## Phase 1: Setup

**Purpose**: línea base y andamiaje de pruebas.

- [ ] T001 Registrar la línea base: `$PIO test -e native` (92/92) y `$PIO run -e megaatmega2560` (RAM 1731 B / 21,1 %, Flash 12304 B), anotándola en las notas de la tarea al cerrarla
- [ ] T002 [P] Crear `test/test_control/test_command_spacer.cpp` con la función `run_command_spacer_tests()` vacía y registrarla en `test/test_control/main_test_control.cpp`
- [ ] T003 [P] Crear `test/test_integration/test_separacion_uc511.cpp` con la función `run_separacion_uc511_tests()` vacía y registrarla en `test/test_integration/main_test_integration.cpp`

---

## Phase 2: Foundational (bloquea todas las historias)

**Purpose**: mapa de equipos, constantes de tiempo, etiquetas de traza y la interfaz del
separador. Nada de lógica de turnos todavía.

**⚠️ CRITICAL**: ninguna historia empieza antes de terminar esta fase.

### Pruebas de la fase (escribir primero, deben fallar)

- [ ] T004 [P] En `test/test_domain/test_config.cpp`, pruebas `test_FR201_cada_valvula_declara_equipo_y_salida`, `test_FR201_mapa_piloto_v01_a_v08_en_uc511_1_a_4` (tabla 1.5 de [data-model.md](data-model.md)), `test_FR203_equipos_1_a_4_instalados_5_a_12_no`, `test_FR214_separacion_no_menor_que_minimo_del_actuador`, y registrarlas en el runner de la suite

### Implementación

- [ ] T005 [P] En `lib/wiga_config/timing_config.h`, añadir `kActuatorMinGapMs = 5000UL` y `kCommandSpacingMs = 8000UL` con comentario del porqué (medida de campo del UC511, 3 s de margen por la latencia del UC100), anotación `FR-214` y `static_assert` `TC-01: kCommandSpacingMs >= kActuatorMinGapMs`
- [ ] T006 En `lib/wiga_config/installation_map.h`, añadir a `ValveMapping` los campos `actuator` y `output` (`uint8_t`), rellenar las 24 filas de `kMap` según la tabla 1.5 de [data-model.md](data-model.md) (pares consecutivos: índices 2k y 2k+1 → equipo k+1, salidas 1 y 2), con anotación `FR-201`
- [ ] T007 En `lib/wiga_config/installation_map.h`, declarar `struct ActuatorConfig { bool installed; }`, el arreglo `kActuators[]` de 12 equipos (1–4 `true`, 5–12 `false`), `kActuatorCount` derivado de su tamaño y las funciones `constexpr` `actuatorOf(index)`, `outputOf(index)` y `actuatorInstalled(actuator)`, con anotación `FR-203`
- [ ] T008 En `lib/wiga_config/installation_map.h`, añadir las validaciones `constexpr` y los `static_assert` `MAP-V13..MAP-V17` descritos en [research.md R-302](research.md), con anotación `FR-202`
- [ ] T009 Comprobar que ningún otro inicializador de `ValveMapping` (incluido `lib/wiga_config/installation_map.cpp` y las pruebas) se rompe por los campos nuevos; ajustar los que existan
- [ ] T010 [P] En `lib/wiga_ports/i_log_port.h`, añadir a `logtag` `kWait = "WAIT"`, `kCancel = "CANC"` y `kNotInstalled = "NOINST"` con su comentario, anotación `FR-216`
- [ ] T011 Crear `lib/wiga_control/command_spacer.h` con la interfaz de la sección 1 de [contracts/command-spacer.md](contracts/command-spacer.md) (`begin`, `update` → `Outcome { Unchanged, Queued, Cancelled }`, `next`, `published`, `pending`), el estado de la sección 2 de [data-model.md](data-model.md) en arreglos estáticos dimensionados con `config::kValveCount` y `config::kActuatorCount`, sin `<Arduino.h>` ni memoria dinámica; cabecera con anotaciones `FR-204..FR-211`
- [ ] T012 Crear `lib/wiga_control/command_spacer.cpp` con implementaciones mínimas que compilen (todo devuelve "sin cambios") para que la suite enlace
- [ ] T013 Ejecutar `$PIO test -e native`: T004 pasa, las 92 pruebas previas siguen pasando

**Checkpoint**: mapa de equipos validado en compilación; el separador existe pero no hace nada.

---

## Phase 3: User Story 1 — Cambio de válvula dentro de un mismo bloque (Priority: P1) 🎯 MVP

**Goal**: dos cambios del mismo UC511 nunca se publican con menos de 8 s; el cierre sale
primero.

**Independent Test**: bajar V01 y subir V02 en la misma vuelta: registro 0 a `0` al
instante, registro 1 a `1` exactamente 8 s después.

### Tests for User Story 1 (escribir primero, deben fallar)

- [ ] T014 [P] [US1] En `test/test_control/test_command_spacer.cpp`: `test_FR205_cambio_con_equipo_libre_se_publica_en_la_misma_vuelta` (vector 1), `test_FR204_apertura_en_el_mismo_equipo_espera_el_intervalo` (vector 2, comprobar `0` en t=67999 y `1` en t=68000), `test_FR207_cierre_sale_antes_que_apertura_pendiente` (vector 3), `test_FR210_intervalo_correcto_con_desbordamiento_del_reloj` (vector 9), `test_FR204_maximo_un_cambio_por_equipo_y_vuelta`
- [ ] T015 [P] [US1] En `test/test_integration/test_separacion_uc511.cpp`: `test_FR204_del_pin_al_registro_cambio_v01_a_v02` con `FakeInputs`, `FakeClock`, `FakeDisplay`, `FakeLog` y `RegisterFile`, pasando el antirrebote real; comprobar registros 0 y 1 y la traza `WAIT` para V02

### Implementation for User Story 1

- [ ] T016 [US1] En `lib/wiga_control/command_spacer.cpp`, implementar `update()` (crear pendiente cuando deseado ≠ publicado; cancelar cuando vuelve a igualarse) y `next()` según [research.md R-304](research.md): equipo libre si `!hasLastChange` o `wiga::elapsed(now, lastChangeMs, kCommandSpacingMs)`; dentro del equipo, cierre antes que apertura, luego `pendingSinceMs` más antiguo, luego menor índice; un cambio por equipo y vuelta (reglas SP-01, SP-02, SP-04, SP-06, SP-07)
- [ ] T017 [US1] En `lib/wiga_control/valve_publisher.h`, añadir un miembro `CommandSpacer spacer_` y actualizar la cabecera de anotaciones con `FR-204`, `FR-205`, `FR-209`
- [ ] T018 [US1] En `lib/wiga_control/valve_publisher.cpp`, reescribir `pollInputs()`: para cada válvula el deseado es `panelRequest(debouncers_[i].stable())` y se informa a `spacer_.update()` en **cada** vuelta (no solo en el flanco); después drenar `spacer_.next()` y escribir el registro con traza `TX`; al final de la vuelta, traza `WAIT` para cada válvula que quedó pendiente en esta vuelta (VP-32, VP-33). El registro solo se escribe desde `next()`
- [ ] T019 [US1] Ejecutar `$PIO test -e native`: T014, T015 y las 92 previas pasan

**Checkpoint**: el cambio dentro de un bloque ya no pierde comandos. Es el MVP del piloto.

---

## Phase 4: User Story 2 — Equipos distintos no se retrasan (Priority: P1)

**Goal**: la separación solo actúa dentro de un equipo.

**Independent Test**: bajar V02 (UC511 #01) y subir V03 (UC511 #02) en la misma vuelta:
los dos registros cambian en esa vuelta.

### Tests for User Story 2

- [ ] T020 [P] [US2] En `test/test_control/test_command_spacer.cpp`: `test_FR208_equipos_distintos_no_se_esperan` (vector 4) y `test_FR208_espera_en_un_equipo_no_retrasa_otro`
- [ ] T021 [P] [US2] En `test/test_integration/test_separacion_uc511.cpp`: `test_FR208_del_pin_al_registro_cambio_v02_a_v03_sin_espera`

### Implementation for User Story 2

- [ ] T022 [US2] Ajustar `lib/wiga_control/command_spacer.cpp` solo si T020/T021 fallan (el estado por equipo de T016 ya debería cumplirlo); ejecutar `$PIO test -e native`

**Checkpoint**: la secuencia del cliente pierde 8 s solo en la segunda válvula de cada bloque.

---

## Phase 5: User Story 5 — Un reinicio no cierra válvulas que riegan (Priority: P1)

**Goal**: la siembra publica sin esperar y ocupa los equipos con alguna válvula en `1`.

**Independent Test**: arrancar con V01 en `1`; el registro 0 vale `1` antes del primer
sondeo; V02 pedida a los 2 s se publica a los 8 s.

### Tests for User Story 5

- [ ] T023 [P] [US5] En `test/test_control/test_command_spacer.cpp`: `test_FR211_siembra_publica_sin_esperar` y `test_FR211_siembra_ocupa_solo_equipos_con_valvula_abierta` (vector 10)
- [ ] T024 [P] [US5] En `test/test_integration/test_separacion_uc511.cpp`: `test_FR211_reinicio_con_v01_regando_registro_en_1_antes_del_bus` y `test_FR211_cambio_tras_arranque_respeta_el_intervalo`

### Implementation for User Story 5

- [ ] T025 [US5] En `lib/wiga_control/command_spacer.cpp`, implementar `begin()` según [research.md R-305](research.md) (regla SP-08)
- [ ] T026 [US5] En `lib/wiga_control/valve_publisher.cpp`, en `begin()`, sembrar `spacer_` con el mismo estado que se escribe en el banco y con el mismo instante, antes de `refreshDisplay()` (VP-35)
- [ ] T027 [US5] Ejecutar `$PIO test -e native`, incluidas las pruebas FR-116 de la spec 002 en `test/test_integration/test_publica_estado.cpp`

**Checkpoint**: las tres historias P1 completas.

---

## Phase 6: User Story 3 — Pedido breve o revertido (Priority: P2)

**Goal**: los pendientes se cancelan sin consumir turno; ningún cambio se pierde ni espera
más de 16 s.

**Independent Test**: con el UC511 #01 ocupado, subir V02 y bajarla a los 3 s: el
registro 1 nunca vale `1`.

### Tests for User Story 3

- [ ] T028 [P] [US3] En `test/test_control/test_command_spacer.cpp`: `test_FR206_apertura_pendiente_se_cancela` (vector 5), `test_FR206_cierre_pendiente_se_cancela_sin_pasar_por_cero` (vector 7), `test_FR206_cancelado_no_consume_turno`, `test_FR204_cierre_pedido_3s_despues_de_abrir_se_publica_a_los_8s` (vector 6), `test_FR207_dos_aperturas_simultaneas_primero_menor_indice` (vector 8), `test_FR207_espera_maxima_dos_intervalos` (vector 12, recorrer secuencias de pedidos cruzados y comprobar que ningún pendiente supera 16000 ms), `test_FR206_nuevo_pedido_tras_cancelar_cuenta_desde_cero`
- [ ] T029 [P] [US3] En `test/test_integration/test_separacion_uc511.cpp`: `test_FR206_del_pin_al_registro_pedido_revertido_no_publica` con comprobación de la traza `CANC`

### Implementation for User Story 3

- [ ] T030 [US3] En `lib/wiga_control/command_spacer.cpp`, completar la cancelación (SP-03) y el orden por antigüedad (SP-04, SP-09) si T028 falla
- [ ] T031 [US3] En `lib/wiga_control/valve_publisher.cpp`, emitir la traza `CANC` cuando `update()` devuelva `Cancelled` (VP-33)
- [ ] T032 [US3] Ejecutar `$PIO test -e native`

**Checkpoint**: los dos huecos de la versión 1 (pedidos de 8–13 s y pedidos simultáneos) cubiertos.

---

## Phase 7: User Story 4 — Equipos no instalados (Priority: P2)

**Goal**: las válvulas de equipos no instalados publican siempre `0`, se ven como `-` y
avisan una vez por pedido.

**Independent Test**: subir V09 (UC511 #05): registro 8 en `0`, una traza `NOINST`,
panel `-`.

### Tests for User Story 4

- [ ] T033 [P] [US4] En `test/test_integration/test_separacion_uc511.cpp`: `test_FR212_valvula_de_equipo_no_instalado_publica_siempre_cero` (vector 11), `test_FR213_aviso_noinst_una_vez_por_pedido` (mantener la entrada arriba varias vueltas y contar una sola traza; bajar y subir de nuevo, contar dos), `test_FR215_panel_muestra_guion_en_equipo_no_instalado` con `FakeDisplay`, `test_FR212_siembra_de_equipo_no_instalado_queda_en_cero`

### Implementation for User Story 4

- [ ] T034 [US4] En `lib/wiga_control/valve_publisher.h`, añadir `symbol::kNotInstalled = '-'` con comentario (configuración, no confirmación del campo: FR-215, FR-114b de la spec 002)
- [ ] T035 [US4] En `lib/wiga_control/valve_publisher.cpp`, forzar el deseado a `Closed` cuando `!config::actuatorInstalled(config::actuatorOf(i))`, también en `begin()`; emitir `NOINST` en el flanco del antirrebote hacia "pedido" de esas válvulas (VP-31, VP-33)
- [ ] T036 [US4] En `lib/wiga_control/valve_publisher.cpp`, en `refreshDisplay()`, mostrar `symbol::kNotInstalled` para las válvulas de equipos no instalados (VP-34)
- [ ] T037 [US4] Ejecutar `$PIO test -e native`

**Checkpoint**: todas las historias completas y verificadas en host.

---

## Phase 8: Polish & Cross-Cutting

- [ ] T038 Compilar `$PIO run -e megaatmega2560` y `$PIO run -e megaatmega2560_silent` sin errores ni advertencias nuevas; anotar RAM y Flash (RAM < 75 %, esperado ≈ 24 %)
- [ ] T039 Comprobación manual de MAP-V15 (quickstart nivel 1): duplicar `actuator`/`output` de V01 en V03, compilar, confirmar el error `MAP-V15`, deshacer
- [ ] T040 Ejecutar `python ../.github/scripts/check_traceability.py ..` (o el intérprete disponible) y confirmar que FR-201..FR-218 aparecen citados
- [ ] T041 [P] Actualizar `docs/piloto-8-valvulas-d2d.md` y su generador `scripts/gen_piloto_d2d.py` al diseño acordado: una acción por regla del UC100, sin *Delay Time*, `Lock time` según lo medido, `Duration` del UC511 como red de seguridad, efecto de la separación en el pulso (6 min 52 s en la segunda válvula del bloque), mapa válvula → UC511 — cubre FR-218
- [ ] T042 [P] Actualizar `docs/configuracion-radio-d2d.md` y `scripts/gen_configuracion_d2d.py` con los mismos tiempos — cubre FR-218
- [ ] T043 [P] Actualizar `docs/configuracion-finca.md`: cómo declarar `actuator`, `output` y `installed` para una finca nueva y qué significan los errores `MAP-V13..V17`
- [ ] T044 [P] Actualizar `docs/ARQUITECTURA_DEL_FIRMWARE.md` y la sección de estructura de `AGENTS.md` con `CommandSpacer`
- [ ] T045 [P] Añadir a `docs/diagnostico.md` las trazas `WAIT`, `CANC`, `NOINST` y el símbolo `-` del panel
- [ ] T046 [P] Añadir a `docs/acta-pruebas-banco.md` un bloque "Spec 003" con las tablas de banco B-1..B-6 y campo C-1..C-6 de [quickstart.md](quickstart.md)
- [ ] T047 Pruebas de banco B-1..B-6 de [quickstart.md](quickstart.md) en la placa real (con el responsable técnico) y registro en el acta
- [ ] T048 Pruebas de campo C-1..C-6 de [quickstart.md](quickstart.md) en el piloto, verificando antes que el cableado coincide con el mapa, y registro en el acta

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (1)** → **Foundational (2)** → historias → **Polish (8)**
- T047 y T048 requieren el firmware cargado en placa y el acta de T046.

### User Story Dependencies

| Historia | Depende de | Notas |
|---|---|---|
| US1 (P1) | Fase 2 | Implementa el núcleo de `CommandSpacer` y la integración |
| US2 (P1) | US1 | Normalmente solo pruebas: el diseño ya lo cumple |
| US5 (P1) | US1 | Añade `begin()` al separador y al publicador |
| US3 (P2) | US1 | Cancelación y orden ya esbozados en T016; aquí se blindan |
| US4 (P2) | Fase 2 + US1 | Toca el deseado y el panel en `ValvePublisher` |

US2, US5, US3 y US4 son independientes entre sí una vez terminada US1, salvo que
comparten `valve_publisher.cpp` (US5, US3 y US4): hacerlas en serie para no pisarse.

### Within Each User Story

- Pruebas primero, y deben fallar.
- `command_spacer.cpp` antes que `valve_publisher.cpp`.
- Cerrar cada historia con `$PIO test -e native` en verde.

### Parallel Opportunities

- T002 y T003 (archivos distintos).
- T004, T005 y T010 en la fase 2.
- Dentro de cada historia, la prueba unitaria y la de integración ([P]).
- En la fase 8, todas las tareas de documentación T041–T046.

## Parallel Example: User Story 1

```text
T014 pruebas unitarias en test/test_control/test_command_spacer.cpp
T015 prueba de integración en test/test_integration/test_separacion_uc511.cpp
```

## Implementation Strategy

### MVP (User Story 1)

1. Fases 1 y 2.
2. Fase 3 (US1).
3. **Parar y validar**: suite en verde y prueba B-2 de banco. Con esto el piloto ya no
   pierde comandos en el cambio de válvula dentro de un bloque.

### Entrega incremental

1. MVP (US1) → US2 → US5: las tres P1, listas para el piloto.
2. US3 → US4: robustez ante pedidos breves y equipos no instalados.
3. Fase 8: documentación, banco, campo.

Un commit por fase como mínimo, con la suite en verde y RAM/Flash en el mensaje, en la
rama `003-retardo-apertura-cancelable`.
