# Fase 0 — Investigación: Migración del firmware Cable Virtual a PlatformIO

**Feature**: `001-migracion-platformio` | **Fecha**: 2026-08-20
**Entrada**: [spec.md](./spec.md) · [constitución v1.0.0](../../.specify/memory/constitution.md)

Todas las incógnitas del Contexto Técnico quedan resueltas en este documento. No hay
marcadores `NEEDS CLARIFICATION` pendientes.

---

## R-001 — Modo API del XBee: escapado (AP=2), confirmado por lectura de la librería

**Decisión**: los radios de campo operan en **modo API escapado (AP=2)**. El firmware
migrado DEBE mantener exactamente ese modo de trama.

**Evidencia**: se descargó la librería usada por el sketch base
(`andrewrapp/XBee`, sha `39f236ea18`) y se inspeccionó su código:

- `XBee::send()` (XBee.cpp:1496) llama a `sendByte(b, true)` para todos los bytes salvo el
  delimitador de inicio, es decir, **siempre escapa** `0x7E`, `0x7D`, `0x11` y `0x13`.
- `XBee::readPacket()` (XBee.cpp:880) desescapa siempre en recepción, y trata
  `START_BYTE` intermedio como error solo cuando `ATAP == 2`.
- `XBee.h:35` fija `#define ATAP 2` de forma incondicional.

**Por qué importa**: si el firmware migrado transmitiera en modo API sin escape (AP=1),
los nodos instalados descartarían toda trama que contuviera un byte reservado y el sistema
fallaría de forma intermitente e imposible de diagnosticar. Este dato no está escrito en
ninguna parte del sketch original; se perdía con él.

**Alternativas consideradas**:
- *Reimplementar el códec de tramas API en el dominio.* Rechazado: es la capa que ya está
  probada byte a byte contra más de veinte nodos en producción, y la especificación exige
  compatibilidad total (FR-015). Se gana testabilidad donde importa —el payload de
  aplicación— sin reescribir el transporte probado.
- *Migrar los nodos a AP=1.* Fuera de alcance: implica intervenir hardware en campo.

---

## R-002 — `readPacket()` devuelve una sola trama por llamada

**Decisión**: el adaptador de radio DEBE invocar `readPacket()` **repetidamente hasta que
no haya respuesta disponible**, no una sola vez por ciclo.

**Evidencia**: `XBee::readPacket()` hace `return` en cuanto completa una trama
(XBee.cpp:970-973), dejando el resto de bytes en el búfer del `HardwareSerial`. El sketch
base la llamaba una vez por vuelta de lazo.

**Consecuencia sobre el firmware base**: con dos confirmaciones consecutivas en el búfer, la
segunda se procesaba una vuelta más tarde o se perdía si el búfer de 64 bytes desbordaba.
Sumado al vaciado explícito del búfer (R-003), explica reintentos observados sobre válvulas
que sí habían respondido.

**Diseño resultante**: `XBeeRadioAdapter::poll()` drena en bucle acotado (máximo N tramas
por ciclo, para no violar el Principio I) y entrega cada trama al dominio.

---

## R-003 — El vaciado del búfer previo a transmitir se elimina (FR-018)

**Decisión**: se elimina `while(Serial1.available()) Serial1.read();` presente en
`transmitirOrden()` del sketch base.

**Racional**: descarta confirmaciones ya recibidas. El efecto en campo es un reintento
—y por tanto un disparo de solenoide— sobre una válvula que ya había obedecido. Contradice
directamente el Principio II (protección de batería y solenoides) y el FR-018.

**Riesgo del cambio y mitigación**: el vaciado probablemente se agregó para evitar
desincronización del parser tras una trama corrupta. La mitigación correcta no es tirar
datos válidos, sino que el parser se recupere solo: la librería ya marca
`UNEXPECTED_START_BYTE` y reinicia el estado al detectar un delimitador fuera de lugar. El
adaptador contabiliza esos errores para diagnóstico (FR-028) en lugar de ocultarlos.

**Alternativas consideradas**: conservar el vaciado tras las respuestas del ciclo (vaciar
solo lo no consumido). Rechazado por innecesario una vez que el drenaje de R-002 es completo.

---

## R-004 — Lógica inversa por transistor: una sola conversión con nombre (FR-021, FR-021b)

**Decisión**: se introducen dos tipos distintos en el dominio y una única función de
conversión entre ellos.

| Concepto | Tipo | Significado |
|---|---|---|
| Nivel eléctrico | `SignalLevel { Low, High }` | lo que hay en el pin o en el byte del protocolo |
| Estado físico | `ValveState { Closed, Open }` | lo que hace la válvula en campo |

Regla del hardware, confirmada por el responsable técnico: la etapa de potencia del nodo
opera con lógica inversa por transistor. **Nivel `High` (1) ⇒ válvula cerrada; nivel `Low`
(0) ⇒ válvula abierta.** La misma inversión aplica a la lectura de las entradas del
tablero, que usan pull-up interno.

**Racional**: en el sketch base la inversión aparecía como un ternario suelto dentro de la
rutina de pantalla (`(estado == 1) ? 0 : 1`) y como una condición suelta en la lectura
(`(digitalRead(PINES[i]) == LOW) ? 1 : 0`). Cualquier mantenedor razonable lo lee como un
defecto y lo "corrige", rompiendo la instalación. Al convertirlo en dos tipos que el
compilador no deja mezclar, el error deja de ser posible.

**Alternativas consideradas**: documentar la inversión en un comentario. Rechazado: un
comentario no impide el error; un tipo sí.

---

## R-005 — Temporización no bloqueante y desbordamiento de `millis()` (FR-011, FR-013)

**Decisión**: toda espera se expresa como `(uint32_t)(now - since) >= interval`, con
`now` obtenido de un puerto `IClock` y comparación en aritmética modular sin signo.

**Racional**: `millis()` desborda a los ~49,7 días. La forma ingenua
(`now >= deadline`) falla exactamente una vez por desbordamiento, dejando un temporizador
colgado hasta 49 días. Con NFR-002 exigiendo 90 días continuos, el desbordamiento ocurre
sí o sí en el periodo de aceptación.

El sketch base evitaba el problema por accidente: contaba vueltas de lazo en enteros
(`T[i]--`) en vez de usar tiempo real, lo que ataba la temporización a la duración exacta
del `delay(15)` y hacía que cualquier variación del lazo desviara los 30 s y los 60 min.

**Conversión de constantes heredadas** (lazo base de 15 ms):

| Constante heredada | Valor en vueltas | Tiempo real | Constante migrada |
|---|---|---|---|
| `VENTANA_LATCH` | 100 | 1 500 ms | `kLatchGuardMs = 1500` |
| `REINTENTO_30S` | 2 000 | 30 000 ms | `kRetryIntervalMs = 30000` |
| `REINTENTO_1HORA` | 240 000 | 3 600 000 ms | `kBackoffIntervalMs = 3600000UL` |
| `MAX_INTENTOS` | 5 | — | `kMaxAttempts = 5` |
| `INTERVALO_MS` | 15 | 15 ms | cadencia de muestreo de entradas |

**Nota**: `REINTENTO_1HORA` se declaraba como `long` (32 bits con signo en AVR) con valor
240 000; el producto en milisegundos (3 600 000) sigue cabiendo en `uint32_t`. Las
constantes migradas se declaran `uint32_t` explícitamente.

---

## R-006 — Antirebote de entradas (FR-002)

**Decisión**: filtro de estabilidad por entrada con ventana configurable, valor por defecto
**50 ms**. Una entrada debe leerse igual durante toda la ventana antes de generar una
solicitud de cambio.

**Racional**: el sketch base no tenía antirebote; cualquier microcorte del contacto del
tablero disparaba una orden latch, con su pico de corriente. La versión antigua
`WIGA_24IN_LCDB1_5` sí tenía un contador antirebote (`maxM = 5` lecturas), lo que confirma
que el problema es real en esta instalación y que se perdió al reescribir.

**Valor elegido**: 50 ms cubre el rebote típico de un relé o contacto seco (5-20 ms) con
margen, y es imperceptible frente a los 1 500 ms de ventana de disparo. Ajustable en banco
sin tocar lógica (FR-023).

---

## R-007 — Panel LCD sin bloquear el lazo (FR-022, Principio I)

**Decisión**: el adaptador de pantalla mantiene un **buffer de sombra** de 4×20 caracteres
con marcas de suciedad, y escribe **como máximo una celda por vuelta de lazo**. El bus I2C
se configura a **400 kHz**.

**Racional**: `LiquidCrystal_I2C` es síncrona. Cada carácter cuesta dos transacciones I2C
de 4 bytes con pulsos de habilitación; a 100 kHz son ~1,5 ms por carácter y un refresco
completo de 80 celdas supera los 100 ms — un bloqueo inaceptable bajo el Principio I. A
400 kHz una celda cuesta ~0,4 ms, dentro del presupuesto de 2 ms por vuelta.

**Tolerancia a fallo (FR-022)**: si el LCD no responde, `Wire` agota su tiempo interno y la
escritura falla en silencio; el control no depende del resultado. El adaptador cuenta
fallos consecutivos y, superado un umbral, deja de intentar hasta el próximo reinicio,
para que un bus caído no consuma presupuesto de ciclo indefinidamente.

**Librería elegida**: `marcoschwartz/LiquidCrystal_I2C@1.1.4`, verificada en el registro.
Expone `init()`, `backlight()`, `setCursor()`, `print()`. **Atención a la migración**: el
sketch base llamaba `lcd.begin()` sin argumentos, firma que **no existe** en esta librería
(el sketch usaba el fork de Frank de Brabander instalado a mano en el IDE de Arduino). El
adaptador migrado usa `init()`, que es la inicialización equivalente.

---

## R-008 — Causa del último reinicio (FR-026)

**Decisión**: leer `MCUSR` en el arranque temprano, antes de que el bootloader o el
arranque de Arduino lo limpien, guardarlo en una variable en la sección `.noinit`, y
limpiar `MCUSR` junto con deshabilitar el watchdog.

**Racional**: sin esto, un reinicio por watchdog en campo es indistinguible de un corte de
alimentación, y el técnico no sabe si persigue un problema de firmware o de batería.

**Implementación**: función marcada con
`__attribute__((naked, used, section(".init3")))`, que se ejecuta antes de `main()`. Es la
técnica estándar en AVR y no depende del bootloader instalado.

**Riesgo conocido**: el bootloader `optiboot` del Mega puede limpiar `MCUSR` antes de que
corra `.init3`. Si en banco el flag de watchdog no aparece, se documenta la limitación en
la guía de diagnóstico y se conserva el resto de la información de arranque; el firmware no
depende de este dato para operar.

---

## R-009 — Estructura del proyecto y pruebas en host (FR-031, Principio IV)

**Decisión**: estructura de librerías internas de PlatformIO, con la lógica pura en
`lib/` y solo el arranque en `src/`.

```
platformio.ini          # entornos megaatmega2560 y native
src/main.cpp            # composición: crea adaptadores, inyecta puertos, corre el lazo
lib/wiga_domain/        # tipos, protocolo de aplicación, política de reintentos  (sin Arduino)
lib/wiga_control/       # supervisor de válvulas, planificador de disparo         (sin Arduino)
lib/wiga_ports/         # interfaces abstractas (solo cabeceras)                  (sin Arduino)
lib/wiga_config/        # mapa de instalación + validación en compilación         (sin Arduino)
lib/wiga_hal/           # adaptadores Arduino: radio, pantalla, entradas, reloj, watchdog, log
test/test_domain/       # Unity, corre en native y en la placa
test/test_control/
```

**Racional**: PlatformIO compila cada carpeta de `lib/` como librería independiente y solo
enlaza lo que se incluye. El entorno `native` compila `wiga_domain`, `wiga_control`,
`wiga_ports` y `wiga_config` con el compilador del equipo, sin tocar `wiga_hal`. Esto hace
que la barrera del Principio III sea **estructural**, no una convención: si alguien incluye
`<Arduino.h>` en el dominio, la compilación `native` falla de inmediato.

**Framework de pruebas**: Unity, incluido en PlatformIO, sin dependencia externa.

**Alternativas consideradas**:
- *Todo en `src/` con carpetas.* Rechazado: `native` compilaría también los adaptadores y
  la separación dependería de la disciplina del desarrollador.
- *Google Test.* Rechazado: pesa más y no aporta sobre Unity para lógica de este tamaño.

---

## R-010 — Validación de la configuración en tiempo de compilación (FR-024, FR-025)

**Decisión**: el mapa de instalación se declara como un único arreglo `constexpr` de
estructuras, y se valida con `static_assert` sobre funciones `constexpr` que detectan pines
repetidos y pares (dirección, identificador) repetidos. El número de válvulas se deriva con
`sizeof(kInstallation)/sizeof(kInstallation[0])`.

**Racional**: el sketch base mantenía tres arreglos paralelos de 24 elementos editados a
mano. Un desajuste entre ellos no produce error de compilación: produce una válvula que
obedece la orden de otra, en campo, de noche. Es el modo de fallo más caro del sistema.
`static_assert` lo convierte en un error de compilación en el escritorio.

**Verificación del estándar**: PlatformIO compila el framework Arduino AVR con
`-std=gnu++11` por defecto. `constexpr` y `static_assert` están disponibles; los bucles
dentro de funciones `constexpr` requieren C++14, por lo que el proyecto fija
`build_unflags = -std=gnu++11` y `build_flags = -std=gnu++14`. Alternativa de respaldo si
el toolchain diera problema: expresar las comprobaciones de forma recursiva, válida en
C++11.

**Dato heredado a revisar en puesta en marcha**: el mapa del sketch base repite la
dirección `0x41C3f595` en cuatro posiciones consecutivas (índices 18-22) con
identificadores 2, 3, 4 y 5, y `0x42878FDB` en los índices 0-3. Son combinaciones
(dirección, id) únicas, así que pasan la validación — pero conviene confirmarlas contra el
plano de la finca antes de dar por buena la migración.

---

## R-011 — Presupuesto de memoria (NFR-001)

**Decisión**: el estado por válvula se dimensiona explícitamente y se verifica contra el
presupuesto de 8 KB de SRAM del ATmega2560.

Estimación del estado de control: por válvula, estado solicitado y confirmado (2 B), marca
de tiempo del próximo intento (4 B), contador de intentos (1 B), banderas (1 B), contador
de fallos (2 B) ≈ **10 B**. Para 24 válvulas: **240 B**.

Buffer de sombra del LCD: 4 × 20 = **80 B** más 80 bits de marcas de suciedad (**10 B**).
La librería XBee reserva `MAX_FRAME_DATA_SIZE` (110 B) para la respuesta. Total del
proyecto muy por debajo del 75 % de 8 192 B. El mapa de instalación va en Flash
(`PROGMEM` vía `constexpr`), no en SRAM, y todas las cadenas literales usan `F()`.

**Regla operativa**: el uso de RAM reportado por PlatformIO se registra en cada commit que
toque la estructura de datos, según la puerta de calidad 3 de la constitución.

---

## R-012 — Compatibilidad del payload de aplicación (FR-015)

**Decisión**: el payload se conserva byte a byte respecto al sketch base:

```
byte 0 : 0xAA            encabezado
byte 1 : 0x14            código de operación (operar válvula)
byte 2 : id              identificador de válvula dentro del nodo (0..7)
byte 3 : nivel           nivel eléctrico solicitado (0 o 1) — lógica inversa, ver R-004
byte 4 : verificador     0xFF - (byte0 + byte1 + byte2 + byte3), truncado a 8 bits
```

**Verificación necesaria en banco**: el sketch base transmite 5 bytes con verificador,
mientras que las dos versiones anteriores (`WIGA_24IN_LCD`, `WIGA_24IN_LCDB1_5`)
transmitían solo 4, sin verificador. Los nodos deben aceptar la forma de 5 bytes — es la
que está desplegada con `CABLE_VIRTUAL___LATCH_V1`. Se confirma en la prueba de aceptación
antes de sustituir firmware en producción.

**Respuesta del nodo**: se interpretan los mismos 4 primeros bytes. El firmware base no
valida el verificador en recepción; el firmware migrado **sí lo valida cuando la trama
tiene 5 bytes**, y acepta tramas de 4 bytes sin verificador para no romper nodos con
firmware antiguo. Esta tolerancia se documenta en el contrato del protocolo.

---

## R-013 — Toolchain de pruebas en host sin instalar nada en el equipo

**Decisión**: el entorno de pruebas `native` usa la plataforma **`windows_x86`** de
PlatformIO, cuyo compilador (`toolchain-gccmingw32`, GCC 5.1) descarga y gestiona el
propio PlatformIO. Se añade un entorno `native_posix` con `platform = native` para
Linux y macOS.

**Motivo**: el equipo de desarrollo no tiene ningún compilador de C++ instalado —ni
MinGW, ni Visual Studio Build Tools, ni clang— y la plataforma `native` de PlatformIO
depende del compilador del sistema. Sin esto, `pio test -e native` sería imposible de
ejecutar y el Principio IV quedaría en el papel.

**Verificación**: se comprobó en banco que GCC 5.1 acepta `constexpr` relajado de C++14
con bucles y `static_assert` sobre él, que es lo que exige la validación del mapa de
instalación (R-010). Las 70 pruebas del proyecto se ejecutan en 18 s.

**Alternativas consideradas**:
- *Instalar MinGW-w64 o Visual Studio Build Tools en el equipo.* Rechazado: obliga a cada
  desarrollador a una instalación manual y hace que el proyecto deje de ser
  autocontenido, contra el requisito FR-030 ("sin pasos manuales").
- *Ejecutar las pruebas solo sobre la placa (`pio test -e megaatmega2560`).* Rechazado:
  exige hardware conectado para cada verificación, que es exactamente el ciclo lento que
  la migración pretende eliminar.

---

## R-014 — Tolerancia a un panel LCD caído

**Decisión**: el bus I2C se configura con `Wire.setWireTimeout(3000, true)`.

**Motivo**: sin límite de tiempo, la librería `Wire` de AVR espera **indefinidamente** a
que el esclavo libere la línea. Un LCD desconectado o un bus en cortocircuito colgaría el
lazo de control hasta que el watchdog reiniciara la placa, en un ciclo perpetuo de
reinicios. Con el límite, la transferencia aborta en 3 ms, el adaptador cuenta el fallo y
el control de válvulas sigue operando (FR-022, DP-03).

Tras `kLcdMaxConsecutiveFailures` fallos seguidos el adaptador deja de intentar escribir y
`healthy()` pasa a `false`, para que un bus caído no consuma presupuesto de ciclo.

---

## Resumen de decisiones

| Id | Decisión | Impacto |
|---|---|---|
| R-001 | Modo API escapado (AP=2) obligatorio | compatibilidad de campo |
| R-002 | Drenar el radio hasta vaciarlo cada ciclo | confirmaciones no se pierden |
| R-003 | Eliminar el vaciado de búfer previo a transmitir | menos disparos, menos batería |
| R-004 | Tipos separados nivel eléctrico / estado físico | la inversión deja de ser un bug latente |
| R-005 | Tiempo real con aritmética modular | correcto a 90 días |
| R-006 | Antirebote de 50 ms configurable | evita disparos por falso contacto |
| R-007 | LCD por buffer de sombra, 1 celda por vuelta, I2C 400 kHz | lazo no bloqueante |
| R-008 | Causa de reinicio vía `MCUSR` en `.init3` | diagnóstico en campo |
| R-009 | Lógica en `lib/`, barrera de capas estructural | pruebas en host reales |
| R-010 | Mapa único con `static_assert` | el error de mapeo se detecta al compilar |
| R-011 | Presupuesto de SRAM explícito | margen de pila garantizado |
| R-012 | Payload de 5 bytes conservado byte a byte | nodos de campo intactos |
| R-013 | Toolchain de host gestionado por PlatformIO | pruebas sin instalar nada |
| R-014 | Límite de tiempo en el bus I2C | un LCD caído no cuelga el control |
