# Acta de pruebas de banco — Esclavo Modbus RTU (Spec 002)

Documento de aceptación para imprimir y firmar. Cubre el **nivel 2** de la
[guía de validación](../specs/002-modbus-esclavo-rs485/quickstart.md): el controlador como
esclavo Modbus frente a un maestro de escritorio, sin UC100 ni radio.

> **Regla de oro:** ningún resultado se anota de memoria. Cada casilla se marca en el
> momento, con la evidencia delante. Si algo no se probó, se marca **NP** (no probado), no
> se deja en blanco ni se supone correcto.
>
> **No confundir con la hoja antigua de la spec 001**, que probaba el ciclo de
> orden/confirmación XBee. Ese ciclo ya no existe: el controlador es un esclavo que solo
> publica lo que el tablero pide y no recibe confirmación del campo.

---

## Datos de la sesión

| Campo | Valor |
|---|---|
| Fecha | 2026 / 09 / 03 |
| Responsable de la prueba | ______________________________________________ |
| Versión de firmware (línea de la consola) | 2.0.0 |
| Fecha de compilación (consola) | ______________________________________ |
| Commit (`git rev-parse --short HEAD`) | ______________________________ |
| Maestro de escritorio | Docklight, modo HEX |

## Equipo utilizado

| Elemento | Modelo / serie | Observaciones |
|---|---|---|
| Arduino Mega 2560 | | |
| Transceptor MAX485 | | DE/RE al pin **6**, TXD2 pin 16, RXD2 pin 17 |
| Conversor USB-RS485 | | A/B y GND común |
| Panel LCD I2C 20×4 | | |
| Fuente | | tensión: ________ V |

**Montaje** (igual al de `quickstart.md` nivel 2):

```
   Arduino Mega 2560            MAX485                 USB-RS485
   TXD2  PH1 (pin 16) ────────▶ DI
   RXD2  PH0 (pin 17) ◀──────── RO           A ◀──────▶ A
   DE/RE PH3 (pin  6) ────────▶ DE + RE      B ◀──────▶ B
                                             GND ◀────▶ GND
```

---

## Bloque 0 — Requisito previo: el bus vive

Primero el enlace físico, antes que el protocolo.

### 0.1 — Saludo de banco (entorno `megaatmega2560_banco`)

> **Registro histórico.** El entorno `_banco` se retiró del firmware después de esta
> prueba. Para repetir el bloque 0 en una placa nueva, empezar directamente por 0.2.

```bash
pio run -e megaatmega2560_banco -t upload
```

Con un terminal a 9600 8N1 en modo texto debe leerse `WIGA 2.0 RS485 OK esclavo=1 9600-8N1`
de forma repetida.

| Qué se ve | Qué significa |
|---|---|
| Texto legible | A/B bien, masa común, velocidad correcta, DE/RE conmutando |
| Basura ilegible | A y B cruzados, o velocidad distinta |
| Nada | Sin masa común, DE/RE mal conectado, o transceptor sin alimentar |

> **No usar con el UC100 conectado.** Volver a producción (sin `_banco`) antes de seguir.

### 0.2 — Verificación de la trama conocida (FC03, versión)

| Enviar | Respuesta esperada | Obtenida | ✔ / ✘ / NP |
|---|---|---|---|
| `01 03 00 18 00 01 04 0D` | `01 03 02 02 00 B9 24` | `01 03 02 02 00 B9 24` | ✔ |

Valida cableado, velocidad 9600, slave ID, CRC y tramado de una sola vez. La respuesta
`02 00` es la versión de firmware 2.0: un valor **conocido**.

**Si no responde:** revisar el montaje del bloque 0.1. No continuar hasta que `0.2` pase.

Firma responsable del bloque 0: ______________________________

---

## Bloque 1 — Lectura del tablero (FC01 / FC03, mapa de pines)

### 1.1 — El mapa se ve de un golpe (24 bobinas, FC01)

| Enviar | Respuesta esperada | Obtenida | ✔ / ✘ / NP |
|---|---|---|---|
| `01 01 00 00 00 18 3C 00` | 3 bytes de bits con solo la entrada que está energizada | `01 01 03 01 00 00 6D 8E` | ✔ |

con solo la entrada **0** energizada, el primer byte de datos vale `01` (solo el bit 0).

Energiza **otra** entrada: ese bit debe moverse a la posición que le corresponde según el
mapa. Repetir por cada una de las 24 para validar el mapa completo contra el tablero real.

| Válvula energizada | Bit esperado (byte/bit) | Obtenido | ✔ / ✘ / NP |
|---|---|---|---|
| 0 | 0 / 0 | `01` | ✔ |

> **Criterio: 24 de 24.** Prestar atención a las válvulas **18 a 22**, cuyo patrón de nodo
> histórico difiere del resto.

### 1.2 — Una válvula por registro (FC03, registro individual)

| Enviar | Respuesta esperada | Obtenida | ✔ / ✘ / NP |
|---|---|---|---|
| `01 03 00 00 00 01 84 0A` (válvula 0) | `0` si reposo, `1` si riega | | |
| `01 03 00 03 00 01 74 0A` (válvula 3) | ídem | | |
| `01 03 00 17 00 01 34 0E` (válvula 23) | la última del mapa es alcanzable | | |

---

## Bloque 2 — Lo que debe RECHAZAR

Tan importante como que responda. Un esclavo que contesta a todo está tan roto como uno que
no contesta a nada.

| Nombre | Enviar | Esperar | Obtenido | ✔ / ✘ / NP |
|---|---|---|---|---|
| E1 otro esclavo | `02 03 00 18 00 01 04 3E` | **SILENCIO**, ni un byte | (silencio) | ✔ |
| E2 CRC malo | `01 03 00 18 00 01 00 00` | SILENCIO, y el registro 26 sube | | |
| E3 dir fuera rango | `01 03 00 20 00 01 85 C0` | `01 83 02 C0 F1` | | |
| E4 rango se pasa | `01 03 00 1E 00 05 E5 CF` | `01 83 02 C0 F1` | | |
| E5 cantidad cero | `01 03 00 00 00 00 45 CA` | `01 83 03 01 31` | | |
| E6 escritura FC06 | `01 06 00 00 00 01 48 0A` | `01 86 01 83 A0` | | |
| E7 escritura FC05 | `01 05 00 00 FF 00 8C 3A` | `01 85 01 83 50` | | |
| E8 función inexistente | `01 2A 00 00 00 01 D9 CC` | `01 AA 01 9F 60` | | |

**E6 y E7 importan especialmente**: confirman que **ninguna trama del bus puede alterar el
estado de una válvula**. El tablero es la única fuente de verdad.

**E1 y E2** conviene provocarlas a propósito una vez, para haber visto ese silencio cuando
es correcto: más adelante, ante una avería real, el síntoma será idéntico.

---

## Bloque 3 — Escenario B-08: una hora de sondeo continuo (FR-116, SC-105)

> **El escenario decisivo.** Con la válvula 3 regando, sondeamos sin parar durante **una
> hora**. Un solo `0` significa que el riego se cortaría en campo. **Tolerancia cero.**

| Comprobación | Esperado | Obtenido | ✔ / ✘ / NP |
|---|---|---|---|
| Sondeo continuo 1 hora | sin una sola lectura de `0` | ✔ |
| Lecturas de `1` en el 100 % de los sondeos | 100 % | ✔ |
| Reinicios por watchdog | **cero** | ✔ |
| Errores de CRC/formato (registros 26/27) | `0` | | |

**Resultado de la sesión del 2026-09-03:** B-08 completado en hardware físico (Mega 2560 +
MAX485 + Docklight) con la válvula 3 regando, `1` en el 100 % de los sondeos, sin una sola
lectura de `0`.

---

## Bloque 4 — Spec 003: separación de comandos por UC511

Procedimiento: [quickstart de la spec 003](../specs/003-retardo-apertura-cancelable/quickstart.md).
Maestro de escritorio leyendo los registros 0–8 cada 500 ms con marca de tiempo, y consola
serie abierta. V01/V02 comparten el UC511 #01; V09 pertenece al #05, no instalado.

### 4.1 — Banco (sin UC100)

| # | Acción en el tablero | Esperado | Obtenido | ✔ / ✘ / NP |
|---|---|---|---|---|
| B-1 | Subir V01 | reg 0 = `1` en el siguiente sondeo; consola `TX 0 1` | 2026-10-14: V01 abrió al instante (ejercitado en B-2 y B-6) | ✔ |
| B-2 | Con V01 arriba > 15 s: bajar V01 y subir V02 a la vez | reg 0 = `0` al instante; `WAIT 1`; reg 1 = `1` ~15 s después | 2026-10-14: V01 cerró al instante, V02 abrió a ~15 s, con UC100 y UC511 reales | ✔ |
| B-3 | Bajar V02 y subir V03 a la vez | reg 1 y reg 2 cambian en el mismo sondeo | 2026-10-14: las dos cambiaron a la vez | ✔ |
| B-4 | Justo tras B-3, subir y bajar V04 en 3 s | reg 3 nunca vale `1`; consola `CANC 3` | 2026-10-14: V04 no se movió | ✔ |
| B-5 | Subir V09 | reg 8 sigue en `0`; LCD `-` en V09; `NOINST 8` una vez | 2026-10-14: V09 siguió en `-`, nada se movió | ✔ |
| B-6 | Con V01 arriba, resetear la placa | reg 0 = `1` en el primer sondeo tras el arranque | 2026-10-14: V01 siguió en `1` y la válvula no cerró | ✔ |

### 4.2 — Campo (piloto: UC100 + UC511 #01–#04)

Antes de empezar: cableado verificado contra el mapa (V01/V02 en UC511 #01, salidas 1/2,
y así con los demás) ☐

| # | Prueba | Aprobada si | Obtenido | ✔ / ✘ / NP |
|---|---|---|---|---|
| C-1 | 10 cambios V01→V02 (mismo UC511) | las dos válvulas responden 10 de 10 | | |
| C-2 | 10 cambios V02→V03 (UC511 distintos) | sin espera añadida, 10 de 10 | | |
| C-3 | 10 pedidos revertidos dentro del intervalo | ninguna válvula se mueve | | |
| C-4 | 10 cierres pedidos 3 s después de abrir | las 10 cierran | 2026-10-14, 1 intento: con separación de 8 s el cierre se perdió; con 15 s cerró a los 15 s. Faltan 9 intentos | parcial |
| C-5 | 10 reinicios con válvulas regando | ninguna cierra | | |
| C-6 | Secuencia completa de un bloque (7 + 7 min) | primera 7:00, segunda 6:45 ± 2 s | | |

---

## Registro de memoria (puerta de calidad 3 de la constitución)

Del reporte de compilación de la versión probada:

| Recurso | Usado | Total | % | Límite |
|---|---|---|---|---|
| RAM | ________ B | 8 192 B | ______ % | 75 % |
| Flash | ________ B | 253 952 B | ______ % | — |

---

## Resultado

| Bloque | Resultado |
|---|---|
| 0 — El bus vive (0.1 y 0.2) | ☐ Aprobado ☐ Rechazado ☐ NP |
| 4 — Spec 003, banco (B-1..B-6) y campo (C-1..C-6) | Banco: ☑ Aprobado (2026-10-14, separación 15 s) · Campo: ☐ pendiente |
| 1 — Lectura del tablero (mapa, 24/24) | ☐ Aprobado ☐ Rechazado ☐ NP |
| 2 — Lo que debe rechazar | ☐ Aprobado ☐ Rechazado ☐ NP |
| 3 — B-08, una hora de sondeo continuo | ☐ Aprobado ☐ Rechazado ☐ NP |

### Dictamen

- ☐ **Apto para sustituir firmware en producción.** Todos los bloques aprobados.
- ☐ **Apto con reservas.** Detallar debajo qué queda pendiente y bajo qué condición.
- ☐ **No apto.** Detallar el motivo.

Observaciones, incidencias y desviaciones:

```
_________________________________________________________________________

_________________________________________________________________________

_________________________________________________________________________

_________________________________________________________________________
```

| | Nombre | Firma | Fecha |
|---|---|---|---|
| Ejecutó la prueba | | | |
| Responsable técnico de firmware | | | |

---

## Después de la sesión

1. Escanear esta acta y guardarla junto al repositorio, o adjuntarla al pull request.
2. Marcar en [`tasks.md`](../specs/002-modbus-esclavo-rs485/tasks.md) la tarea **T064** como
   completada según lo verificado.
3. Si se probó la lectura del mapa contra el plano (bloque 1), registrar las discrepancias
   en [`configuracion-finca.md`](configuracion-finca.md).
4. Si el bloque 1.7 de la hoja antigua (el que no reportó `WATCHDOG`) aplica, documentar la
   limitación en [`diagnostico.md`](diagnostico.md).
