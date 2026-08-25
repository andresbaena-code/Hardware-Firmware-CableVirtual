---
description: "Lista de tareas — Migración del firmware Cable Virtual a PlatformIO"
---

# Tasks: Migración del firmware Cable Virtual a PlatformIO

**Input**: Documentos de diseño en `/specs/001-migracion-platformio/`

**Prerequisites**: [plan.md](./plan.md), [spec.md](./spec.md), [research.md](./research.md),
[data-model.md](./data-model.md), [contracts/](./contracts/)

**Tests**: **OBLIGATORIOS.** El Principio IV de la constitución es NO NEGOCIABLE: toda regla
de negocio lleva prueba ejecutable en el entorno `native`, escrita junto al código que
verifica, no después.

**Organization**: las tareas se agrupan por historia de usuario para poder implementar y
validar cada una de forma independiente.

## Format: `[ID] [P?] [Story] Descripción`

- **[P]**: puede ejecutarse en paralelo (archivos distintos, sin dependencias pendientes)
- **[Story]**: historia de usuario a la que pertenece (US1..US6)
- Cada tarea indica la ruta exacta del archivo y los requisitos que cubre
- **Ruta base**: todas las rutas son relativas a la raíz del proyecto PlatformIO
  (`CABLE_VIRTUAL_V2_PIO/`), salvo `.github/`, que por exigencia de GitHub Actions vive en
  la raíz del repositorio

---

## Phase 1: Setup (Infraestructura compartida)

**Purpose**: dejar el proyecto PlatformIO en pie, con ambos entornos y las dependencias
fijadas, y archivar el material heredado antes de tocar nada.

- [X] T001 Crear el esqueleto de directorios del proyecto (`src/`, `lib/wiga_config/`, `lib/wiga_domain/`, `lib/wiga_control/`, `lib/wiga_ports/`, `lib/wiga_hal/`, `test/`, `docs/`) según la estructura de [plan.md](./plan.md)
- [X] T002 Crear `platformio.ini` con el entorno `megaatmega2560` (framework arduino, `-std=gnu++14` vía `build_unflags`/`build_flags`) y las dependencias fijadas `andrewrapp/XBee@0.0.0-alpha+sha.39f236ea18` y `marcoschwartz/LiquidCrystal_I2C@1.1.4` — cubre FR-030, R-001, R-007
- [X] T003 Añadir al `platformio.ini` el entorno `native` (`platform = native`, `test_framework = unity`) con `lib_ldf_mode = chain+` y `build_src_filter` que **excluya** `lib/wiga_hal`, de modo que la barrera de capas sea estructural — cubre FR-031, CP-03, Principio III
- [X] T004 [P] **Dejar los tres sketches originales donde están, en la raíz del repositorio, sin modificarlos ni moverlos.** Son el material entregado por el cliente. PlatformIO no los compila (solo construye `src/` y `lib/`) — cubre FR-033
- [X] T005 [P] Extender `.gitignore` con los artefactos de PlatformIO (`.pio/`, `.vscode/ipch/`, `compile_commands.json`)
- [X] T006 [P] Crear `.vscode/extensions.json` recomendando `platformio.platformio-ide`, para que VS Code proponga la extensión al abrir el repositorio — cubre SC-002
- [X] T007 Verificar que `pio run -e megaatmega2560` descarga las dependencias y compila un `src/main.cpp` vacío en un equipo limpio — cubre escenario 1 de [quickstart.md](./quickstart.md)

**Checkpoint**: el proyecto compila vacío y las dependencias se resuelven solas.

---

## Phase 2: Foundational (Prerrequisitos bloqueantes)

**Purpose**: tipos, configuración, contratos de puertos y dobles de prueba. Nada de esto es
específico de una historia, y **todo** lo demás depende de ello.

**⚠ CRÍTICO**: ninguna historia de usuario puede comenzar hasta completar esta fase.

### Tipos y configuración

- [X] T008 Implementar `lib/wiga_domain/valve_types.h` con `SignalLevel`, `ValveState`, `ValveIndex` y las funciones `constexpr toValveState()` / `toSignalLevel()` que aplican la lógica inversa por transistor — cubre FR-021, FR-021b, invariantes V-01/V-02
- [X] T009 Escribir `test/test_domain/test_valve_types.cpp` verificando la ida y vuelta de la conversión (V-01) y la tabla de inversión de [contracts/application-protocol.md §4](./contracts/application-protocol.md) — cubre FR-021
- [X] T010 [P] Implementar `lib/wiga_config/timing_config.h` con todas las constantes temporales de [data-model.md §2.2](./data-model.md), tipadas explícitamente y con unidad en el nombre, más los `static_assert` de C-05 y C-06 — cubre FR-023, R-005
- [X] T011 Implementar `lib/wiga_config/installation_map.h` con `ValveMapping`, el arreglo `constexpr detail::kMap` transcrito de [contracts/installation-map.md §4](./contracts/installation-map.md) y `kValveCount` derivado con `sizeof` — cubre FR-023, FR-024, MAP-01, MAP-02
- [X] T012 Añadir a `lib/wiga_config/installation_map.h` las funciones `constexpr` de detección de duplicados y los seis `static_assert` MAP-V1..MAP-V6 con mensajes en español — cubre FR-025, C-01..C-04
- [X] T013 Escribir `test/test_domain/test_config.cpp` verificando que las funciones `constexpr` de detección devuelven el resultado correcto sobre mapas de ejemplo con y sin colisiones — cubre FR-025

### Protocolo de aplicación

- [X] T014 Implementar `lib/wiga_domain/app_protocol.h/.cpp` con `ValveCommand`, `ValveReport`, `encodeCommand()` (5 bytes con verificador) y `decodeReport()` con las validaciones RX-01..RX-04 — cubre FR-015, FR-016, R-012
- [X] T015 Escribir `test/test_domain/test_app_protocol.cpp` con los dos ejemplos normativos de [contracts/application-protocol.md §2](./contracts/application-protocol.md) (`AA 14 03 00 3E` y `AA 14 07 01 39`), el rechazo de tramas cortas, encabezado inválido, verificador incorrecto y nivel fuera de rango, y la aceptación tolerante de tramas de 4 bytes (RX-03) — cubre FR-015, FR-016, P-01, P-02

### Puertos y dobles de prueba

- [X] T016 [P] Declarar `lib/wiga_ports/i_clock.h` según [contracts/ports.md §1](./contracts/ports.md)
- [X] T017 [P] Declarar `lib/wiga_ports/i_digital_input_port.h` según [contracts/ports.md §2](./contracts/ports.md)
- [X] T018 [P] Declarar `lib/wiga_ports/i_radio_port.h` según [contracts/ports.md §3](./contracts/ports.md)
- [X] T019 [P] Declarar `lib/wiga_ports/i_display_port.h` según [contracts/ports.md §4](./contracts/ports.md)
- [X] T020 [P] Declarar `lib/wiga_ports/i_watchdog_port.h` según [contracts/ports.md §5](./contracts/ports.md)
- [X] T021 [P] Declarar `lib/wiga_ports/i_log_port.h` con `LogLevel` y los ocho tags obligatorios según [contracts/ports.md §6](./contracts/ports.md) — cubre FR-026, FR-027, FR-028
- [X] T022 [P] Implementar `test/mocks/fake_clock.h` con avance manual del tiempo, capaz de posicionarse cerca del desbordamiento de 32 bits
- [X] T023 [P] Implementar `test/mocks/fake_inputs.h` con niveles fijados por prueba y capacidad de simular rebote
- [X] T024 [P] Implementar `test/mocks/fake_radio.h` que registra órdenes transmitidas, permite encolar confirmaciones arbitrarias (válidas, corruptas y de nodos desconocidos) y **falla la prueba si alguien intenta vaciar el búfer** — cubre FR-018, RD-02
- [X] T025 [P] Implementar `test/mocks/fake_display.h` que expone el buffer para aserciones y `test/mocks/fake_watchdog.h` que cuenta invocaciones de `feed()` — cubre WD-03
- [X] T026 [P] Implementar `test/mocks/fake_log.h` que acumula eventos para aserciones

**Checkpoint**: tipos, configuración validada, protocolo probado y todos los puertos con su
doble. Las historias de usuario pueden comenzar.

---

## Phase 3: User Story 2 — Compilar y cargar desde VS Code (Priority: P1)

**Goal**: el técnico clona, abre en VS Code, compila y carga sin pasos manuales.

**Independent Test**: escenario 1 de [quickstart.md](./quickstart.md) en un equipo limpio.

- [X] T027 [US2] Crear `lib/wiga_hal/firmware_version.h` con la versión semántica del firmware y las macros `__DATE__`/`__TIME__` de compilación, con las cadenas en Flash — cubre FR-026, LG-03
- [X] T028 [US2] Crear `src/main.cpp` como raíz de composición con `setup()`/`loop()` mínimos que inicializan la consola y emiten el evento `VER` al arrancar — cubre FR-026, CP-01, CP-02
- [X] T029 [US2] Verificar `pio run -e megaatmega2560` y registrar el reporte de RAM y Flash; confirmar RAM < 75 % — cubre FR-030, NFR-001
- [X] T030 [US2] Escribir `docs/puesta-en-marcha.md` con requisitos del entorno, clonado, compilación, carga y solución de problemas frecuentes — cubre FR-034, SC-002

**Checkpoint**: cualquiera puede compilar y cargar el firmware; arranca e identifica su versión.

---

## Phase 4: User Story 1 — El campo sigue al tablero (Priority: P1) 🎯 MVP

**Goal**: las 24 entradas del tablero operan sus válvulas con disparo secuencial,
reintentos acotados y confirmación obligatoria.

**Independent Test**: escenarios 7, 8 y 9 de [quickstart.md](./quickstart.md) en banco.

### Lógica de control (con sus pruebas, Principio IV)

- [X] T031 [P] [US1] Implementar `lib/wiga_control/debouncer.h/.cpp` con el filtro de estabilidad por entrada e invariantes D-01/D-02 — cubre FR-002
- [X] T032 [P] [US1] Escribir `test/test_control/test_debouncer.cpp`: un cambio sostenido `kDebounceMs` se acepta; una oscilación más corta se rechaza y reinicia la ventana — cubre FR-002
- [X] T033 [P] [US1] Implementar `lib/wiga_domain/valve_record.h` con los siete campos de [data-model.md §3](./data-model.md) e invariantes R-01..R-04 — cubre FR-009
- [X] T034 [US1] Implementar `lib/wiga_control/retry_policy.h/.cpp` con intentos acotados, intervalo corto, reposo prolongado y cancelación por cambio de entrada, usando `(uint32_t)(now - mark)` en toda comparación — cubre FR-006, FR-007, FR-008, FR-013
- [X] T035 [US1] Escribir `test/test_control/test_retry_policy.cpp`: exactamente 5 intentos separados 30 s, transición a reposo de 60 min al agotarlos, y cancelación inmediata del reposo al cambiar la entrada — cubre FR-006, FR-007, FR-008
- [X] T036 [US1] Implementar `lib/wiga_control/latch_scheduler.h/.cpp` con la ventana de exclusión y el barrido circular desde `lastServed + 1` — cubre FR-004, invariantes S-01, S-02
- [X] T037 [US1] Escribir `test/test_control/test_scheduler.cpp`: nunca concede dos turnos dentro de `kLatchGuardMs`, y con varias válvulas candidatas reparte los turnos de forma circular en lugar de favorecer siempre la de índice menor — cubre FR-004, S-02
- [X] T038 [US1] Implementar `lib/wiga_control/valve_supervisor.h/.cpp` con la máquina de estados de las 11 transiciones T1..T11 de [data-model.md §4](./data-model.md), orquestando entradas, filtro, política, planificador, radio y panel a través de los puertos — cubre FR-001, FR-003, FR-005, FR-009, FR-010, FR-011, FR-017
- [X] T039 [US1] Escribir `test/test_integration/test_supervisor.cpp` cubriendo las 11 transiciones contra los dobles, incluida T11 (la orden se cancela si la entrada vuelve antes del turno) y T5 (confirmación con nivel distinto no confirma) — cubre FR-003, FR-005, T1..T11
- [X] T040 [US1] Escribir `test/test_integration/test_no_burst_on_boot.cpp`: tras sembrar el estado desde entradas físicas, el supervisor no transmite ninguna orden — cubre FR-010, Principio II
- [X] T041 [US1] Escribir `test/test_integration/test_fault_isolation.cpp`: una válvula en reposo prolongado no impide que las otras 23 sean atendidas sin retraso — cubre FR-009

### Adaptadores de hardware

- [X] T042 [P] [US1] Implementar `lib/wiga_hal/arduino_clock.h` sobre `millis()` (solo cabecera: el adaptador es una linea) — cubre CLK-01..CLK-03
- [X] T043 [P] [US1] Implementar `lib/wiga_hal/arduino_digital_input.h/.cpp` con `INPUT_PULLUP` sobre los pines del mapa, devolviendo `SignalLevel` crudo — cubre FR-001, IN-01..IN-04
- [X] T044 [US1] Implementar `lib/wiga_hal/xbee_radio_adapter.h/.cpp`: `send()` **sin vaciar el búfer de recepción**, y `poll()` que invoca `readPacket()` en bucle hasta agotar el búfer o alcanzar `kMaxFramesPerPoll` — cubre FR-014, FR-017, FR-018, RD-01..RD-07, R-002, R-003
- [X] T045 [P] [US1] Implementar `lib/wiga_hal/avr_watchdog.h/.cpp` con `wdt_disable()`, `wdt_enable(WDTO_8S)` y `wdt_reset()` — cubre FR-012, WD-01..WD-04
- [X] T046 [US1] Cablear en `src/main.cpp` la composición completa: instanciar adaptadores, inyectarlos en `ValveSupervisor` y ejecutar el lazo `feed watchdog → supervisor.tick() → display.tick()`, sin `delay()` — cubre FR-011, FR-012, CP-01, CP-02
- [X] T047 [US1] Verificar en banco los escenarios 7, 8 y 9 de [quickstart.md](./quickstart.md) y registrar la tabla de 24 entradas — cubre SC-001, SC-005, NFR-003

**Checkpoint**: MVP funcional. El sistema replica el tablero en campo con protección de batería.

---

## Phase 5: User Story 3 — Verificar la lógica sin hardware (Priority: P2)

**Goal**: la suite completa corre en el equipo del desarrollador en menos de 30 segundos.

**Independent Test**: escenarios 2 y 3 de [quickstart.md](./quickstart.md).

- [X] T048 [US3] Escribir `test/test_control/test_time_wraparound.cpp` posicionando el reloj falso cerca de `0xFFFFFFFF` y comprobando que ninguna espera se alarga ni se dispara al desbordar — cubre FR-013, NFR-002, invariante R-04
- [X] T049 [US3] Verificar el escenario 3: añadir temporalmente `#include <Arduino.h>` en `lib/wiga_control/valve_supervisor.h` y confirmar que `pio test -e native` **falla**; revertir y confirmar que pasa — cubre Principio III, CP-03
- [X] T050 [US3] Medir el tiempo total de `pio test -e native` y confirmar que es inferior a 30 s; si no, identificar y corregir la prueba lenta — cubre SC-003
- [X] T051 [US3] Añadir a `docs/puesta-en-marcha.md` la sección de ejecución de pruebas y la explicación de por qué `wiga_hal` no se compila en `native` — cubre FR-034

**Checkpoint**: la lógica es verificable en segundos, sin hardware, y la barrera de capas está demostrada.

---

## Phase 6: User Story 4 — Configurar una instalación nueva (Priority: P2)

**Goal**: poner en marcha una finca nueva toca exactamente un archivo.

**Independent Test**: escenarios 4 y 5 de [quickstart.md](./quickstart.md).

- [X] T052 [US4] Verificar el escenario 4: duplicar un pin y confirmar el fallo de compilación con `pin de entrada duplicado en el mapa`; repetir con un par (dirección, id) duplicado; revertir — cubre FR-025, MAP-V3, MAP-V4
- [ ] T053 [US4] Verificar el escenario 5: sustituir el arreglo `detail::kMap` por un mapa de prueba de 6 válvulas, comprobar que compila y que las pruebas pasan, y que `git diff --name-only` lista **un solo archivo** — cubre FR-023, FR-024, SC-004, MAP-04
- [X] T054 [US4] Escribir `docs/configuracion-finca.md` con el procedimiento de seis pasos de [contracts/installation-map.md §5](./contracts/installation-map.md), incluida la lectura de `SH`/`SL` del XBee — cubre FR-034
- [ ] T055 [US4] Contrastar el mapa heredado contra el plano de la finca, con atención a las seis entradas del nodo `0x41C3F595` (ids 2..6, patrón distinto al resto) y dejar constancia del resultado en `docs/configuracion-finca.md` — cubre R-010, riesgo del plan

**Checkpoint**: una finca nueva se configura sin tocar lógica, y el mapa heredado está verificado.

---

## Phase 7: User Story 5 — Diagnosticar en sitio (Priority: P3)

**Goal**: el técnico determina la causa de una válvula problemática en menos de 10 minutos
con solo la consola serie.

**Independent Test**: escenario 6 de [quickstart.md](./quickstart.md).

- [X] T056 [P] [US5] Implementar `lib/wiga_hal/reset_cause.h/.cpp` capturando `MCUSR` en `.init3` mediante `__attribute__((naked, used, section(".init3")))` y guardándolo en `.noinit` — cubre FR-026, R-008
- [X] T057 [P] [US5] Implementar `lib/wiga_hal/serial_log.h/.cpp` con formato de campos fijos, cadenas en Flash, escritura no bloqueante y compilación condicional por `WIGA_LOG_LEVEL` — cubre FR-027, FR-029, LG-01..LG-04
- [X] T058 [US5] Emitir desde `src/main.cpp` y `ValveSupervisor` los ocho eventos del catálogo (`BOOT`, `VER`, `TX`, `ACK`, `TMO`, `BACK`, `DROP`, `LCD`) en sus puntos correspondientes — cubre FR-026, FR-027, FR-028
- [X] T059 [US5] Añadir el contador `radioFailures` por válvula y su exposición en el evento `BACK` — cubre FR-028
- [X] T060 [US5] Verificar que compilar con `-D WIGA_LOG_LEVEL=0` elimina las trazas sin alterar el tamaño ni el comportamiento de la lógica de control, y registrar la diferencia de Flash — cubre FR-029, LG-02
- [X] T061 [US5] Escribir `docs/diagnostico.md` con el catálogo de eventos, tres fallos típicos y cómo distinguir problema de radio, de entrada o de válvula — cubre FR-034, SC-007
- [ ] T062 [US5] Documentar en `docs/diagnostico.md` la limitación conocida de `optiboot` sobre `MCUSR`, verificando primero en banco si el flag de watchdog llega o no — cubre R-008

**Checkpoint**: la consola cuenta lo que hace el firmware y el técnico puede seguirla.

---

## Phase 8: User Story 6 — Conservar el legado (Priority: P3)

**Goal**: el conocimiento de las versiones anteriores queda en el repositorio.

**Independent Test**: localizar los sketches archivados y su documento de diferencias.

- [X] T063 [US6] Escribir `docs/firmware-anterior.md` indicando, por cada sketch original, a qué instalación corresponde, en qué se diferencia del firmware vigente y por qué no se migró — cubre FR-033
- [X] T064 [US6] Documentar en `docs/firmware-anterior.md` los defectos identificados en las versiones antiguas (`WIGA_24IN_LCDB1_5` declara el LCD como 16x3 pero escribe 4 filas, y `mapear()` recorre `sizeof(direcciones)` en vez del número de elementos) para que nadie los reintroduzca — cubre FR-033

**Checkpoint**: el legado está preservado y explicado, sin cargar mantenimiento.

---

## Phase 9: Panel de estado y pulido transversal

**Purpose**: la interfaz visual y los cierres que cruzan varias historias.

- [X] T065 Implementar `lib/wiga_hal/lcd_display_adapter.h/.cpp` con buffer de sombra 4×20, marcas de suciedad, escritura de `kDisplayCellsPerTick` celdas por `tick()`, bus I2C a 400 kHz y `healthy()` tras fallos consecutivos — cubre FR-019, FR-022, R-007, DP-01..DP-05, invariantes B-01, B-02
- [X] T066 Mapear en `ValveSupervisor` los cuatro símbolos del panel (`0`, `1`, `?`, `!`) derivados del **estado físico**, según [data-model.md §7.1](./data-model.md) — cubre FR-019, FR-020, FR-021, NV-02
- [ ] T067 Verificar el escenario 10 en banco (usar [docs/acta-pruebas-banco.md](../../docs/acta-pruebas-banco.md), bloque 5): instrumentar el tiempo de ciclo y confirmar que ninguna vuelta supera 2 ms con tráfico de radio y refresco de panel activos — cubre Principio I, FR-011
- [ ] T068 Verificar (acta, bloque 5.1) que el fallo del panel (desconectar el LCD en caliente) no detiene ni retrasa el control de válvulas — cubre FR-022, DP-03
- [X] T069 [P] Anotar en cada módulo el comentario de cabecera con los `FR-xxx` que implementa, y en cada prueba el `FR-xxx` que verifica — cubre FR-032, Principio VII
- [X] T070 [P] Escribir `README.md` en la raíz: qué es el sistema, arquitectura en una imagen, cómo compilar, cómo probar y dónde está cada cosa — cubre FR-034
- [ ] T071 Ejecutar la revisión de cumplimiento constitucional: verificar explícitamente Principios I, II y IV sobre el código final, y registrar el resultado — puerta de calidad 3 de la constitución
- [X] T072 Registrar el uso final de RAM y Flash y confirmar RAM < 75 % — cubre NFR-001
- [X] T074 Crear `.github/workflows/ci.yml` con las tres puertas de calidad automatizadas: pruebas en host, compilación con verificación de RAM < 75 %, y comprobación de que la barrera de capas es estructural — cubre NFR-001, FR-029, FR-031, Principios III y IV
- [X] T075 Crear `docs/acta-pruebas-banco.md`, formulario de aceptación para los escenarios 6 a 10, con el bloque 0 bloqueante sobre compatibilidad del payload — cubre SC-001, SC-005, R-012
- [X] T076 Crear `.github/scripts/check_traceability.py` y añadirlo al CI: verifica que **todo** requisito declarado en la especificación esté citado en el código o en la infraestructura, y falla si alguno deja de estarlo — cubre SC-008, Principio VII
- [ ] T073 Publicar la rama en `https://github.com/andresbaena-code/Hardware-Firmware-CableVirtual` y abrir el pull request hacia `main` con el resumen de la migración

---

## Dependencias entre historias

```
Phase 1 (Setup)
      ↓
Phase 2 (Foundational)  ← BLOQUEA TODO
      ↓
      ├──► Phase 3 (US2 · P1) ──┐
      │                          │
      ├──► Phase 4 (US1 · P1) ◄──┘  requiere el main.cpp de US2
      │         ↓
      │    ┌────┴──────────────────┐
      │    ↓                       ↓
      ├──► Phase 5 (US3 · P2)   Phase 6 (US4 · P2)     ← independientes entre sí
      │
      ├──► Phase 7 (US5 · P3)   ← requiere el supervisor de US1
      │
      └──► Phase 8 (US6 · P3)   ← independiente de todo lo demás
                ↓
           Phase 9 (Panel y pulido)
```

| Historia | Depende de | Puede empezar cuando |
|---|---|---|
| US2 (compilar/cargar) | Phase 1, 2 | terminada la fase foundational |
| US1 (control) | Phase 2, US2 (T028) | existe `src/main.cpp` |
| US3 (pruebas en host) | US1 | la lógica de control existe |
| US4 (configuración) | Phase 2 (T011-T013) | el mapa y sus validaciones existen |
| US5 (diagnóstico) | US1 (T038) | el supervisor emite transiciones |
| US6 (legado) | Phase 1 (T004) | los sketches están documentados |
| Phase 9 (panel) | US1 (T038) | el supervisor conoce el estado |

**US6 es independiente de todo**: puede hacerse en cualquier momento, incluso en paralelo
con la implementación del control.

---

## Oportunidades de paralelización

**Dentro de Phase 2** — el bloque de puertos y dobles es masivamente paralelo:

```
T016 T017 T018 T019 T020 T021    (seis interfaces, seis archivos distintos)
T022 T023 T024 T025 T026         (cinco dobles de prueba, archivos distintos)
```

Además, `T010` (tiempos) corre en paralelo con `T008`/`T009` (tipos).

**Dentro de Phase 4** — tres frentes simultáneos tras T033:

```
Frente A: T031 → T032          (antirebote y su prueba)
Frente B: T034 → T035          (política de reintentos y su prueba)
Frente C: T042, T043, T045     (adaptadores sin dependencias entre sí)
```

`T036`/`T037` (planificador) también son independientes de A y B. Todos convergen en
`T038` (supervisor), que es el punto de sincronización de la fase.

**Entre fases** — Phase 5 (US3) y Phase 6 (US4) son independientes entre sí y pueden
correr en paralelo una vez terminada Phase 4. Phase 8 (US6) puede correr en paralelo con
cualquier cosa desde T004.

---

## Estrategia de implementación

### MVP (entrega mínima con valor)

**Phase 1 + Phase 2 + Phase 3 (US2) + Phase 4 (US1)** — tareas T001 a T047.

Con eso el sistema ya sustituye al firmware actual en campo: compila desde VS Code,
controla las 24 válvulas con disparo secuencial y protección de batería, y su lógica está
verificada en host. Es el punto en que la migración deja de ser una promesa.

### Incrementos posteriores

| Incremento | Fases | Valor añadido |
|---|---|---|
| 2 | Phase 9 (panel) | el operador ve el estado en el LCD |
| 3 | Phase 5 + 6 | mantenimiento sostenible y puesta en marcha de fincas nuevas |
| 4 | Phase 7 | diagnóstico en campo sin adivinar |
| 5 | Phase 8 | conocimiento heredado preservado |

> **Nota sobre el orden real de entrega**: el panel (Phase 9) está listado al final por ser
> transversal, pero en la práctica conviene adelantarlo al incremento 2, inmediatamente
> después del MVP. Sin panel, el sistema funciona pero el operador queda ciego, y eso es
> una regresión frente al firmware actual. **No debe sustituirse firmware en una
> instalación productiva hasta completar al menos T065 y T066.**

### Puertas antes de integrar a `main`

Según la constitución (Flujo de Desarrollo, punto 3), antes de integrar:

1. `pio run -e megaatmega2560` compila sin errores ni advertencias nuevas.
2. `pio test -e native` pasa en su totalidad.
3. El uso de SRAM y Flash queda registrado en el commit o el PR.
4. La revisión verifica explícitamente los Principios I, II y IV (tarea T071).

Los escenarios 1 a 5 de [quickstart.md](./quickstart.md) deben estar en verde antes de
integrar; los escenarios 6 a 10, antes de sustituir firmware en producción.

---

## Resumen

| Métrica | Valor |
|---|---|
| Tareas totales | 73 |
| Tareas paralelizables `[P]` | 25 |
| Fases | 9 |
| Historias de usuario cubiertas | 6 de 6 |
| Requisitos trazados | 38 de 38 (35 FR + 3 NFR), verificado por script |
| Alcance del MVP | T001–T047 (47 tareas) |
