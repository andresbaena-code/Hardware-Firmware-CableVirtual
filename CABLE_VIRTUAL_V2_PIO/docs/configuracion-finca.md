# Configurar una instalación

Poner en marcha una finca nueva se hace editando **un solo archivo**:
[`lib/wiga_config/installation_map.h`](../lib/wiga_config/installation_map.h).

Si hace falta tocar cualquier otra cosa, el diseño está mal y se corrige el diseño, no el
mapa.

---

## Qué es el mapa

Una fila por válvula. El **índice** de la fila es el número de válvula del sistema y
determina su posición en el panel LCD (tres filas de ocho, en orden).

```cpp
constexpr ValveMapping kMap[] = {
    //  idx    direccion LSB   id  pin
    /*  0 */ { 0x42878FDBUL,   0,  54 },   // A0
    /*  1 */ { 0x42878FDBUL,   1,  55 },   // A1
    ...
};
```

| Campo | Qué es | De dónde sale |
|---|---|---|
| `nodeAddressLsb` | 32 bits bajos de la dirección de 64 bits del nodo | parámetro `SL` del XBee |
| `valveId` | posición de la válvula dentro del nodo, 0 a 7 | plano de la finca |
| `inputPin` | pin del Mega conectado a la salida del tablero | plano de conexionado |

Los 32 bits **altos** de la dirección (`0x0013A200`, prefijo de Digi) son iguales para
toda la red y se declaran una sola vez en `timing_config.h`.

### Sobre los pines analógicos

Los pines `A0`–`A15` se escriben por su **número digital equivalente** (A0 = 54, …,
A15 = 69), con el nombre original en el comentario. Así el mapa es idéntico en la placa y
en las pruebas de host, donde los nombres `A0..A15` no existen.

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

## Procedimiento

1. **Obtener el plano.** Lista de válvulas, con su nodo y su posición dentro del nodo.
2. **Leer las direcciones.** Con XCTU o la consola AT, leer `SH`/`SL` de cada XBee. Solo
   se usa `SL`.
3. **Escribir una fila por válvula**, en el orden en que deben aparecer en el panel.
4. **Compilar.** `pio run -e megaatmega2560`. Si falla una validación, corregir el mapa.
   **No** modificar las validaciones.
5. **Ajustar tiempos** en `timing_config.h` solo si la instalación lo exige. Los valores
   por defecto son los validados en campo.
6. **Registrar el cambio en el repositorio.** El mapa de cada finca es parte del historial
   del proyecto, no un archivo local en el portátil de alguien.

---

## Lo que el compilador no deja pasar

Estas seis comprobaciones corren al compilar. Un mapa inválido **no llega a existir como
firmware**:

| Comprobación | Mensaje |
|---|---|
| Mapa vacío | `MAP-V1: mapa de instalacion vacio` |
| Demasiadas válvulas para el panel | `MAP-V2: demasiadas valvulas para el panel` |
| **Pin de entrada repetido** | `MAP-V3: pin de entrada duplicado en el mapa` |
| **Par (dirección, id) repetido** | `MAP-V4: par direccion/id duplicado en el mapa` |
| `valveId` fuera de 0..7 | `MAP-V5: valveId fuera de rango (0..7)` |
| Dirección sin asignar | `MAP-V6: direccion de nodo sin asignar` |

Las dos marcadas en negrita son las que importan. El firmware anterior mantenía tres
arreglos paralelos de 24 elementos editados a mano; desplazar un elemento en uno de ellos
no producía ningún error, producía **una válvula que obedece la orden de otra**, en campo,
de noche. Es el modo de fallo más caro del sistema y ahora es un error de compilación en
el escritorio.

El número de válvulas se **deriva** del tamaño del arreglo. No se declara por separado.

---

## Verificar el mapa antes de dar por buena una migración

Compilar no basta: el compilador detecta incoherencias internas, no que una fila apunte
al nodo equivocado. Antes de sustituir firmware en producción hay que contrastar el mapa
contra el plano, válvula por válvula, y ejecutar el escenario 7 del
[quickstart](../specs/001-migracion-platformio/quickstart.md): conmutar las 24 entradas
una por una y confirmar que se mueve la válvula correcta.

### Pendiente en el mapa heredado

Las filas **18 a 22** comparten el nodo `0x41C3F595` con los identificadores 2, 3, 4, 5 y
6. Es una combinación válida y pasa todas las comprobaciones, pero es un patrón distinto
al del resto de nodos, que agrupan sus válvulas en pares o cuartetos con identificadores
consecutivos desde 0.

**Esto no se ha verificado contra el plano.** Debe confirmarse antes de la puesta en
producción.

---

## Parámetros de tiempo

En [`lib/wiga_config/timing_config.h`](../lib/wiga_config/timing_config.h).

| Constante | Por defecto | Qué controla |
|---|---|---|
| `kLoopPeriodMs` | 15 ms | cadencia del lazo de control |
| `kDebounceMs` | 50 ms | estabilidad exigida a una entrada antes de actuar |
| `kLatchGuardMs` | 1 500 ms | separación mínima entre dos disparos |
| `kRetryIntervalMs` | 30 000 ms | espera de confirmación antes de reintentar |
| `kBackoffIntervalMs` | 3 600 000 ms | reposo tras agotar los reintentos |
| `kMaxAttempts` | 5 | reintentos rápidos por cambio de estado |
| `kLcdI2cAddress` | `0x27` | dirección del panel; algunos módulos usan `0x3F` |

Dos de estos valores están protegidos por comprobaciones en compilación: la ventana
antirebote debe cubrir al menos una vuelta de lazo, y el reposo prolongado debe ser mayor
que el reintento rápido. Sin esa segunda condición, la protección de batería no existiría.
