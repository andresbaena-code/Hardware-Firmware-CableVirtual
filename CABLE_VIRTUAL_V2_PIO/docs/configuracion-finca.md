# Configurar una instalación

Poner en marcha una finca nueva se hace editando **un solo archivo**:
[`lib/wiga_config/installation_map.h`](../lib/wiga_config/installation_map.h).

Si hace falta tocar cualquier otra cosa, el diseño está mal y se corrige el diseño, no el
mapa.

Para el mapa de registros que consume el UC100, la fuente de verdad es
[`contracts/register-map.md`](../specs/002-modbus-esclavo-rs485/contracts/register-map.md).

---

## Qué es el mapa

Una fila por válvula. El **índice** de la fila es el número de válvula del sistema, y
determina tres cosas a la vez:

- su posición en el panel LCD (tres filas de ocho, en orden),
- su **dirección de registro Modbus**,
- el canal que hay que configurar en el UC100.

```cpp
constexpr ValveMapping kMap[] = {
    //  idx    direccion LSB   id  pin        pin original
    /*  0 */ { 0x42878FDBUL,   0,  54 },   // A0
    /*  1 */ { 0x42878FDBUL,   1,  55 },   // A1
    ...
};
```

| Campo | Qué es | Estado |
|---|---|---|
| `inputPin` | Pin del Mega conectado a la salida del tablero | **vigente** |
| `actuator` | Número del UC511 que acciona la válvula (1..12) | **vigente** (spec 003) |
| `output` | Salida de ese UC511 (1 o 2) | **vigente** (spec 003) |
| `nodeAddressLsb` | Dirección XBee de la instalación original | **histórico** |
| `valveId` | Posición dentro del nodo XBee original | **histórico** |

### Los campos históricos ya no direccionan nada

`nodeAddressLsb` y `valveId` vienen de la instalación XBee original. **No participan en
ningún direccionamiento.** Se conservan solo para poder contrastar el mapa contra el plano
de la finca.

El direccionamiento vigente es Modbus, y el controlador es **un único esclavo** con un
único identificador, que vive en `timing_config.h` como `kModbusSlaveId`.

> Una versión anterior de este documento afirmaba que `nodeAddressLsb` se usaba como slave
> ID Modbus y que había que asignar IDs secuenciales por nodo. **Era incorrecto** y venía
> de suponer que el controlador era el maestro del bus. No lo es: es el esclavo, y el
> maestro es el UC100.

### La dirección de registro se deriva, no se escribe

```
registro Modbus de la válvula N  =  N
```

No hay columna que rellenar. Guardarla sería duplicar el índice y permitir que ambos se
desincronizasen.

### Sobre los pines analógicos

Los pines `A0`–`A15` se escriben por su **número digital equivalente** (A0 = 54, …,
A15 = 69), con el nombre original en el comentario. Así el mapa es idéntico en la placa y
en las pruebas de escritorio, donde los nombres `A0..A15` no existen.

| Nombre | Número | | Nombre | Número |
|---|---|---|---|---|
| A0 | 54 | | A8 | 62 |
| A1 | 55 | | A9 | 63 |
| A2 | 56 | | A10 | 64 |
| A3 | 57 | | A11 | 65 |
| A4 | 58 | | A12 | 66 |
| A5 | 59 | | A13 | 67 |
| A6 | 60 | | A14 | 68 |
| A7 | 61 | | A15 | 69 |

---

### Equipos UC511 y separación de comandos (spec 003)

Un UC511 acciona dos válvulas y pierde sin aviso el segundo de dos comandos que le lleguen
demasiado juntos (Medido en banco el 2026-10-14: tras una apertura, un cierre a 8 s se pierde y a 15 s llega; entre cierre y apertura bastan 5 s.) El firmware lo evita: nunca publica dos cambios de válvulas
del **mismo** UC511 con menos de 15 s (`kCommandSpacingMs` en `timing_config.h`). Para eso
necesita saber qué válvulas comparten equipo, y lo saca de `actuator` y `output`.

Los equipos se declaran aparte, en `kActuators`, con una marca de instalado. Su posición
en el arreglo más uno es el número de equipo:

```cpp
constexpr ActuatorConfig kActuators[] = {
    /* #01 */ { true  },
    /* #02 */ { true  },
    ...
    /* #05 */ { false },   // previsto, aún no instalado
    ...
};
```

Una válvula de un UC511 **no instalado** publica siempre `0`, pida lo que pida el
tablero; el LCD la muestra con `-` y la consola avisa `NOINST`. Así, si al crecer alguien
olvida marcar un equipo nuevo, la válvula no abre y se nota en la puesta en marcha, en vez
de funcionar sin separación de comandos.

**El mapa tiene que coincidir con el cableado real.** Si V01 y V02 no están de verdad en
las salidas 1 y 2 del UC511 #01, la separación protege al equipo equivocado.

## Poner en marcha una finca nueva

1. **Obtener el plano.** Lista de válvulas con su posición en el tablero.
2. **Rellenar el mapa.** Una fila por válvula, con su pin de entrada, su UC511 y su salida.
   El orden de las filas determina el número de válvula, su registro y su posición en el
   panel. Marcar en `kActuators` qué UC511 están instalados.
3. **Compilar.** Si el mapa es incoherente, **no compila**. Ver la tabla de abajo.
4. **Configurar el UC100** con un canal por válvula, según
   [`register-map.md`](../specs/002-modbus-esclavo-rs485/contracts/register-map.md) §5.
5. **Verificar en banco** antes de ir a campo, siguiendo
   [puesta-en-marcha.md](puesta-en-marcha.md).

---

## El mapa inválido no compila

Toda inconsistencia se detecta **al compilar**, no en campo.

| Error en el mapa | Mensaje del compilador |
|---|---|
| Mapa vacío | `MAP-V1: mapa de instalacion vacio` |
| Demasiadas válvulas | `MAP-V2: demasiadas valvulas para el panel` |
| Pin de entrada duplicado | `MAP-V3: pin de entrada duplicado en el mapa` |
| Par dirección/id duplicado | `MAP-V4: par direccion/id duplicado en el mapa` |
| `valveId` fuera de rango | `MAP-V5: valveId fuera de rango (0..7)` |
| Dirección sin asignar | `MAP-V6: direccion de nodo sin asignar` |
| **Un pin pisa el DE/RE del RS485** | `MAP-V9: un pin de entrada pisa el pin DE/RE del transceptor RS485` |
| Válvula apunta a un UC511 que no existe | `MAP-V13: una valvula apunta a un UC511 que no existe en kActuators` |
| Salida distinta de 1 o 2 | `MAP-V14: salida de UC511 invalida (solo 1 o 2)` |
| Dos válvulas en la misma salida | `MAP-V15: dos valvulas en la misma salida del mismo UC511` |
| Tres válvulas en un UC511 | `MAP-V16: un UC511 tiene mas de 2 valvulas` |
| UC511 declarado sin válvulas | `MAP-V17: un UC511 declarado no tiene ninguna valvula` |
| Separación menor que la que tolera el UC511 | `TC-01: la separacion entre comandos no puede ser menor que la que tolera el UC511...` |

**MAP-V9 es nueva** y cierra un hueco real: si un `inputPin` coincidiera con el pin de
control de dirección del transceptor (`kRs485DeRePin`, el pin 6), leer un interruptor
conmutaría el bus a transmisión y el controlador quedaría mudo de forma intermitente,
imposible de diagnosticar en campo.

---

## Parámetros del bus

En [`lib/wiga_config/timing_config.h`](../lib/wiga_config/timing_config.h):

| Constante | Valor | Qué es |
|---|---|---|
| `kModbusSlaveId` | `1` | Identificador propio en el bus. Debe coincidir con el que el UC100 tiene configurado en cada canal |
| `kRadioBaudRate` | `9600` | Debe coincidir con el UC100 |
| `kModbusSilenceMs` | `200` | Plazo para descartar una trama parcial perdida (el delimitador real de trama es la longitud esperada de 8 bytes) |
| `kBusSilentThresholdMs` | `30000` | Sin sondeo durante este plazo, el panel avisa |
| `kDebounceMs` | `50` | Ventana de antirrebote de los interruptores |
| `kRs485DeRePin` | `6` | Pin de dirección del MAX485 (en `installation_map.h`) |

---

## Configurar el UC100 paso a paso

El UC100 es **maestro Modbus** y **controlador D2D** (equipo comprado, no se programaba con
nuestro firmware): se configura desde el software **ToolBox** de Milesight. Esta sección es
la guía para que lea correctamente el esclavo Modbus que corre en el ATmega2560.

> **Topología de referencia:**
> `[ ATmega2560 (esclavo) ] ←──── RS485 9600 8N1 ────→ [ UC100 (maestro) ]`
> El UC100 pregunta y el Mega contesta. El Mega **nunca** inicia una transacción.

### Paso 1 — Poner el puerto serie en «Modbus Channels»

En el ToolBox, seleccionar el UC100 y a la opción de operación del puerto serie asignarle el
modo **Modbus Channels** (no GPS, no Modbus-Slave, no serial dedicado). Esto convierte el
RS485 del UC100 en un **maestro Modbus** que va a sondear el esclavo.

### Paso 2 — Ajustar el enlace serie

Los parámetros serie del puerto Modbus del UC100 deben coincidir **exactamente** con los
del esclavo:

| Parámetro | Valor |
|---|---|
| Baud rate | **9600** |
| Data bits | **8** |
| Paridad | **ninguna (none)** |
| Stop bits | **1** |

Es el **9600 8N1** que declaramos en `kRadioBaudRate` de `timing_config.h`. Si no coincide,
el UC100 manda tramas con ruido que nuestro esclavo descarta **en silencio**, y parece un
"no responde" aunque el cable esté bien.

### Paso 3 — Crear UN canal Modbus por válvula

En la vista *Modbus Channels* crear un canal por cada válvula. **La dirección del canal es
el índice de la válvula** (`valveRegister(index) == index`): no hay cálculo de mapa, el
número de válvula ES la dirección de registro.

| Campo del canal | Valor |
|---|---|
| Slave ID | **1** (coincide con `kModbusSlaveId`) |
| Address | **0 a 23** (una por válvula, en orden) |
| Quantity | **1** |
| Type | Coil `00`, Discrete `01`, Hold16 `0E` o Input16 `10` — **cualquier lectura sirve** |
| Execution Interval | 100 ms (podemos aguantar hasta 10 ms) |
| Max Response Time | 1000 ms |
| Max Retry Times | 3 |

> **Por qué un canal por válvula y no uno empaquetado:** las reglas IF-THEN del UC100
> evalúan el **valor** de un canal contra un umbral. Si empaquetáramos varias válvulas en un
> registro, no se podrían condicionar por separado. Por eso la arquitectura publica **un
> registro por válvula** (FR-109).

### Paso 4 — Crear las reglas IF-THEN (lo que acciona la válvula)

Después de leer el valor, el UC100 usa **reglas IF-THEN** para disparar el comando de radio
D2D hacia el **UC511** que acciona la válvula (el UC501 no sirve de agente D2D). Por cada
válvula se crean **dos reglas**, con una sola acción cada una:

```
IF  canal(válvula N) Above 0.5  →  D2D código N01  →  ABRIR
IF  canal(válvula N) Below 0.5  →  D2D código N00  →  CERRAR
```

Los códigos, canales y reglas exactos del piloto están en
[piloto-8-valvulas-d2d.md](piloto-8-valvulas-d2d.md).

### Paso 5 — `Lockout` y `Delay Time`

- **`Delay Time` = 0.** No sirve para separar comandos: medido en el piloto, si la
  condición desaparece durante la espera, la acción se ejecuta igual. La separación entre
  comandos al mismo UC511 la hace el firmware (spec 003).
- **`Lockout` sin marcar.** Si con la condición sostenida el UC100 vuelve a emitir la
  orden (la re-aserción de **FR-115**) está pendiente de medir en el piloto (prueba B4).
  Mientras tanto, la red de seguridad ante una orden perdida es el `Duration` del UC511,
  por encima del pulso de riego.

### Paso 6 — Verificar con el botón Fetch

En el ToolBox, cada canal tiene un botón **Fetch**: hace una lectura puntual inmediata.

1. Con los 24 interruptores en reposo: `Fetch` debe devolver **0** en todos los canales.
2. Subir el interruptor de la válvula **3**: `Fetch` del canal 3 debe pasar a **1** (y a `0`
   al bajarlo).

Si `Fetch` devuelve el valor correcto de una válvula, la configuración de **lectura** está
bien. Si `Fetch` falla o no responde, revisar en este orden: Slave ID del canal (`1`),
parámetros serie (9600 8N1), y que el cable RS485 tenga A/B y masa común correctos.

### Paso 7 — Probar el escenario de campo completo

Una vez leído bien por `Fetch`, probar de extremo a extremo con el panel del UC501:

| Prueba | Resultado esperado |
|---|---|
| Subir un interruptor | La válvula del campo abre en menos de un segundo tras el siguiente sondeo |
| Bajar el interruptor | La válvula cierra |
| Dejar una válvula regando más que el temporizador del UC501 | El riego continúa: la re-emisión reinicia el temporizador del actuador |
| Apagar el UC100 con una válvula regando | El actuador cierra solo al vencer su temporizador (**red de seguridad** contra inundación) |

---

## Verificación pendiente del mapa heredado

Un mapa correcto en su forma puede seguir estando **mal en la realidad**: si dos filas se
intercambian, el sistema riega el sector equivocado y compila igual.

> **Pendiente (tarea T055 de la especificación 001).** Las filas **18 a 22** comparten el
> nodo histórico `0x41C3F595` con los identificadores 2, 3, 4, 5 y 6. Es un patrón distinto
> al del resto de nodos, que agrupan sus válvulas en pares o cuartetos.
>
> Hay que contrastarlo contra el plano de la finca **antes** de sustituir firmware en
> producción.
