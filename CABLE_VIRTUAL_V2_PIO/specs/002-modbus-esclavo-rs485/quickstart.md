# Guía de validación — Esclavo Modbus RTU

**Feature**: 002-modbus-esclavo-rs485 | **Fecha**: 2026-08-27

Cómo comprobar que la funcionalidad hace lo que dice, en tres niveles: escritorio, banco
con maestro simulado, y campo con el UC100 real.

Referencias: [contracts/register-map.md](contracts/register-map.md) para el mapa,
[contracts/modbus-slave.md](contracts/modbus-slave.md) para el comportamiento en el bus.

---

## Nivel 1 — Escritorio, sin hardware

Es el nivel que más defectos atrapa y el más barato. Debe pasar antes de tocar una placa.

```bash
pio test -e native
```

En Linux o macOS, y en integración continua:

```bash
pio test -e native_posix
```

**Línea base antes de esta funcionalidad**: 73 pruebas en verde en 19 s, de las cuales
**ninguna** ejercitaba el código Modbus, porque vivía en `lib/wiga_hal` y el entorno de
pruebas lo excluye con `lib_ignore`. Cerrar ese hueco es el objetivo de la suite nueva.

### Qué debe cubrir la suite nueva `test/test_modbus/`

| Grupo | Comprobación |
|---|---|
| CRC | Contra el vector de referencia del estándar: `01 04 00 01 00 01` → `60 0A` |
| Tramado | Las cuatro funciones de lectura, con los vectores del contrato |
| Fragmentación | Una petición entregada byte a byte se reensambla igual que de una vez |
| Silencio | Una trama parcial seguida de 4 ms de silencio se descarta y no contamina la siguiente |
| Direccionamiento | Una petición a otro identificador produce **silencio absoluto** |
| Verificador | Un CRC incorrecto produce silencio y suma al contador |
| Excepciones | Función no soportada → `0x01`; rango fuera del mapa → `0x02`; cantidad inválida → `0x03` |
| Escrituras | FC05, FC06, FC15 y FC16 producen excepción `0x01` |

### Regresión por defecto catalogado (FR-124)

Un caso por defecto de la auditoría. Reintroducir cualquiera debe poner la suite en rojo.

| Defecto | Caso de prueba |
|---|---|
| Lectura fuera de búfer por longitud mentirosa | Tramas con campo de longitud `250`, `251` y `255`. El `251` es el que hoy hace que `(uint8_t)(3+251+2)` valga `0` y se lean 254 bytes de un búfer de 32 |
| Truncamiento que dejaba 12 de 24 válvulas invisibles | Leer las 24 válvulas y exigir las 24 en la respuesta |
| Inversión de polaridad | Con el interruptor en posición de riego, el registro vale `1`; en reposo, `0`. En ambos sentidos |
| Verificador de respuesta sin comprobar | Trama con CRC malo descartada, nunca atendida |
| Excepciones Modbus no detectadas | La respuesta de excepción lleva el bit `0x80` en el código de función |

### Barrera de capas

El CI la comprueba rompiéndola a propósito: incluir `<Arduino.h>` en `lib/wiga_control`
debe hacer fallar la compilación de pruebas. Con esta funcionalidad la comprobación
**se extiende a `lib/wiga_modbus`**, que es la biblioteca nueva y la que más importa
mantener pura.

```bash
pio run -e megaatmega2560            # compila para la placa
pio run -e megaatmega2560_silent     # sin trazas, no debe cambiar el comportamiento
```

**Presupuesto de memoria** (NFR-001, por debajo del 75 %). Línea base verificada:
RAM 20,5 % (1682 / 8192 B), Flash 4,6 % (11806 B). Se espera quedar igual o por debajo,
porque se retira más código del que se añade.

---

## Nivel 2 — Banco, con maestro Modbus simulado

Valida el comportamiento en el cable sin depender del UC100 ni de la radio.

### Montaje

```
   Arduino Mega 2560            MAX485                 Adaptador USB-RS485
   ─────────────────           ────────                ───────────────────
   TXD2  PH1 (pin 16) ────────▶ DI
   RXD2  PH0 (pin 17) ◀──────── RO           A ◀──────▶ A
   DE/RE PH3 (pin  6) ────────▶ DE + RE      B ◀──────▶ B
                                             GND ◀────▶ GND
```

### Paso 0 — validar el cable antes que el protocolo

Antes de hablar Modbus conviene comprobar que el enlace fisico existe. El entorno de
banco emite cada 2 s un texto legible por el RS485:

```bash
pio run -e megaatmega2560_banco -t upload
```

Con un terminal a 9600 8N1 en modo texto debe leerse `WIGA 2.0 RS485 OK esclavo=1
9600-8N1` de forma repetida.

| Qué se ve | Qué significa |
|---|---|
| El texto legible | A/B bien, masa común, velocidad correcta, DE/RE conmutando |
| Basura ilegible | A y B cruzados, o velocidad distinta |
| Nada | Sin masa común, DE/RE mal conectado, o transceptor sin alimentar |

Esa distincion entre *basura* y *nada* es justo la que no se obtiene sondeando un
esclavo: su silencio es identico ante los tres fallos.

> **No usar con el UC100 conectado.** Un esclavo Modbus no debe transmitir sin que le
> pregunten; el saludo colisionaria con el maestro. Por eso vive en un entorno aparte y
> el build lo avisa con un `#warning`. Volver a produccion antes de seguir.

### Sondeo con maestro de escritorio

Con cualquier maestro de escritorio: `mbpoll`, QModMaster o ToolBox de Milesight.

```bash
mbpoll -m rtu -b 9600 -P none -d 8 -s 1 -a 1 -t 4 -r 1 -c 24 COM5
```

`-t 4` son registros de retención, `-r 1` empieza en el registro 0 (mbpoll numera desde
1), `-c 24` lee las 24 válvulas.

### Secuencias para Docklight

Todos los verificadores estan **calculados**, no transcritos, con la implementacion
contrastada contra el vector de referencia del estandar (`01 04 00 01 00 01` -> `60 0A`).
En Docklight se crean como *Send Sequences* en modo **HEX**.

Respuesta de lectura de un registro de valvula, para no repetirla en cada fila:

| Valor | Respuesta |
|---|---|
| `0` no riega | `01 03 02 00 00 B8 44` |
| `1` riega | `01 03 02 00 01 79 84` |

#### A · El bus vive

| Nombre | Enviar | Esperar | Qué demuestra |
|---|---|---|---|
| `A1 version` | `01 03 00 18 00 01 04 0D` | `01 03 02 02 00 B9 24` | Valor CONOCIDO y distinto de cero: valida cableado, velocidad, slave ID, CRC y tramado de una vez |
| `A2 peticiones` | `01 03 00 1C 00 01 45 CC` | sube en cada envio | Que cada trama se entiende y se contabiliza, no que hay un eco casual |

#### B · Lectura del tablero

| Nombre | Enviar | Qué demuestra |
|---|---|---|
| `B1 valvula 0` | `01 03 00 00 00 01 84 0A` | Coincide con el primer digito de E1 en el panel |
| `B2 valvula 3` | `01 03 00 03 00 01 74 0A` | Otra valvula distinta responde por separado |
| `B3 valvula 23` | `01 03 00 17 00 01 34 0E` | La ultima del mapa es alcanzable |
| `B4 las 24 reg` | `01 03 00 00 00 18 45 C0` | 53 bytes: `01 03 30` + 48 de datos + CRC |
| `B5 las 24 bits` | `01 01 00 00 00 18 3C 00` | 3 bytes de bits. **La mas util**: se ve el mapa entero de un golpe |

`B5` es la mejor para validar el mapa de pines: energiza UNA entrada, mira que bit se
enciende, y comprueba que es el que corresponde a esa valvula. Repetido por las 24,
valida el mapa completo contra el tablero real.

#### C · Diagnostico

| Nombre | Enviar | Qué esperar |
|---|---|---|
| `C1 bloque` | `01 03 00 18 00 08 C4 0B` | Los 8 registros de diagnostico de una vez |
| `C2 seg sin sondeo` | `01 03 00 19 00 01 55 CD` | `FFFF` antes del primer sondeo, luego crece desde `0` |
| `C3 errores CRC` | `01 03 00 1A 00 01 A5 CD` | `0` en un bus sano; sube al enviar `E2` |
| `C4 errores formato` | `01 03 00 1B 00 01 F4 0D` | `0` en un bus sano |
| `C5 causa reinicio` | `01 03 00 1D 00 01 14 0C` | Mapa de bits de `MCUSR`: distingue watchdog de corte de luz |
| `C6 valv. regando` | `01 03 00 1E 00 01 E4 0C` | Cuenta de entradas activas; debe cuadrar con el panel |

#### D · Tipos alternativos de canal del UC100

| Nombre | Enviar | Qué demuestra |
|---|---|---|
| `D1 FC04` | `01 04 00 00 00 01 31 CA` | Devuelve lo mismo que FC03: el instalador puede elegir `Input16` |
| `D2 FC02` | `01 02 00 00 00 18 78 00` | Devuelve lo mismo que FC01: puede elegir `Discrete` |

Conviene probarlas: si el canal del UC100 se configura con otro tipo, el firmware
responde igual sin recompilar.

#### E · Lo que debe RECHAZAR

Tan importante como que responda. Un esclavo que contesta a todo esta tan roto como uno
que no contesta a nada, y su comportamiento correcto ante casi todo es **callar**.

| Nombre | Enviar | Esperar |
|---|---|---|
| `E1 otro esclavo` | `02 03 00 18 00 01 04 3E` | **SILENCIO**, ni un byte |
| `E2 CRC malo` | `01 03 00 18 00 01 00 00` | **SILENCIO**, y `C3` sube |
| `E3 dir fuera rango` | `01 03 00 20 00 01 85 C0` | `01 83 02 C0 F1` |
| `E4 rango se pasa` | `01 03 00 1E 00 05 E5 CF` | `01 83 02 C0 F1` |
| `E5 cantidad cero` | `01 03 00 00 00 00 45 CA` | `01 83 03 01 31` |
| `E6 escritura FC06` | `01 06 00 00 00 01 48 0A` | `01 86 01 83 A0` |
| `E7 escritura FC05` | `01 05 00 00 FF 00 8C 3A` | `01 85 01 83 50` |
| `E8 funcion inexistente` | `01 2A 00 00 00 01 D9 CC` | `01 AA 01 9F 60` |

`E6` y `E7` importan especialmente: confirman que **ninguna trama del bus puede alterar
el estado de una valvula**. El tablero es la unica fuente de verdad.

`E1` y `E2` conviene provocarlas a proposito una vez, para haber visto ese silencio
cuando es correcto: mas adelante, ante una averia real, el sintoma sera identico.

### Escenarios

| # | Qué se hace | Qué debe pasar | Requisito |
|---|---|---|---|
| B-01 | Sondear con los 24 interruptores en reposo | 24 registros a `0`, trama bien formada | FR-101, SC-101 |
| B-02 | Subir el interruptor de la válvula 3 y esperar 50 ms | Solo el registro 3 pasa a `1` | FR-109, SC-103 |
| B-03 | Sondear dirigiendo la petición al esclavo `2` | **Silencio absoluto** | FR-103 |
| B-04 | Enviar una trama con CRC alterado | Silencio; el contador del registro 26 sube | FR-104, FR-113 |
| B-05 | Pedir la dirección 32 | Excepción `01 83 02 C0 F1` | FR-106 |
| B-06 | Intentar escribir con FC06 | Excepción `0x01` | FR-106 |
| B-07 | Leer las 24 válvulas como bobinas con FC01 | Los mismos estados que por FC03 | FR-109 |
| **B-08** | **Con la válvula 3 regando, sondear sin parar 1 hora** | **`1` en el 100 % de los sondeos. Ni un solo `0`** | **FR-116, SC-105** |
| B-09 | Con la válvula 3 regando, pulsar reset | Al volver, el registro 3 vuelve a `1`. Durante el reinicio hay expiración, **nunca un `0`** | FR-116 |
| B-10 | Desconectar el maestro 5 minutos | El registro 25 crece; el panel lo señala; el controlador sigue funcionando | FR-114 |
| B-11 | Medir la ocupación del lazo con osciloscopio en un pin de prueba | Por debajo de 2 ms por vuelta | SC-104 |

**B-08 es el escenario decisivo.** Un solo `0` en toda la hora significa que el riego se
cortaría en campo. Tolerancia cero.

### Vectores rápidos

Se pueden inyectar tal cual por el puerto serie:

```
01 03 00 03 00 01 74 0A     válvula 3 como registro     → 01 03 02 00 01 79 84
01 01 00 00 00 18 3C 00     24 válvulas como bobinas    → 01 01 03 .. .. .. CRC
01 03 00 20 00 01 85 C0     dirección fuera del mapa    → 01 83 02 C0 F1
02 03 00 00 00 01 ...       dirigida a otro esclavo     → (silencio)
```

---

## Nivel 3 — Campo, con el UC100 real

### Configuración del puente

Modo **Modbus Channels**, un canal por válvula:

| Parámetro | Valor |
|---|---|
| Slave ID | `1` |
| Address | la dirección de la válvula, 0 a 23 |
| Quantity | `1` |
| Type | `0e` (`Hold16_AB`) o `00` (`Coil`) |
| Baud / formato | 9600, 8, sin paridad, 1 de parada |
| Execution Interval | 100 ms |
| Max Resp Time | 1000 ms |
| Max Retry Times | 3 |

Dos reglas IF-THEN por válvula, una de apertura y otra de cierre.

> **Comprobar el `lockout time` de la regla de apertura.** Es lo que hace que el UC100
> vuelva a emitir el comando de radio mientras la condición siga cumpliéndose. Con valor
> `0` la condición se detecta **una sola vez** y un comando perdido no se recupera nunca.
> Es el ajuste más fácil de pasar por alto y el que sostiene toda la estrategia de
> ráfagas.

### Escenarios

| # | Qué se hace | Qué debe pasar |
|---|---|---|
| C-01 | Botón *Fetch* del ToolBox sobre un canal | Devuelve el valor correcto de esa válvula |
| C-02 | Subir un interruptor del tablero | La válvula del campo abre en menos de un segundo tras el siguiente sondeo |
| C-03 | Bajar el interruptor | La válvula cierra |
| C-04 | Dejar una válvula regando más que el temporizador del UC501 | El riego continúa: la re-emisión del UC100 reinicia el temporizador del actuador |
| C-05 | Apagar el UC100 con una válvula regando | El actuador cierra solo al vencer su temporizador. **Es la red de seguridad contra inundación** |
| C-06 | Desconectar el cable RS485 | El panel señala la ausencia de sondeo; el controlador sigue funcionando |

### Antes de sustituir firmware en producción

- [ ] Nivel 1 completo en verde
- [ ] Nivel 2 completo, con **B-08 sin una sola lectura de `0`**
- [ ] Mapa de la finca contrastado contra el plano, con atención a las válvulas 18 a 22
      *(tarea T055 heredada de la especificación 001, todavía abierta)*
- [ ] Temporizador de auto-apagado configurado y **verificado** en cada UC501
- [ ] `lockout time` configurado en las reglas de apertura de cada UC100
- [ ] Consumo de RAM y Flash registrado en el commit o el PR

---

## Diagnóstico rápido en campo

Sin conectar un portátil, leyendo el bloque de diagnóstico:

| Síntoma | Registro a mirar | Interpretación |
|---|---|---|
| Una válvula no riega | El registro de esa válvula | Si vale `1`, el controlador cumple: el problema está en el puente, la radio o el actuador |
| Nada responde | 25 (antigüedad del último sondeo) | Si crece sin parar, el maestro no está preguntando: UC100 apagado o cable cortado |
| Comportamiento errático | 26 y 27 (errores de CRC y formato) | Si suben, hay ruido eléctrico o desajuste de parámetros serie |
| Reinicios inesperados | 29 (causa del último reinicio) | Distingue watchdog de corte de alimentación |

Con el portátil, por la consola a 9600 baudios: versión, fecha de compilación, causa del
último reinicio y cada cambio de válvula.
