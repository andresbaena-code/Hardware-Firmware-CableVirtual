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

## Poner en marcha una finca nueva

1. **Obtener el plano.** Lista de válvulas con su posición en el tablero.
2. **Rellenar el mapa.** Una fila por válvula, con su pin de entrada. El orden de las filas
   determina el número de válvula, su registro y su posición en el panel.
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

**MAP-V9 es nueva** y cierra un hueco real: si un `inputPin` coincidiera con el pin de
control de dirección del transceptor (`kRs485DeRePin`, el pin 2), leer un interruptor
conmutaría el bus a transmisión y el controlador quedaría mudo de forma intermitente,
imposible de diagnosticar en campo.

---

## Parámetros del bus

En [`lib/wiga_config/timing_config.h`](../lib/wiga_config/timing_config.h):

| Constante | Valor | Qué es |
|---|---|---|
| `kModbusSlaveId` | `1` | Identificador propio en el bus. Debe coincidir con el que el UC100 tiene configurado en cada canal |
| `kRadioBaudRate` | `9600` | Debe coincidir con el UC100 |
| `kModbusSilenceMs` | `4` | Silencio reglamentario entre tramas a 9600 8N1 |
| `kBusSilentThresholdMs` | `30000` | Sin sondeo durante este plazo, el panel avisa |
| `kDebounceMs` | `50` | Ventana de antirrebote de los interruptores |
| `kRs485DeRePin` | `2` | Pin de dirección del MAX485 (en `installation_map.h`) |

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
