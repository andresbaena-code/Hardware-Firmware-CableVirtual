# Diagnóstico en campo

Con un portátil y un cable USB se puede determinar en minutos si una válvula
problemática es un fallo de radio, de entrada del tablero o de la válvula misma.

```bash
pio device monitor -b 9600
```

---

## Formato de la salida

```
<tiempo_ms> [<TAG>] v=<válvula> x=<valor>
```

El tiempo es milisegundos desde el arranque. `v=--` significa que el evento no
corresponde a una válvula concreta.

## Catálogo de eventos

| Tag | Cuándo aparece | `x=` significa |
|---|---|---|
| `BOOT` | al arrancar | bits de `MCUSR`, causa del último reinicio |
| `TX` | se transmitió una orden | número de intento (1 a 5) |
| `ACK` | el nodo confirmó | nivel eléctrico confirmado (0 o 1) |
| `TMO` | venció la espera sin confirmar | intentos consumidos |
| `BACK` | la válvula entra en reposo de 1 h | fallos acumulados de esa válvula |
| `DROP` | se descartó una trama o un envío falló | motivo o dato de contexto |
| `LCD` | el panel dejó de responder | fallos consecutivos |

Al arrancar, antes de `BOOT`, aparecen tres líneas sin tag: nombre del firmware, versión
y fecha de compilación.

## Una sesión normal

```
0 WIGA Cable Virtual
0 2.0.0
0 Aug 20 2026 14:32:10
1 [BOOT] v=-- x=1
1 ALIMENTACION
4210 [TX] v=7 x=1
4290 [ACK] v=7 x=0
```

La válvula 7 recibió la orden y el nodo confirmó 80 ms después. Todo bien.

---

## Los tres fallos típicos

### 1. El nodo no responde (fallo de radio)

```
4210 [TX] v=7 x=1
34215 [TMO] v=7 x=1
34216 [TX] v=7 x=2
64220 [TMO] v=7 x=2
...
154240 [BACK] v=7 x=5
```

Cinco intentos, ninguna confirmación, y a reposo. **El firmware está bien; el problema
está fuera.** Revisar, en este orden: alimentación del nodo, antena y alcance, y que el
XBee del nodo esté en la red correcta (`ID`, `SC`).

Tras el `BACK` no habrá otro `TX` hasta pasada una hora. Es deliberado: protege la batería
y los solenoides. Para forzar un intento inmediato, conmutar la entrada del tablero.

### 2. La entrada del tablero no llega (fallo de entrada)

El operador acciona el tablero y **no aparece ningún `TX`**.

El firmware no vio el cambio. Comprobar con un multímetro el pin correspondiente en el
Mega: en reposo debe leer ~5 V (pull-up interno) y al accionar debe bajar a ~0 V. Si el
nivel no cambia, el problema es el cableado o el contacto del tablero, no el firmware.

Si el nivel oscila, puede ser el antirebote haciendo su trabajo: el firmware exige 50 ms
de estabilidad antes de actuar. Un contacto muy sucio puede no llegar nunca a estabilizarse.

### 3. El nodo confirma un estado distinto (fallo de válvula o de nodo)

```
4210 [TX] v=7 x=1
4290 [DROP] v=7 x=1
34215 [TMO] v=7 x=1
```

El nodo respondió, pero con un estado que **no coincide** con el solicitado. El radio
funciona; lo que falla está en el nodo o en la válvula: solenoide agarrotado, cableado de
la salida, o batería del nodo demasiado baja para completar el pulso latch.

El firmware no da la válvula por confirmada, que es lo correcto.

---

## Causa del último reinicio

El evento `BOOT` va seguido de una línea con la causa dominante:

| Texto | Qué pasó |
|---|---|
| `ALIMENTACION` | arranque normal tras conectar la alimentación |
| `EXTERNO` | alguien pulsó reset, o el cargador reinició la placa |
| `BROWNOUT` | la tensión cayó por debajo del umbral — **revisar la batería** |
| `WATCHDOG` | el lazo de control dejó de progresar — **es un fallo de firmware** |

`WATCHDOG` es el único que apunta al firmware. Si aparece, hay que reportarlo con el
contexto de la consola: significa que una vuelta del lazo tardó más de 8 segundos, algo
que el diseño no permite.

### Limitación conocida

Algunos bootloaders (`optiboot`) limpian `MCUSR` antes de que el firmware pueda leerlo.
Si en banco se provoca un reinicio por watchdog y **no** aparece `WATCHDOG`, es esta
limitación, no un defecto del diagnóstico. El control de válvulas no depende de este dato.

---

## Qué mira el técnico primero

1. ¿Aparece `TX` al conmutar la entrada? → **No**: problema de entrada o cableado.
2. ¿Aparece `ACK` después del `TX`? → **No**: problema de radio o de alimentación del nodo.
3. ¿Aparece `DROP` con la válvula afectada? → problema en el nodo o en la válvula.
4. ¿Aparece `BACK`? → la válvula lleva 5 intentos fallidos; está en reposo de una hora.
5. ¿Aparece `WATCHDOG` en el arranque? → reportar al equipo de firmware.

## Silenciar el diagnóstico

Para compilar sin trazas:

```bash
pio run -e megaatmega2560 -a "-DWIGA_LOG_LEVEL=0"
```

Ahorra unos 790 bytes de Flash y 104 de RAM, y **no cambia nada más** del comportamiento.
Los niveles son 1 = solo errores, 2 = + avisos, 3 = + información (por defecto).
