---

description: "Lista de tareas — El controlador de riego como esclavo Modbus RTU"
---

# Tasks: El controlador de riego como esclavo Modbus RTU

**Input**: Documentos de diseño en `specs/002-modbus-esclavo-rs485/`

**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md),
[data-model.md](data-model.md), [contracts/](contracts/), [quickstart.md](quickstart.md)

**Tests**: **OBLIGATORIAS**, no opcionales. El Principio IV de la constitución v2.0.0 es
NO NEGOCIABLE y FR-124 exige un caso de prueba por cada defecto catalogado en la
auditoría. Las pruebas se escriben **antes** del código que verifican.

**Organization**: por historia de usuario, para poder implementar y verificar cada una de
forma independiente.

## Format: `[ID] [P?] [Story] Descripción`

- **[P]**: paralelizable (archivo distinto, sin dependencia de tareas incompletas)
- **[Story]**: `[US1]`, `[US2]`, `[US3]`
- Toda tarea lleva ruta de archivo y cita los `FR-1xx` que cubre (Principio VII)

## Puertas que toda tarea debe dejar en verde

```bash
pio run -e megaatmega2560
```

```bash
pio test -e native
```

**Línea base verificada**: 92 pruebas · RAM 20,5 % (1682/8192 B) · Flash 4,6 %
(11806 B).

## Orden de ejecución y por qué

1. La capa pura `lib/wiga_modbus` **con sus pruebas** va primero: es lo único
   verificable sin hardware, y su ausencia fue la causa raíz de los 25 defectos.
2. Después el puerto y el doble de prueba.
3. Después el adaptador, que solo mueve bytes.
4. Después el control, el dominio y la configuración.
5. Después la composición y el panel.
6. **La eliminación del código huérfano va al final, a propósito**: mientras el sustituto
   no esté en verde, borrar deja el repositorio sin compilar.

---

## Phase 1: Setup (infraestructura compartida)

**Purpose**: crear el esqueleto de la biblioteca pura y su suite, sin lógica todavía.

- [X] T001 Crear el directorio `lib/wiga_modbus/` con un `README.md` de una línea que declare la regla que lo justifica: esta biblioteca **nunca** incluye `<Arduino.h>`, para quedar del lado comprobable de la barrera del Principio III
- [X] T002 Crear el ejecutor de la suite nueva en `test/test_modbus/main_test_modbus.cpp`, siguiendo el patrón de `test/test_domain/main_test_domain.cpp`
- [X] T003 [P] Verificar en `platformio.ini` que `lib/wiga_modbus` entra en `env:native` y `env:native_posix` (no debe aparecer en `lib_ignore`, que solo excluye `wiga_hal`)

**Checkpoint**: `pio test -e native` sigue en verde con 73 pruebas y una suite vacía más.

---

## Phase 2: Foundational (prerrequisitos bloqueantes)

**Purpose**: los tipos, el verificador de redundancia, el banco de registros, el puerto y
la configuración. Todo lo que las tres historias necesitan.

**⚠️ CRÍTICO**: ninguna historia puede empezar hasta que esta fase esté completa.

- [X] T004 Definir códigos de función, códigos de excepción y límites del protocolo en `lib/wiga_modbus/modbus_types.h` — cubre FR-101, FR-106
- [X] T005 [P] Escribir las pruebas de CRC16 en `test/test_modbus/test_crc.cpp`, incluido el vector de referencia del estándar `01 04 00 01 00 01` → `60 0A` y los tres vectores de `contracts/register-map.md` §6 — cubre FR-104
- [X] T006 Implementar el CRC16 Modbus (polinomio `0xA001`) en `lib/wiga_modbus/modbus_crc.h/.cpp` hasta que T005 pase — cubre FR-104
- [X] T007 [P] Escribir las pruebas de acceso acotado del banco en `test/test_modbus/test_register_file.cpp`: índice fuera de rango en lectura devuelve `0` y en escritura no hace nada, nunca desborda — cubre FR-107
- [X] T008 Implementar `RegisterFile` en `lib/wiga_modbus/register_file.h/.cpp` con `kRegisterCount` **derivado** del número de válvulas más el bloque de diagnóstico, nunca escrito dos veces — cubre FR-107, FR-109, Principio V
- [X] T009 [P] Declarar el puerto `IByteStream` en `lib/wiga_ports/i_byte_stream.h` con el contrato de `contracts/modbus-slave.md` §1.1: ninguna operación puede bloquear más de lo que cuesta mover un byte a un búfer — cubre FR-108, FR-121
- [X] T010 [P] Implementar el doble `FakeByteStream` en `test/mocks/fake_byte_stream.h`, con capacidad de inyectar fragmentación, ruido y truncamiento — cubre FR-123
- [X] T011 Añadir a `lib/wiga_config/timing_config.h` el identificador de esclavo propio (con `static_assert` de rango 1..247), el silencio entre tramas de 4 ms, las direcciones base de los bloques y el umbral de bus mudo — cubre FR-102, FR-105, FR-117
- [X] T012 Añadir el campo `registerAddress` a `ValveMapping` en `lib/wiga_config/installation_map.h`, rellenar las 24 filas según la tabla de `contracts/register-map.md` §2, y añadir los `static_assert` MAP-V7 (sin direcciones duplicadas), MAP-V8 (dentro del bloque de válvulas) y MAP-V9 (ningún `inputPin` pisa el pin DE/RE) — cubre FR-119, FR-120

**Checkpoint**: la capa pura tiene tipos, CRC y banco de registros, todo verificado en
host. El adaptador todavía no existe.

---

## Phase 3: User Story 1 — El puente lee el estado que pide el tablero (P1) 🎯 MVP

**Goal**: que el controlador responda a los sondeos del UC100 con el estado que los 24
interruptores del tablero están pidiendo.

**Independent Test**: con un maestro Modbus de escritorio se sondea el controlador y se
comprueba que responde tramas válidas que reflejan la posición física de los
interruptores. Sin UC100, sin UC501, sin radio.

### Pruebas de la Historia 1 (se escriben ANTES y deben fallar)

- [X] T013 [P] [US1] Prueba del vector FC03 en `test/test_modbus/test_read_registers.cpp`: `01 03 00 03 00 01 74 0A` → `01 03 02 00 01 79 84` — cubre FR-101, FR-109
- [X] T014 [P] [US1] Prueba del vector FC01 en `test/test_modbus/test_read_bits.cpp`: `01 01 00 00 00 18 3C 00` → `01 01 03 09 00 00 EC 4C` — cubre FR-101, FR-109
- [X] T015 [P] [US1] Prueba de que FC04 devuelve lo mismo que FC03 y FC02 lo mismo que FC01, en `test/test_modbus/test_read_registers.cpp` y `test_read_bits.cpp` — cubre FR-109
- [X] T016 [P] [US1] Prueba de excepción por dirección fuera del mapa en `test/test_modbus/test_exceptions.cpp`: `01 03 00 20 00 01 85 C0` → `01 83 02 C0 F1` — cubre FR-106
- [X] T017 [P] [US1] Prueba de que una petición dirigida a otro identificador produce **silencio absoluto** en `test/test_modbus/test_addressing.cpp` — cubre FR-103
- [X] T018 [P] [US1] Prueba de que un verificador de redundancia incorrecto produce silencio y suma al contador, en `test/test_modbus/test_addressing.cpp` — cubre FR-104, FR-113
- [X] T019 [P] [US1] Prueba de que FC05, FC06, FC15 y FC16 responden excepción `0x01` en `test/test_modbus/test_exceptions.cpp` — cubre FR-106
- [X] T020 [P] [US1] Prueba de reensamblado en `test/test_modbus/test_framing.cpp`: una petición entregada byte a byte produce la misma respuesta que entregada de una vez — cubre FR-105
- [X] T021 [P] [US1] Prueba de que un silencio de 4 ms con trama parcial la descarta sin contaminar la petición siguiente, en `test/test_modbus/test_framing.cpp` — cubre FR-105

### Implementación de la Historia 1

- [X] T022 [US1] Implementar la máquina de recepción de `ModbusSlave` en `lib/wiga_modbus/modbus_slave.h/.cpp` según `contracts/modbus-slave.md` §2: análisis por longitud esperada, con el silencio como resincronizador — cubre FR-101, FR-105
- [X] T023 [US1] Implementar el filtrado por identificador y la validación del verificador en `lib/wiga_modbus/modbus_slave.cpp`, con silencio en ambos casos (reglas MB-01 y MB-02) — cubre FR-103, FR-104
- [X] T024 [US1] Implementar FC03 y FC04 en `lib/wiga_modbus/modbus_slave.cpp` — cubre FR-109
- [X] T025 [US1] Implementar FC01 y FC02 sobre el mismo estado en `lib/wiga_modbus/modbus_slave.cpp` — cubre FR-109
- [X] T026 [US1] Implementar las excepciones `0x01`, `0x02` y `0x03`, y el rechazo de toda escritura, en `lib/wiga_modbus/modbus_slave.cpp` (reglas MB-04 a MB-07) — cubre FR-106
- [X] T027 [US1] Implementar el adaptador `Rs485StreamAdapter` en `lib/wiga_hal/rs485_stream_adapter.h/.cpp`: `Serial1` más control DE/RE, con la **máquina de transmisión no bloqueante** de `contracts/modbus-slave.md` §4. Prohibido `Serial1.flush()`: para la respuesta mayor bloquearía 55,2 ms — cubre FR-108, FR-121
- [X] T028 [US1] Simplificar `ValveRecord` en `lib/wiga_domain/valve_record.h`, retirando `ValvePhase`, `attempts`, `nextAttemptAtMs` y `radioFailures`, que existían para gestionar confirmaciones inexistentes — cubre FR-125
- [X] T029 [US1] Añadir la conversión única `ValveState` → valor de registro en `lib/wiga_domain/valve_types.h`, de modo que ningún adaptador construya el valor a mano — cubre FR-111, FR-112
- [X] T030 [US1] Reescribir `ValveSupervisor` en `lib/wiga_control/valve_supervisor.h/.cpp` para que solo lea entradas, antirrebote, publique en el `RegisterFile` y refresque el panel. Sin política de reintentos ni planificador — cubre FR-109, FR-110, FR-111
- [X] T031 [US1] Componer el esclavo en `src/main.cpp`: instanciar `RegisterFile`, `ModbusSlave` y `Rs485StreamAdapter`, y atender el bus en cada vuelta del lazo sin bloquear — cubre FR-101, FR-121
- [X] T032 [P] [US1] Prueba de integración en `test/test_integration/test_publica_estado.cpp`: un cambio de interruptor se refleja en el registro correspondiente, y solo en ése, tras la ventana de antirrebote — cubre FR-109, FR-122

**Checkpoint**: el controlador ya responde a un maestro de escritorio con el estado real
del tablero. **Es el MVP entregable**: sin las historias 2 y 3, el sistema ya riega.

---

## Phase 4: User Story 2 — La orden sobrevive a una pérdida de radio (P2)

**Goal**: que el nivel publicado sea estable, para que el UC100 vuelva a emitir el
comando de radio y un receptor que perdió un mensaje capture uno posterior.

**Independent Test**: muestrear el registro de una válvula en riego sostenido a lo largo
de miles de sondeos y comprobar que **nunca** se lee un `0`.

### Pruebas de la Historia 2 (se escriben ANTES)

- [X] T033 [P] [US2] **Prueba dominante** en `test/test_integration/test_nivel_estable.cpp`: con una válvula pidiendo riego, miles de sondeos consecutivos devuelven `1` sin una sola excepción — cubre FR-115, FR-116, SC-105
- [X] T034 [P] [US2] Prueba de que el arranque siembra el banco desde los pines **antes** de atender el bus, de modo que el primer sondeo ya devuelva el estado correcto, en `test/test_integration/test_nivel_estable.cpp` — cubre FR-116
- [X] T035 [P] [US2] Prueba de reinicio: tras reiniciar con válvulas regando, el banco se repuebla a `1` desde los interruptores físicos, en `test/test_integration/test_nivel_estable.cpp` — cubre FR-116, FR-119
- [X] T036 [P] [US2] **Prueba de desbordamiento de `millis()` de extremo a extremo** en `test/test_integration/test_wraparound.cpp`, inexistente hoy en ninguna suite: al cruzar los 49,7 días ningún registro parpadea y el antirrebote sigue funcionando — cubre FR-116, SC-109
- [X] T037 [P] [US2] Prueba de detección de bus mudo en `test/test_integration/test_bus_mudo.cpp`: sin peticiones durante el umbral configurado, el controlador lo señala y **sigue funcionando** — cubre FR-114

### Implementación de la Historia 2

- [X] T038 [US2] Garantizar en `lib/wiga_control/valve_supervisor.cpp` que el nivel publicado no presenta ningún valor transitorio: el registro solo se escribe cuando el antirrebote confirma un nivel **distinto** — cubre FR-115, FR-116
- [X] T039 [US2] Implementar el seguimiento de la antigüedad del último sondeo válido en `lib/wiga_modbus/modbus_slave.cpp`, con aritmética modular resistente al desbordamiento — cubre FR-114, FR-113
- [X] T040 [US2] Publicar el bloque de diagnóstico en las direcciones 24 a 31 según `contracts/register-map.md` §3, con contadores que **saturan** en vez de dar la vuelta — cubre FR-113
- [X] T041 [US2] Señalar la ausencia prolongada de sondeo en el panel y en la consola, en `lib/wiga_control/valve_supervisor.cpp` — cubre FR-114

**Checkpoint**: el sistema resiste pérdidas de radio y el fallo más probable en campo
—UC100 apagado o cable cortado— es visible sin conectar un portátil.

---

## Phase 5: User Story 3 — El protocolo se verifica sin hardware (P3)

**Goal**: convertir la lista de defectos de la auditoría en una red de seguridad
permanente, para que ninguno pueda reaparecer en silencio.

**Independent Test**: reintroducir a mano cualquier defecto catalogado debe poner la
suite en rojo.

### Batería de regresión por defecto catalogado (FR-124)

- [X] T042 [P] [US3] Prueba del campo de longitud mentiroso en `test/test_modbus/test_regresion_auditoria.cpp`, con `byteCount` de 250, 251 y 255. El caso `251` es el que hoy hace que `(uint8_t)(3+251+2)` valga `0`, la comprobación pase y se lean 254 bytes de un búfer de 32 — cubre FR-107, SC-107
- [X] T043 [P] [US3] Prueba de que las **24** válvulas son observables, ninguna estructuralmente excluida, en `test/test_modbus/test_regresion_auditoria.cpp`. Regresión del truncamiento que dejaba 12 de 24 inalcanzables — cubre FR-118, SC-102
- [X] T044 [P] [US3] Prueba de polaridad en ambos sentidos en `test/test_integration/test_polaridad.cpp`: interruptor en posición de riego → registro `1`; en reposo → registro `0`. Regresión de la inversión doble del camino de lectura — cubre FR-111, FR-112, FR-116
- [X] T045 [P] [US3] Prueba de que el ruido en la línea antes de una petición legítima no la contamina ni deja al receptor sordo, en `test/test_modbus/test_regresion_auditoria.cpp` — cubre FR-105
- [X] T046 [P] [US3] Prueba de corpus aleatorio en `test/test_modbus/test_fuzz.cpp`: alimentar tramas aleatorias y de casos límite del campo de longitud, y exigir que ninguna provoque acceso fuera de los búferes — cubre FR-107, SC-107

### Infraestructura de verificación

- [X] T047 [US3] Extender el job `barrera-de-capas` de `.github/workflows/ci.yml` para que también verifique que incluir `<Arduino.h>` en `lib/wiga_modbus` rompe la compilación de pruebas — cubre FR-123
- [X] T048 [US3] Extender `.github/scripts/check_traceability.py` para que recorra **todas** las carpetas de `specs/`, no solo `001-migracion-platformio`, de modo que los `FR-101`..`FR-125` queden cubiertos por la puerta de CI — cubre FR-124, Principio VII

**Checkpoint**: los 25 defectos del transporte tienen prueba que los detecta, y la
barrera que los dejó entrar está cerrada estructuralmente.

---

## Phase 6: Polish y asuntos transversales

**Purpose**: defectos confirmados fuera del transporte, retirada del código huérfano y
documentación. **La eliminación va aquí a propósito**: antes de este punto, borrar
dejaría el repositorio sin compilar.

### Defectos confirmados del panel

- [X] T049 [P] Corregir la lectura fuera de la cadena en `setStatusLine()` de `lib/wiga_hal/lcd_display_adapter.cpp:82`: cuando `text[c] == '\0'` coincide con un espacio ya presente, el `continue` salta la rama de terminación y el bucle sigue leyendo `text[16]`, `text[17]`… más allá del literal
- [X] T050 [P] Corregir el contador de recuperación de `lib/wiga_hal/lcd_display_adapter.cpp:103`: `recoveryCounter_` cuenta iteraciones de `loop()`, no vueltas de 15 ms, porque `display.tick()` está fuera del gate en `src/main.cpp:88`. El comentario dice «~66 × 15 ms = ~1 s» y en realidad es quince veces más frecuente
- [X] T051 [P] Corregir la detección de salud del panel en `lib/wiga_hal/lcd_display_adapter.cpp:135`: hoy solo consulta `Wire.getWireTimeoutFlag()`, pero un LCD desconectado en caliente produce NACK y no expiración, de modo que `healthy_` nunca pasa a `false` y `kLcdMaxConsecutiveFailures` es código muerto
- [X] T052 Consumir de verdad `healthy()` en `src/main.cpp`, o corregir el comentario de `src/main.cpp:52` que afirma que la salud del panel se monitorea en el lazo cuando no se invoca en ningún punto — cubre FR-114
- [X] T053 Aplicar la nueva semántica del panel en `lib/wiga_control/valve_supervisor.cpp`: solo símbolos `0` y `1`, retirando `?` y `!`, con etiqueta en la línea de estado que aclare que representa **lo pedido** por el tablero y no un estado confirmado — cubre FR-114b

### Retirada del código huérfano

- [X] T054 Eliminar `lib/wiga_hal/rs485_modbus_adapter.h/.cpp` (motor de transacciones de maestro, 25 defectos) y `lib/wiga_ports/i_modbus_port.h` — cubre FR-125
- [X] T055 Eliminar `lib/wiga_control/retry_policy.h/.cpp` y `lib/wiga_control/latch_scheduler.h/.cpp`, junto con sus pruebas en `test/test_control/test_retry_policy.cpp` y `test/test_control/test_scheduler.cpp` — cubre FR-125
- [X] T056 Eliminar `lib/wiga_domain/app_protocol.h/.cpp` y sus 13 pruebas en `test/test_domain/test_app_protocol.cpp`. **Verificado**: cero referencias fuera de sus propias pruebas, de modo que daban cobertura aparente sobre código que nadie invoca — cubre FR-125
- [X] T057 [P] Eliminar los restos del transporte XBee: `lib/wiga_ports/i_radio_port.h` y `test/mocks/fake_radio.h`, que no los incluye nadie y se pudrirían sin que ningún build lo notase — cubre FR-125
- [X] T058 [P] Eliminar `test/mocks/fake_modbus.h`, doble del contrato de maestro, ya sustituido por `FakeByteStream`. Era infiel al adaptador real en tres aspectos conocidos — cubre FR-125

### Documentación

- [X] T059 [P] Reescribir `docs/ARQUITECTURA_DEL_FIRMWARE.md` con la topología real: ATmega2560 esclavo → UC100 maestro Modbus y controlador D2D → UC501 agente, **sin gateway**. Corregir de paso el periodo del watchdog, que el código fija en `WDTO_8S` y el documento describe como 2 s
- [X] T060 [P] Reescribir `docs/configuracion-finca.md`: el mapa gana la dirección de registro por válvula y pierde la interpretación de `nodeAddressLsb` como slave ID, que era incorrecta
- [X] T061 [P] Reescribir `docs/puesta-en-marcha.md` con el procedimiento de los tres niveles de [quickstart.md](quickstart.md), incluida la configuración del UC100 y el aviso sobre el `lockout time`

### Puertas finales

- [X] T062 Ejecutar `pio test -e native` completo y confirmar que toda la suite queda en verde, sin pruebas eliminadas por conveniencia
- [X] T063 Ejecutar `pio run -e megaatmega2560` y `pio run -e megaatmega2560_silent`, registrar el consumo de RAM y Flash, y confirmar que queda por debajo del 75 % de NFR-001. Referencia de partida: RAM 1682 B, Flash 11806 B
- [X] T064 Recorrer el nivel 2 de [quickstart.md](quickstart.md) con un maestro de escritorio, con atención especial al escenario **B-08**: una hora de sondeo continuo con una válvula regando y **cero** lecturas de `0` — cubre SC-105. **Cerrada el 2026-09-03**: validado en hardware físico (Mega 2560 + MAX485 + conversor USB-RS485) vía Docklight. Las tramas de referencia respondieron exactamente; ver `docs/acta-pruebas-banco.md`.

---

## Dependencias

```
Phase 1 (Setup)
    │
    ▼
Phase 2 (Foundational)  ◀── BLOQUEANTE para todo lo demás
    │
    ├────────────────────┬────────────────────┐
    ▼                    ▼                    ▼
Phase 3 (US1)  ────▶ Phase 4 (US2)      Phase 5 (US3)
   MVP                   │                    │
    │                    │                    │
    └────────────────────┴────────────────────┘
                         │
                         ▼
                  Phase 6 (Polish)
              incluye la ELIMINACIÓN
```

| Relación | Motivo |
|---|---|
| US2 depende de US1 | No se puede comprobar la estabilidad de un nivel que todavía no se publica |
| US3 es **casi** independiente de US2 | Su batería de regresión ataca la capa pura, que existe desde la Fase 2. Puede avanzar en paralelo a US2 |
| Fase 6 depende de todo | Borrar antes de que el sustituto esté en verde deja el repositorio sin compilar |

## Oportunidades de paralelismo

| Bloque | Tareas | Nota |
|---|---|---|
| Pruebas de la Fase 2 | T005, T007, T009, T010 | Archivos distintos, sin dependencia |
| Pruebas de US1 | T013 a T021 | Nueve pruebas, todas en archivos separados |
| Pruebas de US2 | T033 a T037 | |
| Regresión de US3 | T042 a T046 | |
| Defectos del panel | T049, T050, T051 | Zonas distintas del mismo archivo: coordinar el orden de edición |
| Documentación | T059, T060, T061 | Tres documentos independientes |

## Estrategia de entrega

**MVP = Fase 1 + Fase 2 + Fase 3 (US1).** Con eso el controlador ya responde al UC100 con
el estado real del tablero y el riego funciona de extremo a extremo.

**Incremento 2 = US2.** Añade la garantía de nivel estable y la detección de bus mudo. Es
lo que convierte un sistema que funciona en un sistema que se puede dejar solo en una
finca.

**Incremento 3 = US3.** No cambia el comportamiento: convierte la auditoría en una red de
seguridad permanente. Se puede solapar con el incremento 2.

**Fase 6 al final**, siempre.

## Criterio de verificación independiente por historia

| Historia | Cómo se verifica sola |
|---|---|
| **US1** | Un maestro de escritorio sondea y obtiene el estado real de los interruptores. Sin UC100, sin UC501, sin radio |
| **US2** | Miles de sondeos con una válvula regando, sin una sola lectura de `0`, atravesando arranque, reinicio y desbordamiento del contador de tiempo |
| **US3** | Reintroducir a mano cualquier defecto catalogado pone la suite en rojo |

## Trazabilidad de requisitos

| Requisito | Tareas |
|---|---|
| FR-101 rol de esclavo | T004, T013, T014, T022, T031 |
| FR-102 identificador propio | T011 |
| FR-103 silencio ante otro identificador | T017, T023 |
| FR-104 verificador de redundancia | T005, T006, T018, T023 |
| FR-105 delimitación y reensamblado | T020, T021, T022, T045 |
| FR-106 excepciones | T004, T016, T019, T026 |
| FR-107 longitud incoherente | T007, T008, T042, T046 |
| FR-108 dirección del transceptor | T009, T027 |
| FR-109 estado publicado | T008, T013, T014, T015, T030, T032 |
| FR-110 semántica de nivel | T030 |
| FR-111 punto único de conversión | T029, T030, T044 |
| FR-112 inversión por transistor | T029, T044 |
| FR-113 diagnóstico | T018, T039, T040 |
| FR-114 bus mudo | T037, T041, T052 |
| FR-114b panel honesto | T053 |
| FR-115 re-aserción | T033, T038 |
| FR-116 sin `0` transitorio | T033, T034, T035, T036, T038, T044 |
| FR-117 configuración temporal | T011 |
| FR-118 las 24 válvulas | T043 |
| FR-119 mapa validado | T012, T035 |
| FR-120 campos históricos | T012 |
| FR-121 no bloqueante | T009, T027, T031 |
| FR-122 muestreo del tablero | T032 |
| FR-123 código puro comprobable | T010, T047 |
| FR-124 un caso por defecto | T042 a T046, T048 |
| FR-125 retirada del huérfano | T028, T054 a T058 |

---

## Notas de ejecucion (2026-08-27)

Tres desviaciones respecto al plan original, cada una con su motivo:

- **T012 — direccion de registro derivada, no almacenada.** El plan preveia una columna
  `registerAddress` en `ValveMapping`. Pero `contracts/register-map.md` seccion 2 establece
  que la direccion **coincide** con el indice de la valvula, de modo que guardarla
  duplicaria el indice y permitiria que ambos se desincronizasen. Se implemento como
  `config::valveRegister(index)`, derivada. Se anadio en su lugar el `static_assert`
  **MAP-V9**, que si aporta una comprobacion nueva: ningun pin de entrada puede pisar el
  pin DE/RE del transceptor.

- **T030 — publicador nuevo junto al supervisor viejo, no reescritura in situ.**
  Reescribir `ValveSupervisor` habria roto `retry_policy`, `latch_scheduler` y sus pruebas
  de integracion, que la Fase 6 elimina pero que ahora deben seguir compilando. Se creo
  `ValvePublisher` al lado, siguiendo el patron de estrangulamiento, para que el build
  quede verde en cada paso. Es justo la razon por la que la eliminacion va al final.

- **T028 resuelta en la Fase 6.** `ValveRecord` no se simplifico: se ELIMINO entero junto
  con las fases de confirmacion. El estado pedido de una valvula es exactamente el valor de
  su registro, y mantener una copia paralela habria duplicado la fuente de verdad.

- **T012 amplio su alcance.** Al desacoplar el mapa del protocolo XBee se descubrio que
  `installation_map.h` incluia `app_protocol.h` solo para leer `proto::kMaxValveId`. Se
  sustituyo por una constante local, lo que permitio retirar el protocolo XBee entero.

- **T037 movio una decision fuera de la raiz de composicion.** La deteccion de maestro
  ausente vivia en `main.cpp`, que el entorno de pruebas no compila. Se traslado a
  `ModbusSlave::masterSilent()`, en la capa pura, y ahora tiene cuatro pruebas incluida la
  del desbordamiento del reloj.

- **T048 amplio su alcance.** Al eliminar los modulos de confirmacion, 13 requisitos de la
  especificacion 001 se quedaron sin codigo que los citara y la puerta de CI empezo a
  fallar. En vez de falsear citas, se anadio a la especificacion 002 una seccion de
  **requisitos derogados** y se enseno al verificador a respetarla. Trazabilidad final:
  51/51.

- **T064 cerrada en hardware (2026-09-03).** El nivel 2 de `quickstart.md` se recorrió con
  hardware físico: Arduino Mega 2560, transceptor MAX485 y adaptador USB-RS485, con
  Docklight como maestro de escritorio. Se probaron las tramas de referencia: FC03 sobre el
  registro de diagnóstico devolvió la versión de firmware (`01 03 02 02 00 B9 24`), FC01
  sobre las 24 bobinas devolvió el mapa real con solo la entrada 0 energizada, y una
  petición dirigida al esclavo `2` produjo el silencio absoluto esperado. El escenario
  **B-08** (una hora de sondeo continuo con la válvula regando) se completó **sin una sola
  lectura de `0`**. Resultados registrados en `docs/acta-pruebas-banco.md`.

Un defecto encontrado al compilar para la placa, que las pruebas en host no podian
detectar: `RegisterFile::bit()` colisiona con la macro `#define bit(b)` de `Arduino.h`.
Renombrado a `coilBit()`. Solo aparece en el entorno de la placa, porque la capa pura no
incluye `Arduino.h` — que es precisamente la barrera funcionando como debe.

Cuatro verificadores CRC de las pruebas se escribieron mal a mano y se corrigieron tras
calcularlos: FC03 de 24 registros, FC02 de 24 bits, rango 30+5 y funcion desconocida.
