# Implementation Plan: El controlador de riego como esclavo Modbus RTU

**Branch**: `002-modbus-esclavo-rs485` | **Date**: 2026-08-27 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/002-modbus-esclavo-rs485/spec.md`

## Summary

Sustituir el transporte del controlador de riego, hoy implementado como maestro Modbus
que sondea nodos remotos, por un **esclavo Modbus RTU estándar** que publica en registros
lo que el tablero pide y responde a las peticiones del UC100.

El cambio de rol simplifica el sistema en vez de complicarlo: al no existir confirmación
del campo, desaparecen la política de reintentos, el planificador de disparo y tres de
las cuatro fases de la máquina de estados. El firmware se reduce a **leer 24
interruptores, antirrebotarlos, publicarlos en 24 registros y contestar**.

El eje técnico del plan es una **separación estructural** que hoy no existe: toda la
lógica de tramado, CRC y análisis de peticiones pasa a una biblioteca pura sin
`<Arduino.h>`, ejercitable en `env:native`. Que esa lógica viviera en `lib/wiga_hal`,
que el entorno de pruebas excluye con `lib_ignore`, es la causa raíz documentada de que
25 defectos entraran con la suite entera en verde.

## Technical Context

**Language/Version**: C++14 (`build_flags = -std=gnu++14`), avr-gcc 7.3.0 del toolchain
de PlatformIO. C++14 es necesario para `constexpr` con bucles en la validación del mapa.

**Primary Dependencies**: framework-arduino-avr 5.4.0; `LiquidCrystal_I2C@1.1.4` (única
dependencia externa, ya fijada). **Ninguna biblioteca Modbus de terceros**: el protocolo
se implementa en el repositorio para que sea código puro y comprobable, que es
precisamente lo que exige el Principio IV v2.0.0.

**Storage**: N/A. Sin persistencia; el estado se reconstruye de las entradas físicas.

**Testing**: Unity sobre PlatformIO. `env:native` (Windows, `platform=windows_x86`) y
`env:native_posix` (Linux/macOS y CI). Línea base: 73 pruebas en verde en 19 s.

**Target Platform**: Arduino Mega 2560 (ATmega2560, 16 MHz, 8 KB SRAM, 248 KB Flash).

**Project Type**: Firmware embebido de un solo binario, con biblioteca de lógica pura
verificable en el equipo de desarrollo.

**Performance Goals**:
- Cadencia nominal del lazo de control: 15 ms.
- Ocupación del lazo atribuible al transporte: **< 2 ms** por vuelta (Principio I).
- Respuesta a una petición del maestro: dentro del *Max Resp Time* del UC100, que es
  configurable de 10 a 60000 ms. Con márgenes de sobra.

**Constraints**:
- Prohibido `delay()`, esperas activas y cualquier bloqueo mayor a 2 ms en el lazo.
- Prohibida la asignación dinámica de memoria.
- SRAM total por debajo del 75 % (NFR-001). Línea base actual: 20,5 % (1682 B).
- `lib/wiga_control` y `lib/wiga_domain` no pueden incluir `<Arduino.h>`.
- **FR-116**: el registro de una válvula que riega no puede presentar jamás un `0`
  transitorio. Un solo sondeo que lea `0` cierra la válvula en campo.

**Scale/Scope**: 24 válvulas, 24 registros de válvula más un bloque de diagnóstico. Un
solo maestro en el bus. 9600 baudios 8N1.

## Constitution Check

*GATE: evaluado contra la constitución **v2.0.0** (enmendada el 2026-08-27).*

| Principio | Puerta | Cómo la pasa este diseño |
|---|---|---|
| **I. Determinismo del lazo** | Sin `delay()`, sin espera activa, sin bloqueo > 2 ms | El esclavo no espera nunca a nadie: consume los bytes disponibles y vuelve. La transmisión de la respuesta se hace con una **máquina de estados de TX no bloqueante** (ver Riesgo 1), no con `Serial1.flush()`, que para una respuesta de 53 B bloquearía 55 ms |
| **II. Seguridad de válvula (NO NEGOCIABLE)** | Sin actuación simultánea masiva; re-aserción acotada; seguridad sin acuse; honestidad de la interfaz; sin ráfagas al reiniciar | El controlador solo publica nivel estable; nunca lo devuelve a `0` por iniciativa propia (FR-110, FR-116). El panel muestra intención con etiqueta explícita (FR-114b). El arranque no emite órdenes: publicar no es emitir |
| **III. Arquitectura en capas** | El dominio y el control jamás incluyen `<Arduino.h>` | La biblioteca nueva `lib/wiga_modbus` es pura y queda **dentro** de la barrera, no fuera. El adaptador de `wiga_hal` solo mueve bytes |
| **IV. Pruebas en host (NO NEGOCIABLE)** | Toda regla de negocio con prueba en `native`; **la lógica de protocolo debe ser código puro** | Es el eje del plan. `lib/wiga_modbus` se compila y se prueba en host. El adaptador que queda en `wiga_hal` no contiene ninguna decisión |
| **V. Configuración centralizada** | Cero números mágicos; el número de válvulas se deriva del mapa | Slave ID, direcciones base de registro, silencio entre tramas y umbrales van a `timing_config.h` y al mapa, con `static_assert` |
| **VI. Observabilidad** | Versión, causa de reinicio, transiciones, contadores de error | Se conserva `SerialLog`. Los contadores del bus pasan a ser **legibles por el maestro** además de por consola (FR-113), corrigiendo que hoy sean de solo escritura |
| **VII. Trazabilidad** | `FR-xxx` en el módulo y en la prueba | Cada módulo nuevo cita sus `FR-1xx`; se extiende el verificador de CI para cubrirlos |

**Resultado de la puerta: PASA.** Sin violaciones que justificar. La sección *Complexity
Tracking* queda vacía a propósito.

Conviene subrayar por qué pasa ahora y no antes: la implementación vigente **viola el
Principio IV** (la lógica de protocolo vive en una capa excluida de las pruebas) y el
**Principio I** (bloquea el lazo entre 480 y 805 ms por vuelta). Este plan existe en
buena medida para cerrar esas dos violaciones.

## Project Structure

### Documentation (this feature)

```text
specs/002-modbus-esclavo-rs485/
├── spec.md              # Especificación (26 FR, 9 SC)
├── plan.md              # Este archivo
├── research.md          # Fase 0
├── data-model.md        # Fase 1
├── quickstart.md        # Fase 1
├── contracts/
│   ├── modbus-slave.md  # Contrato del protocolo en el bus
│   └── register-map.md  # Mapa de registros que consume el UC100
├── checklists/
│   └── requirements.md
└── tasks.md             # Fase 2 (/speckit-tasks, no lo crea este comando)
```

### Source Code (repository root)

```text
CABLE_VIRTUAL_V2_PIO/
├── lib/
│   ├── wiga_modbus/                 # NUEVA — capa pura, sin <Arduino.h>
│   │   ├── modbus_types.h           # códigos de función, excepciones, límites
│   │   ├── modbus_crc.h/.cpp        # CRC16 (polinomio 0xA001)
│   │   ├── register_file.h/.cpp     # los registros publicados y su acceso acotado
│   │   └── modbus_slave.h/.cpp      # ensamblado de trama, análisis y respuesta
│   │
│   ├── wiga_ports/
│   │   ├── i_byte_stream.h          # NUEVA — bytes + control de dirección del bus
│   │   ├── i_clock.h                # sin cambios
│   │   ├── i_digital_input_port.h   # sin cambios
│   │   ├── i_display_port.h         # sin cambios
│   │   ├── i_log_port.h             # sin cambios
│   │   ├── i_watchdog_port.h        # sin cambios
│   │   ├── i_modbus_port.h          # SE ELIMINA (contrato de maestro)
│   │   └── i_radio_port.h           # SE ELIMINA (resto de XBee)
│   │
│   ├── wiga_hal/
│   │   ├── rs485_stream_adapter.h/.cpp   # NUEVO — Serial1 + DE/RE, TX no bloqueante
│   │   ├── rs485_modbus_adapter.h/.cpp   # SE ELIMINA (motor de maestro, 25 defectos)
│   │   ├── lcd_display_adapter.h/.cpp    # se corrige (3 defectos)
│   │   └── (arduino_clock, arduino_digital_input, serial_log,
│   │        avr_watchdog, reset_cause, firmware_version)  # sin cambios
│   │
│   ├── wiga_control/
│   │   ├── valve_supervisor.h/.cpp  # se simplifica: leer, publicar, refrescar
│   │   ├── debouncer.h/.cpp         # sin cambios
│   │   ├── retry_policy.h/.cpp      # SE ELIMINA (sin fuente de datos)
│   │   └── latch_scheduler.h/.cpp   # SE ELIMINA (el controlador ya no dispara)
│   │
│   ├── wiga_domain/
│   │   ├── valve_types.h            # sin cambios (la inversión se conserva)
│   │   ├── valve_record.h           # se simplifica: sin fases de confirmación
│   │   ├── time_math.h              # sin cambios
│   │   └── app_protocol.h/.cpp      # SE ELIMINA (protocolo XBee, cero llamadores)
│   │
│   └── wiga_config/
│       ├── installation_map.h/.cpp  # gana dirección de registro por válvula
│       └── timing_config.h          # gana slave ID, silencio entre tramas, umbrales
│
├── src/main.cpp                     # compone el esclavo; corrige el gate del panel
└── test/
    ├── mocks/
    │   ├── fake_byte_stream.h       # NUEVO — bus en memoria, con inyección de fallos
    │   ├── fake_modbus.h            # SE ELIMINA (doble del contrato de maestro)
    │   ├── fake_radio.h             # SE ELIMINA (resto de XBee)
    │   └── (fake_clock, fake_display, fake_inputs, fake_log, fake_watchdog)
    ├── test_modbus/                 # NUEVA suite — CRC, tramado, excepciones, límites
    ├── test_control/                # se poda de reintentos y planificador
    ├── test_domain/                 # se poda de app_protocol (13 pruebas)
    └── test_integration/            # se reorienta a publicar y responder
```

**Structure Decision**: se conserva la arquitectura en capas de la especificación 001 y
se le añade **una biblioteca nueva, `lib/wiga_modbus`**, situada del lado *puro* de la
barrera del Principio III. Es la decisión estructural central del plan.

La regla que la justifica es simple de verificar: `platformio.ini` excluye `wiga_hal` de
las pruebas con `lib_ignore`. Todo lo que necesite prueba tiene que vivir fuera de
`wiga_hal`. Hoy el CRC16, el tramado y el análisis de peticiones viven dentro, y por eso
ninguna de las 73 pruebas los toca. Al moverlos a `wiga_modbus` quedan cubiertos por
construcción, y lo que queda en `wiga_hal` es un adaptador sin decisiones: lee bytes de
`Serial1`, se los da a la capa pura, y escribe los que ésta devuelva.

## Riesgos de diseño y cómo los cierra el plan

### Riesgo 1 — La respuesta bloquearía el lazo 55 ms

Una respuesta FC03 de 24 registros son 53 bytes. A 9600 baudios 8N1 son
`53 × 10 / 9600 = 55,2 ms` de transmisión. Un `Serial1.flush()` bloquearía todo ese
tiempo, **27 veces por encima del límite de 2 ms del Principio I**, y es exactamente el
tipo de espera activa que la implementación actual comete.

**Cierre**: máquina de estados de TX en el adaptador. Se afirma DE, se entregan los bytes
al búfer de `Serial1` (64 B en el Mega, suficiente), se calcula el instante de fin a
partir del número de bytes y del baudaje, y se libera DE en una vuelta posterior del lazo
cuando ese instante ha pasado **y** el búfer se ha vaciado. Ninguna vuelta bloquea.

### Riesgo 2 — Un `0` transitorio corta el riego (FR-116)

Es el requisito dominante. Tres momentos concentran el peligro:

| Momento | Análisis | Cierre |
|---|---|---|
| **Arranque** | Si el esclavo contestara antes de sembrar los registros desde los pines, devolvería `0` para una válvula que sí riega | Seguro por construcción: `setup()` siembra antes de que `loop()` atienda el bus. Se blinda con prueba explícita |
| **Reinicio por watchdog** | Igual que el arranque, más la ventana en que el Mega no responde | Un **no-responder no es un `0`**: el maestro ve expiración, no un valor, y no dispara cierre. Es la distinción clave y se documenta |
| **Desbordamiento de `millis()`** | Un temporizador que se desmadre podría alterar un registro | El valor deriva de `Debouncer::stable()`, que solo cambia cuando un nivel **distinto** se sostiene 50 ms. El desbordamiento no puede fabricar un cambio de nivel. Se añade prueba de wraparound de extremo a extremo, que hoy no existe |

Un cuarto punto, menos evidente: no hay lectura rota entre capas. Ninguna interrupción
escribe los registros; el control y el protocolo se ejecutan cooperativamente en el mismo
hilo, así que un registro no puede cambiar a mitad de una respuesta.

### Riesgo 3 — Reintroducir alguno de los 25 defectos

**Cierre**: FR-124 exige un caso de prueba por defecto catalogado. La suite nueva
`test/test_modbus/` se escribe con esos casos como criterio de aceptación, no como
añadido posterior. En particular la lectura fuera de buffer con campo de longitud
mentiroso, que hoy es explotable con `byteCount = 251`.

### Riesgo 4 — El mapa de registros no coincide con la configuración del UC100

Es la única dependencia externa que no se puede probar aquí.

**Cierre**: el contrato `contracts/register-map.md` es la fuente de verdad única, y
`quickstart.md` incluye el procedimiento de verificación con un maestro de escritorio
antes de conectar el UC100. Además se soportan **cuatro funciones de lectura** (FC01,
FC02, FC03, FC04) sobre el mismo estado, de modo que cualquier tipo de canal que el
instalador elija en el UC100 funcione sin recompilar el firmware.

## Re-evaluación de la puerta constitucional tras la Fase 1

*Exigida por el flujo: la puerta se vuelve a evaluar contra el diseño concreto, no solo
contra la intención.*

| Principio | Qué se comprobó sobre el diseño ya escrito | Veredicto |
|---|---|---|
| **I** | El peor caso de ocupación del lazo queda acotado a mover bytes entre búferes. Los 55,2 ms de la respuesta mayor transcurren **en el cable**, no en el lazo: `contracts/modbus-slave.md` §4 fija que la espera de DE ocurre entre vueltas, y DE-03 lo hace regla | **PASA** |
| **II** | `data-model.md` §4 recorre los cinco momentos en que podría aparecer un `0` transitorio y cierra cada uno con su prueba. La distinción entre *no responder* y *responder `0`* queda escrita como compromiso C-02 del contrato | **PASA** |
| **III** | `lib/wiga_modbus` no incluye `<Arduino.h>`. La única dependencia hacia fuera es `IByteStream`, declarado en la capa de puertos. El adaptador de `wiga_hal` no toma ninguna decisión de protocolo | **PASA** |
| **IV** | `ModbusSlave` no tiene entrada/salida propia: bytes entran, bytes salen. Es comprobable en `env:native` por construcción, no por convención. Los vectores de prueba del contrato están calculados y contrastados contra el vector de referencia del estándar | **PASA** |
| **V** | Slave ID, silencio entre tramas, direcciones base y umbrales quedan en `timing_config.h` y en el mapa. `kRegisterCount` se **deriva**; no se escribe dos veces | **PASA** |
| **VI** | Los contadores del bus pasan a ser legibles por el maestro además de por consola, corrigiendo que hoy `framingErrors()` se incremente en seis puntos sin que nadie lo lea | **PASA** |
| **VII** | Cada módulo nuevo cita sus `FR-1xx`; el verificador de CI se extiende a todas las carpetas de `specs/` | **PASA** |

**Sin regresiones ni desviaciones nuevas.** El diseño además **cierra** las dos
violaciones vigentes en `main`: la del Principio IV (lógica de protocolo en una capa sin
pruebas) y la del Principio I (de 480–805 ms de bloqueo por vuelta a menos de 2 ms).

## Complexity Tracking

> Sin violaciones de la constitución que justificar. Sección intencionadamente vacía.

El plan **reduce** complejidad en vez de añadirla: elimina dos módulos de control
completos (`retry_policy`, `latch_scheduler`), un protocolo de aplicación entero
(`app_protocol`), dos puertos y dos dobles de prueba, y sustituye un motor de
transacciones de maestro por un esclavo que solo contesta.
