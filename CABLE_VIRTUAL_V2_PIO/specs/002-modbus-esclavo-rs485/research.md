# Fase 0 — Investigación y decisiones técnicas

**Feature**: 002-modbus-esclavo-rs485 | **Fecha**: 2026-08-27

Este documento no contiene incógnitas abiertas: la especificación entró a planificación
sin marcadores `NEEDS CLARIFICATION`. Recoge las decisiones de diseño, su fundamento y
las alternativas descartadas, más los cálculos que las sostienen.

Las cifras sobre el código vigente están **verificadas numéricamente** contra la rama
`001-migracion-platformio`, no estimadas.

---

## R-101 — Implementar Modbus en el repositorio, no usar una biblioteca de terceros

**Decisión**: escribir el esclavo Modbus RTU dentro del proyecto, en `lib/wiga_modbus`.

**Fundamento**: el Principio IV v2.0.0 exige que la lógica de protocolo sea código puro
comprobable en `env:native`. Las bibliotecas Modbus habituales para Arduino
(`ArduinoModbus`, `ModbusRTUSlave`, `SimpleModbusSlave`) dependen de `<Arduino.h>` y de
`Stream`, de modo que caerían del lado excluido por `lib_ignore` y volverían a dejar el
protocolo sin cobertura de pruebas. Sería repetir exactamente el error que originó los 25
defectos.

El alcance real es pequeño: cuatro funciones de lectura sobre un banco de registros, sin
escrituras, sin diagnóstico extendido, sin múltiples esclavos. Son unos cientos de líneas
de lógica pura y directa.

**Alternativas descartadas**:
- *Biblioteca de terceros*: rompe el Principio IV y añade una dependencia sobre la que no
  se pueden escribir las pruebas de regresión que FR-124 exige.
- *Envolver una biblioteca detrás de un puerto*: conserva el problema, porque lo que hay
  que probar es justamente el tramado que quedaría dentro de la biblioteca.

---

## R-102 — Delimitación de trama por longitud esperada, con el silencio como resincronizador

**Decisión**: analizar la petición de forma incremental. En cuanto se conocen el
identificador y el código de función, la longitud de la petición queda determinada (las
cuatro funciones soportadas usan peticiones de **8 bytes fijos**). Cuando se han recibido
exactamente esos bytes se verifica el CRC y se atiende. El silencio entre tramas se usa
para **descartar una trama parcial** y volver a sincronizar.

**Fundamento**: es más robusto que depender solo de la temporización. `millis()` tiene
resolución de 1 ms y el silencio reglamentario a 9600 baudios es de 4,0 ms, de modo que
una detección puramente temporal tiene un margen de error del 25 % y puede partir una
trama válida. Conocer la longitud elimina esa fragilidad en el caso normal y deja el
temporizador solo para el caso anómalo, donde la precisión no importa.

**Cálculo del silencio**: Modbus RTU define t3.5 sobre caracteres de 11 bits.

```
t3.5 = 3,5 × 11 bits / 9600 baudios = 4,010 ms  →  4 ms
t1.5 = 1,5 × 11 bits / 9600 baudios = 1,719 ms
```

**Alternativas descartadas**:
- *Solo silencio t3.5*: frágil con la resolución de `millis()`, y es la vía por la que la
  implementación actual acabó truncando toda trama a unos 4 bytes.
- *Usar `micros()` para precisión*: añade una segunda base de tiempo con su propio
  desbordamiento a los 71,6 minutos, a cambio de una precisión que el análisis por
  longitud ya hace innecesaria.

---

## R-103 — Transmisión no bloqueante con máquina de estados de dirección del bus

**Decisión**: no usar `Serial1.flush()`. El adaptador afirma DE, entrega los bytes al
búfer de transmisión de `Serial1`, calcula el instante de finalización y libera DE en una
vuelta posterior del lazo, cuando ese instante ha pasado **y** el búfer se ha vaciado.

**Fundamento**: el tiempo en el cable no es despreciable a 9600 baudios.

| Trama | Bytes | Tiempo en el cable |
|---|---:|---:|
| Petición FC03 | 8 | 8,3 ms |
| Respuesta FC03, 1 registro | 7 | 7,3 ms |
| Respuesta FC01, 24 bobinas | 8 | 8,3 ms |
| **Respuesta FC03, 24 registros** | **53** | **55,2 ms** |

Un `flush()` de la respuesta completa bloquearía **55 ms**, veintisiete veces el límite de
2 ms del Principio I. El búfer de transmisión del Mega es de 64 bytes, suficiente para la
respuesta mayor, así que `write()` retorna de inmediato.

La liberación de DE no puede hacerse antes de que el último bit salga del registro de
desplazamiento, o se corta la propia respuesta. De ahí la doble condición.

**Alternativas descartadas**:
- *`Serial1.flush()`*: viola el Principio I. Es lo que hace el código actual.
- *Interrupción de fin de transmisión (TXC)*: preciso, pero introduce estado compartido
  entre una ISR y el lazo, con el riesgo de lectura rota que el proyecto evita por norma.
- *Retardo fijo generoso*: desperdicia tiempo de bus y sigue siendo una espera.

---

## R-104 — Soportar cuatro funciones de lectura sobre el mismo estado

**Decisión**: implementar **FC01** (Read Coils), **FC02** (Read Discrete Inputs),
**FC03** (Read Holding Registers) y **FC04** (Read Input Registers). Las cuatro exponen
las mismas 24 válvulas: FC01 y FC02 como bits, FC03 y FC04 como registros de 16 bits con
valor `0` o `1`. Toda función de escritura responde excepción `0x01` (función ilegal).

**Fundamento**: el tipo de canal del UC100 lo elige el instalador en la puesta en marcha,
y la documentación del equipo admite `00=Coil`, `01=Discrete` e Input/Holding de 16, 32 y
64 bits. Soportar las cuatro lecturas cuesta poco —son dos vistas del mismo arreglo— y
elimina el único riesgo que no se puede probar en el laboratorio: que el mapa del
firmware no case con la configuración del puente. Sin esto, un cambio de tipo de canal en
campo exigiría recompilar y reprogramar el Mega.

Rechazar las escrituras no es una limitación sino una garantía: el controlador es la
**única fuente de verdad** del estado pedido, y ninguna trama del bus puede alterarlo.

**Alternativas descartadas**:
- *Solo FC03*: es el mínimo de la especificación, pero deja la puesta en marcha atada a
  una única elección de tipo de canal.
- *Soportar escrituras*: no hay nada que escribir, y abriría una vía para que el bus
  corrompiera el estado que el tablero determina.

---

## R-105 — Un registro por válvula, sin empaquetar

**Decisión**: 24 registros contiguos, uno por válvula, con valor `0` o `1`.

**Fundamento**: es una **exigencia derivada del UC100**, no una preferencia. Sus reglas
IF-THEN evalúan el *valor de un canal* contra un umbral (`Above`, `Within`, `Below`,
`Change`). Si varias válvulas compartieran un registro, el valor de ese registro no
permitiría condicionar cada válvula por separado y las reglas no podrían distinguirlas.

El coste es asumible: 24 de los 32 canales del puente, y 48 bytes de SRAM.

**Alternativas descartadas**:
- *Empaquetar 24 bits en 2 registros*: ahorra canales y memoria, pero **rompe el
  mecanismo de reglas** del UC100, que es lo único que convierte el estado en una orden
  de radio. Descartada por incompatibilidad funcional, no por estilo.

---

## R-106 — La conversión nivel↔estado se queda en el dominio

**Decisión**: el adaptador y la capa de protocolo **nunca** construyen a mano un
`SignalLevel` ni interpretan un bit como estado. La lectura del pin se convierte una sola
vez con `toValveState()` en el dominio, y lo que se publica en el registro es el estado
físico ya resuelto.

**Fundamento**: la inversión por transistor está documentada como característica del
hardware (`SignalLevel::High` equivale a `ValveState::Closed`), y los invariantes V-01 y
V-02 exigen un punto único de conversión. El defecto más grave de la implementación
actual es precisamente haberla hecho a mano en el adaptador y en dos sentidos distintos:

| requested | nivel TX | bit FC05 | bit RX | nivel RX | confirmed | |
|---|---|---|---|---|---|---|
| Closed | High | 1 | 1 | Low | **Open** | invertido |
| Open | Low | 0 | 0 | High | **Closed** | invertido |

El camino de escritura usaba `bit = rawOf(level)` y el de lectura
`level = NOT levelFromRaw(bit)`. Asimetría dentro del mismo archivo, con tres comentarios
contradictorios en 120 líneas.

Al desaparecer el camino de lectura —no hay confirmación— el riesgo se reduce a la mitad,
pero la regla se mantiene para que no reaparezca.

---

## R-107 — Qué se elimina y por qué es seguro

**Decisión**: retirar del binario y de la suite los módulos que quedan sin fuente de
datos o sin llamador.

| Módulo | Motivo | Verificación |
|---|---|---|
| `wiga_hal/rs485_modbus_adapter.*` | Motor de transacciones de maestro; rol invertido | 25 defectos catalogados |
| `wiga_ports/i_modbus_port.h` | Contrato de maestro (`send`/`poll`) | Sustituido por `i_byte_stream.h` |
| `wiga_control/retry_policy.*` | Reintenta hasta confirmar; no hay confirmación | D2D no tiene acuse de recibo |
| `wiga_control/latch_scheduler.*` | Reparte turnos de disparo; el controlador ya no dispara | El UC100 espacia por su intervalo entre canales |
| Fases `Awaiting`, `Backoff` de `ValvePhase` | Estados de espera de confirmación | Inalcanzables sin confirmación |
| `wiga_domain/app_protocol.*` | Protocolo XBee de 5 bytes | **Cero referencias** fuera de sus pruebas, comprobado por búsqueda en `lib/` y `src/` |
| `test/test_domain/test_app_protocol.cpp` | 13 pruebas sobre código muerto | Daban verde y cobertura aparente sobre algo que nadie invoca |
| `wiga_ports/i_radio_port.h`, `test/mocks/fake_radio.h` | Restos de XBee sin incluir por nadie | Se pudrirían sin que ningún build lo notase |

**Fundamento**: dejar código muerto en un firmware embebido no es neutro. Ocupa Flash,
confunde al siguiente que lo lea y —el caso de `app_protocol`— produce una señal falsa de
cobertura: 13 pruebas en verde que no protegen nada en producción.

---

## R-108 — Presupuesto de memoria

**Decisión**: el cambio debe quedar neutro o negativo en SRAM. Estimación:

| Concepto | Bytes |
|---|---:|
| Banco de registros (24 válvulas + 8 de diagnóstico, 16 bits) | +64 |
| Búfer de recepción (16 B) y de composición de respuesta (64 B) | +80 |
| Estado del esclavo (índice, marcas de tiempo, contadores) | +16 |
| **Añadido** | **+160** |
| `ValveRecord` pasa de 10 a 3 bytes por válvula, ×24 | −168 |
| `RetryPolicy` y `LatchScheduler` eliminados | −12 |
| **Retirado** | **−180** |
| **Neto estimado** | **−20** |

Línea base verificada: **1682 B de 8192 (20,5 %)**. El presupuesto del 75 % de NFR-001
queda con un margen enorme. En Flash se espera una reducción apreciable al retirar
`app_protocol`, el motor de maestro y dos módulos de control.

Una optimización adicional queda **anotada pero fuera de esta fase**: llevar a `PROGMEM`
las cadenas literales y el mapa de instalación, que hoy ocupan SRAM sin necesidad. No es
urgente con un 20 % de uso y añadiría accesos indirectos a código que conviene mantener
legible.

---

## R-109 — Estrategia de pruebas

**Decisión**: tres niveles, todos en `env:native`, sin hardware.

1. **Unitarias de protocolo** (`test/test_modbus/`, suite nueva). CRC16 contra vectores
   conocidos; construcción y análisis de las cuatro funciones; excepciones; y un bloque
   de **regresión por defecto catalogado**, escrito de modo que reintroducir el fallo
   ponga la suite en rojo. Casos obligatorios:
   - campo de longitud mentiroso, con `byteCount` de 250, 251 y 255;
   - petición que excede el rango de registros configurado;
   - trama dirigida a otro identificador de esclavo;
   - CRC incorrecto;
   - función no soportada y funciones de escritura;
   - trama fragmentada en varias entregas de bytes;
   - ruido antes de una petición legítima.

2. **Integración del supervisor** (`test/test_integration/`, reorientada). Que un cambio
   de interruptor se refleje en el registro tras el antirrebote; que el arranque siembre
   sin publicar `0` espurios; y una prueba nueva de **desbordamiento de `millis()` de
   extremo a extremo**, que hoy no existe en ninguna suite.

3. **Barrera de capas** (ya en CI). Incluir `<Arduino.h>` en `wiga_control` debe romper
   la compilación de pruebas. Se **extiende** para cubrir también `lib/wiga_modbus`, que
   es la biblioteca nueva y la que más importa mantener pura.

**Fundamento**: FR-124 exige un caso por defecto catalogado. Escribir esos casos primero
convierte la lista de la auditoría en una red de seguridad permanente en vez de un
informe que envejece.

**Sobre el doble de prueba**: el `FakeModbus` actual es infiel al adaptador real de tres
maneras conocidas (entrega cada reporte una sola vez, no puede expresar un fallo
persistente de envío, y su contador de enviados desborda su propia capacidad). Se
sustituye por `FakeByteStream`, que modela lo que de verdad hay debajo —una tubería de
bytes— y permite inyectar fragmentación, ruido y errores. Un doble a nivel de bytes no
puede mentir sobre el protocolo, porque no lo conoce.

---

## R-110 — Deuda de infraestructura que se resuelve en esta fase

| Deuda | Estado actual | Acción |
|---|---|---|
| Verificador de trazabilidad | Solo lee `specs/001-.../spec.md`; los `FR-101+` no están cubiertos | Extenderlo a todas las carpetas de `specs/` |
| Entorno `native` en CI | El CI solo ejecuta `native_posix`; `native` (Windows) nunca se ejercita | Documentar el desfase; el entorno de Windows se valida en local |
| Documentación de operación | `ARQUITECTURA_DEL_FIRMWARE.md`, `configuracion-finca.md` y `puesta-en-marcha.md` describen la topología de maestro con nodos remotos | Reescribir con la topología real |
| Watchdog documentado | El código usa `WDTO_8S`; la arquitectura documenta 2 s | Corregir la documentación al valor real |

---

## Defectos confirmados fuera del transporte, incluidos en el alcance

Los encontró la auditoría y se verificaron leyendo el código. No dependen del cambio de
rol, pero conviene cerrarlos mientras se toca el firmware.

| Defecto | Archivo | Análisis |
|---|---|---|
| Lectura fuera de la cadena | `lcd_display_adapter.cpp:82` | Cuando `text[c] == '\0'` coincide con un espacio ya presente, el `continue` salta la rama de terminación y el bucle sigue leyendo `text[16]`, `text[17]`… más allá del literal |
| Recuperación 15 veces más rápida de lo documentado | `lcd_display_adapter.cpp:103` | `recoveryCounter_` cuenta iteraciones de `loop()`, no vueltas de 15 ms, porque `display.tick()` está fuera del gate en `main.cpp:88`. El comentario dice «~66 × 15 ms = ~1 s» y no es cierto |
| Salud del panel ciega al fallo más común | `lcd_display_adapter.cpp:135` | Solo consulta `Wire.getWireTimeoutFlag()`, pero un LCD desconectado en caliente produce NACK y no expiración; `healthy_` nunca pasa a `false` y `kLcdMaxConsecutiveFailures` es código muerto |
| Telemetría que no consume nadie | `src/main.cpp:52` | Un comentario afirma que la salud del LCD se monitorea en el lazo. No es cierto: `healthy()` no se invoca en ningún punto, y `framingErrors()` se incrementa seis veces sin leerse jamás |

**Refutado durante la auditoría**: el riesgo de bucle de reinicio por watchdog en el
ATmega2560. La guarda canónica ya existe en `reset_cause.cpp:19-26`, con una función
`naked` en `.init3` que captura `MCUSR`, lo limpia y llama a `wdt_disable()` antes de
`main()`. No requiere acción.
