# Contrato — Mapa de registros publicado al UC100

**Feature**: 002-modbus-esclavo-rs485 | **Fecha**: 2026-08-27

> **Este documento es la fuente de verdad única del mapa.** La configuración cargada en
> el UC100 debe coincidir con él. Si alguna vez discrepan el código, la documentación de
> operación y este contrato, manda este contrato.

---

## 1. Parámetros del enlace

| Parámetro | Valor | Nota |
|---|---|---|
| Medio | RS485 half-duplex | Transceptor MAX485, DE y RE unidos en **PH3 (pin 6)** |
| Baudios | 9600 | Heredado de la instalación |
| Formato | 8N1 | 8 datos, sin paridad, 1 de parada |
| Puerto en el Mega | **`Serial2`** · TXD2=PH1 (pin 16), RXD2=PH0 (pin 17) | La consola de diagnóstico va por `Serial` |
| Identificador de esclavo | **1** | Único esclavo del bus. Configurable en compilación, validado en 1..247 |
| Silencio entre tramas | **4 ms** | `3,5 × 11 bits / 9600 = 4,010 ms` |

---

## 2. Bloque de válvulas — direcciones 0 a 23

Un registro por válvula. La dirección coincide con el índice de sistema de la válvula,
que es también su posición en el panel LCD.

| Dirección | Contenido | Valores |
|---|---|---|
| `0` … `23` | Estado que el tablero **pide** para esa válvula | `0` = no riega · `1` = riega |

**Semántica de nivel.** El valor es un nivel sostenido, no un pulso ni un código de
orden. Mientras el interruptor del tablero pida riego, el registro vale `1` en **todos**
los sondeos. El controlador nunca lo devuelve a `0` por iniciativa propia.

**Es intención, no confirmación.** El valor dice lo que el tablero pide, no lo que la
válvula hace. El medio de radio no acusa recibo, de modo que nadie puede afirmar el
estado real del campo.

### Correspondencia con el mapa de instalación

| Registro | Válvula | Pin del Mega | Referencia XBee histórica |
|---:|---:|---:|---|
| 0 | 0 | 54 (A0) | `0x42878FDB` id 0 |
| 1 | 1 | 55 (A1) | `0x42878FDB` id 1 |
| 2 | 2 | 56 (A2) | `0x42878FDB` id 2 |
| 3 | 3 | 57 (A3) | `0x42878FDB` id 3 |
| 4 | 4 | 58 (A4) | `0x41C3F584` id 4 |
| 5 | 5 | 59 (A5) | `0x41C3F584` id 5 |
| 6 | 6 | 60 (A6) | `0x41C3F549` id 6 |
| 7 | 7 | 61 (A7) | `0x41C3F549` id 7 |
| 8 | 8 | 62 (A8) | `0x41C3F568` id 0 |
| 9 | 9 | 27 | `0x41C3F568` id 1 |
| 10 | 10 | 5 | `0x41C3F57A` id 2 |
| 11 | 11 | 8 | `0x41C3F57A` id 3 |
| 12 | 12 | 46 | `0x41C3F544` id 4 |
| 13 | 13 | 44 | `0x41C3F544` id 5 |
| 14 | 14 | 11 | `0x41C3F58B` id 6 |
| 15 | 15 | 12 | `0x41C3F58B` id 7 |
| 16 | 16 | 10 | `0x41C3F658` id 0 |
| 17 | 17 | 45 | `0x41C3F658` id 1 |
| 18 | 18 | 9 | `0x41C3F595` id 2 |
| 19 | 19 | 63 (A9) | `0x41C3F595` id 3 |
| 20 | 20 | 66 (A12) | `0x41C3F595` id 4 |
| 21 | 21 | 68 (A14) | `0x41C3F595` id 5 |
| 22 | 22 | 67 (A13) | `0x41C3F595` id 6 |
| 23 | 23 | 69 (A15) | `0x41C3F54F` id 7 |

Las columnas XBee son **referencia histórica** de la instalación original. No participan
en ningún direccionamiento y se conservan para poder contrastar el mapa contra el plano
de la finca.

> **Pendiente heredado de la especificación 001 (tarea T055):** las filas 18 a 22
> comparten el nodo `0x41C3F595`, un patrón distinto al del resto. Conviene contrastarlo
> contra el plano antes de sustituir firmware en producción.

---

## 3. Bloque de diagnóstico — direcciones 24 a 31

Solo lectura, para vigilar la salud del sistema desde el UC100 o desde un maestro de
escritorio.

| Dirección | Contenido | Unidad / rango |
|---:|---|---|
| `24` | Versión de firmware, `mayor << 8 \| menor` | — |
| `25` | Segundos desde la última petición válida | s, satura en 65535 |
| `26` | Tramas descartadas por verificador de redundancia | satura |
| `27` | Tramas descartadas por longitud o formato | satura |
| `28` | Peticiones válidas atendidas | satura |
| `29` | Causa del último reinicio (`MCUSR`) | mapa de bits del AVR |
| `30` | Número de válvulas pidiendo riego | 0..24 |
| `31` | Reservado | lee `0` |

El registro `29` permite distinguir en campo un reinicio por watchdog de un corte de
alimentación sin conectar un portátil.

Los contadores **saturan** en su máximo en lugar de dar la vuelta: un contador que vuelve
a cero miente sobre la salud del bus.

---

## 4. Funciones Modbus soportadas

Se admiten **cuatro funciones de lectura** sobre el mismo estado, de modo que el
instalador pueda elegir cualquier tipo de canal del UC100 sin recompilar el firmware.

| Código | Nombre | Vista | Rango válido |
|---|---|---|---|
| `0x01` | Read Coils | Bits: bit `n` = válvula `n` | 0..23 |
| `0x02` | Read Discrete Inputs | Bits, idénticos a FC01 | 0..23 |
| `0x03` | Read Holding Registers | Registros de 16 bits | 0..31 |
| `0x04` | Read Input Registers | Registros, idénticos a FC03 | 0..31 |

**Ninguna función de escritura está soportada.** FC05, FC06, FC15, FC16 y cualquier otra
responden excepción `0x01`. No es una carencia: el controlador es la única fuente de
verdad del estado pedido, y ninguna trama del bus puede alterarlo.

### Excepciones

| Código | Nombre | Cuándo |
|---|---|---|
| `0x01` | Illegal Function | Función no soportada, incluida toda escritura |
| `0x02` | Illegal Data Address | La dirección de inicio o el rango exceden el mapa |
| `0x03` | Illegal Data Value | Cantidad igual a cero o por encima del límite Modbus |

Una trama con verificador incorrecto o dirigida a otro identificador **no produce
excepción**: se descarta en silencio, como manda el estándar.

---

## 5. Configuración correspondiente en el UC100

Modo **Modbus Channels**. Un canal por válvula.

| Parámetro del canal | Valor |
|---|---|
| Slave ID | `1` |
| Address | la dirección de la tabla del apartado 2 |
| Quantity | `1` |
| Type | `0e` (`Hold16_AB`) o `00` (`Coil`), ambos soportados |

Ajustes de bus recomendados:

| Parámetro | Valor sugerido | Rango admitido |
|---|---|---|
| Execution Interval | 100 ms | 10..1000 ms |
| Max Resp Time | 1000 ms | 10..60000 ms |
| Max Retry Times | 3 | 0..5 |

**Reglas IF-THEN**, dos por válvula:

```
IF   canal de la válvula n  ==  1     THEN  enviar comando D2D de apertura
IF   canal de la válvula n  ==  0     THEN  enviar comando D2D de cierre
```

El parámetro **lockout time** de la regla de apertura es lo que produce la repetición del
comando de radio: al vencer, el UC100 comprueba que la condición sigue cumpliéndose y
vuelve a emitir. Un valor de `0` haría que la condición se detectase **una sola vez**, y
un comando perdido no se recuperaría nunca.

> **Límite del equipo, relevante para el dimensionamiento:** el UC100 admite **16 reglas
> IF-THEN** como máximo. A dos reglas por válvula, un UC100 gobierna **8 válvulas**. El
> reparto de válvulas entre equipos queda fuera del alcance de esta especificación.

---

## 6. Ejemplos de trama

Válvula 3 pidiendo riego, leída como registro de retención.

```
Petición   01 03 00 03 00 01 74 0A
           │  │  └─┬──┘ └─┬──┘ └─┬─┘
           │  │    │      │      └─ CRC16 (primero el byte bajo)
           │  │    │      └──────── cantidad = 1 registro
           │  │    └─────────────── dirección de inicio = 3
           │  └──────────────────── función 03
           └─────────────────────── esclavo 1

Respuesta  01 03 02 00 01 79 84
           │  │  │  └─┬──┘ └─┬─┘
           │  │  │    │      └───── CRC16
           │  │  │    └──────────── valor = 1  (la válvula 3 pide riego)
           │  │  └───────────────── 2 bytes de datos
           │  └──────────────────── función 03
           └─────────────────────── esclavo 1
```

Las 24 válvulas de una vez, como bobinas:

```
Petición   01 01 00 00 00 18 3C 00      (24 bobinas desde la 0)
Respuesta  01 01 03 09 00 00 EC 4C      (3 bytes: bit n = válvula n)
                     └─ 0x09 = 0b00001001 → válvulas 0 y 3 pidiendo riego
```

Petición fuera de rango, que debe producir excepción:

```
Petición   01 03 00 20 00 01 85 C0      (dirección 32, fuera del mapa)
Respuesta  01 83 02 C0 F1               (0x83 = 0x03 | 0x80 → excepción 02)
```

> Todos los verificadores de este apartado están **calculados y comprobados**, con la
> implementación contrastada contra el vector de referencia del estándar
> (`01 04 00 01 00 01` → `60 0A`). Sirven como vectores de prueba directos para la suite
> de `test/test_modbus/`.

---

## 7. Compromisos que el firmware no puede romper

| # | Compromiso |
|---|---|
| C-01 | Una válvula que pide riego devuelve `1` en **todos** los sondeos, sin una sola excepción, incluidos el arranque, el reinicio por watchdog y el desbordamiento del contador de tiempo a los 49,7 días |
| C-02 | Ante cualquier duda el esclavo **calla**. No responder es seguro; responder un valor equivocado no lo es |
| C-03 | El esclavo nunca transmite si no es en respuesta a una petición válida dirigida a él |
| C-04 | Ninguna trama recibida, por malformada que sea, provoca lectura o escritura fuera de los búferes |
| C-05 | El valor publicado es **intención**, y así se etiqueta en el panel y en la documentación |
