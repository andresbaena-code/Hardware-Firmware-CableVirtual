# Diagnóstico en campo

El controlador ya no transmite órdenes por radio: es un **esclavo Modbus** que publica en
un banco de registros el estado pedido por los 24 interruptores del tablero, y contesta
cuando el UC100 (maestro) le pregunta por RS485. La confirmación del campo no existe: el
protocolo de radio no acusa recibo (constitución v2.0.0, Principio II).

Por eso hay **dos** formas de diagnosticar, complementarias:

1. **Registros Modbus** — sin abrir la carcasa ni conectar un portátil, leyendo el bloque
   de diagnóstico desde el maestro (o un maestro de escritorio).
2. **Consola serie** — con un portátil y un cable USB para ver evento a evento.

---

## 1 · Diagnosticar por los registros Modbus

El bloque de diagnóstico vive en las direcciones 24 a 31 (ver
[`contracts/register-map.md`](../specs/002-modbus-esclavo-rs485/contracts/register-map.md)).

| Registro | Síntoma que ayuda a ver | Interpretación |
|---|---|---|
| 0..23 (válvulas) | Una válvula no riega | Si vale `1`, el controlador cumple: el problema está en el puente, la radio o el actuador |
| 25 (segundos sin sondeo) | Nada responde | Si crece sin parar, el maestro no pregunta: UC100 apagado o cable RS485 cortado |
| 26 (errores de CRC) | Comportamiento errático | Si sube, hay ruido eléctrico o desajuste de parámetros serie |
| 27 (errores de formato) | Ídem | Ídem |
| 28 (peticiones válidas) | Referencia de actividad | Debe subir con cada sondeo bien formado |
| 29 (causa de reinicio) | Reinicios inesperados | Distingue watchdog de corte de alimentación |
| 30 (válvulas regando) | Coherencia | Debe cuadrar con el panel |

**Regla de oro:** el controlador solo responde a una pregunta. Si no le preguntan, su
**silencio es idéntico** ante "maestro apagado", "cable cortado" o "esclavo sano": el
registro 25 es lo que distingue los tres casos.

---

## 2 · Consola serie

```bash
pio device monitor -b 9600
```

### Formato de la salida

```
<tiempo_ms> [<TAG>] v=<válvula> x=<valor>
```

El tiempo es milisegundos desde el arranque. `v=--` significa que el evento no
corresponde a una válvula concreta.

### Catálogo de eventos

| Tag | Cuándo aparece | `x=` significa |
|---|---|---|
| `BOOT` | al arrancar | bits de `MCUSR`, causa del último reinicio |
| `TX` | el nivel pedido por un interruptor cambió | nivel que el tablero pide (0 o 1) |
| `DROP` | se descartó una trama, o el panel dejó de responder | motivo o dato de contexto |
| `LCD` | el panel quedó marcado como no sano | fallos consecutivos |
| `WAIT` | un cambio de válvula espera su turno porque su UC511 tuvo otro cambio hace menos de 8 s | número de UC511 |
| `CANC` | un cambio que esperaba turno se descartó porque el tablero lo revirtió | número de UC511 |
| `NOINST` | el tablero pidió riego en una válvula de un UC511 no instalado (el registro sigue en 0) | número de UC511 |

`WAIT` es normal en el cambio de una válvula a la otra del mismo UC511: la que abre sale
8 s después de que cierre su vecina. Un `TX` de esa válvula 8 s después cierra el ciclo.

En el panel, una válvula de un UC511 no instalado se muestra con **`-`** en vez de `0`/`1`.
Si aparece `-` en una válvula que sí está instalada, falta marcar su equipo en `kActuators`
([configuracion-finca.md](configuracion-finca.md)).

Al arrancar, antes de `BOOT`, aparecen tres líneas sin tag: nombre del firmware, versión y
fecha de compilación.

### Una sesión normal

```
0 WIGA Cable Virtual
0 2.0.0
0 Sep 03 2026 14:32:10
1 [BOOT] v=-- x=1
1 ALIMENTACION
4210 [TX] v=7 x=1
```

El operador accionó el interruptor 7 y el registro de esa válvula pasó a `1` tras la
ventana de antirrebote. No hay confirmación: el controlador no sabe (ni puede saber) si la
válvula de campo se movió; solo publica lo que el tablero pide.

---

## 3 · Los fallos típicos, ordenados

### 1. El maestro no pregunta (fallo de bus o de UC100)

Síntoma: el registro **25** crece sin parar y el panel cambia a `PEDIDO - SIN BUS`.

Es el fallo más probable en campo (FR-114). Revisar, en este orden: alimentación del UC100,
conector RS485, y que A/B no estén cruzados. El controlador **sigue funcionando** y
responderá en cuanto vuelva el maestro.

### 2. Una válvula no riega aunque el registro vale `1`

El controlador publica el nivel pedido; el problema está en el puente (UC100), la radio o
el actuador. Revisar las reglas IF-THEN del UC100, el `lockout time`, y el equipo de campo.

### 3. El panel no cambia al accionar un interruptor (fallo de entrada o panel)

El firmware no vio el cambio (no aparece `TX`), o el panel dejó de responder (evento
`LCD`). Comprobar con un multímetro el pin correspondiente en el Mega y el cable del LCD.
Recuerde el antirrebote de 50 ms: un contacto sucio puede no estabilizarse nunca.

---

## Causa del último reinicio

El evento `BOOT` va seguido de una línea con la causa dominante:

| Texto | Qué pasó |
|---|---|
| `ALIMENTACION` | arranque normal tras conectar la alimentación |
| `EXTERNO` | alguien pulsó reset, o el cargador reinició la placa |
| `BROWNOUT` | la tensión cayó por debajo del umbral — **revisar la batería** |
| `WATCHDOG` | el lazo de control dejó de progresar — **es un fallo de firmware** |

`WATCHDOG` es el único que apunta al firmware. Si aparece, reportarlo con el contexto de
la consola: significa que una vuelta del lazo tardó más de 8 segundos, algo que el diseño
no permite.

### Limitación conocida

Algunos bootloaders (`optiboot`) limpian `MCUSR` antes de que el firmware pueda leerlo. Si
en banco se provoca un reinicio por watchdog y **no** aparece `WATCHDOG`, es esta
limitación, no un defecto del diagnóstico. El control de válvulas no depende de este dato.

---

## Qué mira el técnico primero

1. ¿El registro 25 sube sin parar? → el maestro no pregunta: bus o UC100.
2. ¿Aparece el evento `TX` al conmutar una entrada? → **No**: problema de entrada o
   cableado.
3. ¿El registro de esa válvula vale `1` pero no riega? → el problema está en el puente, la
   radio o el actuador.
4. ¿Aparece `LCD`? → el panel está desconectado o su bus falla; el control sigue operando.
5. ¿Aparece `WATCHDOG` en el arranque? → reportar al equipo de firmware.

## Silenciar el diagnóstico

Para compilar sin trazas:

```bash
pio run -e megaatmega2560_silent
```

Ahorra unos 790 bytes de Flash y 104 de RAM, y **no cambia nada más** del comportamiento.
Los niveles son 1 = solo errores, 2 = + avisos, 3 = + información (por defecto).
