# Contrato — Mapa de instalación de finca

**Feature**: `001-migracion-platformio` | **Versión del contrato**: 1.0 | **Fecha**: 2026-08-20

Este es el **único** archivo que se modifica para poner en marcha una finca nueva (FR-023,
SC-004). Vive en `lib/wiga_config/installation_map.h` y no depende de nada.

Quien edita este archivo puede no ser quien escribió el firmware. Por eso el contrato exige
que todo error de mapeo se detecte **al compilar**, en el escritorio, y no en campo de
noche con una válvula abierta que debería estar cerrada.

---

## 1. Estructura de una entrada

```
struct ValveMapping {
  uint32_t nodeAddressLsb;   // 32 bits bajos de la dirección de 64 bits del nodo
  uint8_t  valveId;          // 0..7  — posición de la válvula dentro del nodo
  uint8_t  inputPin;         // pin del Mega conectado a la salida del tablero
};
```

Los 32 bits **altos** de la dirección son `0x0013A200` para toda la red XBee (prefijo del
fabricante Digi) y se declaran una sola vez en `timing_config.h`. No se repiten por válvula.

El **índice** de cada entrada en el arreglo es el número de válvula del sistema
(0 a 23), y determina su posición en el panel LCD: filas de ocho, en orden.

---

## 2. Forma del archivo

```
constexpr ValveMapping kMap[] = {          // en namespace detail
  // idx  dirección LSB   id  pin      bloque / finca
  /*  0 */ { 0x42878FDB,  0,  A0  },   // bloque 1
  /*  1 */ { 0x42878FDB,  1,  A1  },   // bloque 1
  ...
  /* 23 */ { 0x41C3F54F,  7,  A15 },   // bloque 8
};

constexpr uint8_t kValveCount = sizeof(kMap) / sizeof(kMap[0]);
```

**Regla MAP-01** (FR-024): `kValveCount` se **deriva** del arreglo. Está prohibido
declararlo como literal. El firmware base tenía `const int Entradas = 24` junto a tres
arreglos de 24 elementos: cuatro sitios que podían desincronizarse.

**Regla MAP-02**: un solo arreglo de estructuras, no arreglos paralelos. El firmware base
mantenía `DIR[]`, `IDS[]` y `PINES[]` por separado; desplazar un elemento en uno de ellos
hace que una válvula obedezca la orden de otra, sin error de compilación.

---

## 3. Validaciones en tiempo de compilación

Todas se expresan como `static_assert` sobre funciones `constexpr`. Un fallo detiene la
compilación con un mensaje que nombra el problema.

| Id | Validación | Mensaje | Requisito |
|---|---|---|---|
| MAP-V1 | `kValveCount > 0` | `mapa de instalacion vacio` | FR-024 |
| MAP-V2 | `kValveCount <= 96` | `demasiadas valvulas para el panel` | FR-019 |
| MAP-V3 | Ningún `inputPin` repetido | `pin de entrada duplicado en el mapa` | FR-025 |
| MAP-V4 | Ningún par (`nodeAddressLsb`, `valveId`) repetido | `par direccion/id duplicado en el mapa` | FR-025 |
| MAP-V5 | `valveId <= 7` en todas las entradas | `valveId fuera de rango (0..7)` | C-04 |
| MAP-V6 | `nodeAddressLsb != 0` en todas las entradas | `direccion de nodo sin asignar` | C-01 |

**Regla MAP-03**: estas comprobaciones NO se ejecutan en tiempo de arranque. Un error de
mapeo que llega a la placa ya es tarde: el objetivo es que el firmware defectuoso no
llegue a existir.

---

## 4. Mapa heredado de la instalación actual

Transcrito de `CABLE_VIRTUAL___LATCH_V1.ino` sin modificar. **Debe contrastarse contra el
plano de la finca antes de dar por buena la migración** (R-010, riesgo registrado en el plan).

| Idx | Dirección LSB | Id | Pin | Observación |
|---|---|---|---|---|
| 0 | `0x42878FDB` | 0 | A0 | |
| 1 | `0x42878FDB` | 1 | A1 | |
| 2 | `0x42878FDB` | 2 | A2 | |
| 3 | `0x42878FDB` | 3 | A3 | |
| 4 | `0x41C3F584` | 4 | A4 | |
| 5 | `0x41C3F584` | 5 | A5 | |
| 6 | `0x41C3F549` | 6 | A6 | |
| 7 | `0x41C3F549` | 7 | A7 | |
| 8 | `0x41C3F568` | 0 | A8 | |
| 9 | `0x41C3F568` | 1 | 27 | |
| 10 | `0x41C3F57A` | 2 | 5 | |
| 11 | `0x41C3F57A` | 3 | 8 | |
| 12 | `0x41C3F544` | 4 | 46 | |
| 13 | `0x41C3F544` | 5 | 44 | |
| 14 | `0x41C3F58B` | 6 | 11 | |
| 15 | `0x41C3F58B` | 7 | 12 | |
| 16 | `0x41C3F658` | 0 | 10 | |
| 17 | `0x41C3F658` | 1 | 45 | |
| 18 | `0x41C3F595` | 2 | 9 | ⚠ dirección repetida en 18-22 |
| 19 | `0x41C3F595` | 3 | A9 | ⚠ |
| 20 | `0x41C3F595` | 4 | A12 | ⚠ |
| 21 | `0x41C3F595` | 5 | A14 | ⚠ |
| 22 | `0x41C3F595` | 6 | A13 | ⚠ |
| 23 | `0x41C3F54F` | 7 | A15 | |

**Nota de verificación**: las seis entradas del nodo `0x41C3F595` usan identificadores 2 a
6, no 0 a 7 desde el principio. Es una combinación válida (pasa MAP-V4), pero es un patrón
distinto al de los demás nodos y merece confirmación con el plano. El resto de los nodos
agrupa sus válvulas en pares o cuartetos con identificadores consecutivos.

**Advertencia sobre los pines**: los pines `A0`–`A15` del Mega se usan como entradas
digitales. Es legítimo, pero implica que el controlador **no dispone de entradas analógicas
libres** en esta configuración. Cualquier funcionalidad futura que requiera medición
analógica (tensión de batería, por ejemplo) debe replantear el mapeo.

---

## 5. Procedimiento para configurar una finca nueva

1. Obtener del plano de la finca la lista de válvulas, con su nodo y su posición dentro del
   nodo.
2. Leer la dirección de 64 bits de cada nodo (parámetros `SH`/`SL` del XBee). Se usan
   únicamente los 32 bits bajos (`SL`).
3. Escribir una fila por válvula en `detail::kMap`, en el orden en que deben aparecer en
   el panel.
4. Compilar. Si alguna validación `MAP-V*` falla, corregir el mapa. **No** modificar las
   validaciones.
5. Ajustar en `timing_config.h` únicamente los tiempos que la instalación requiera. Los
   valores por defecto son los validados en campo.
6. Registrar el cambio en el repositorio: el mapa de cada finca es parte del historial del
   proyecto, no un archivo local.

**Regla MAP-04** (FR-023, SC-004): poner en marcha una finca nueva NO DEBE requerir
modificar ningún archivo de `lib/wiga_domain`, `lib/wiga_control`, `lib/wiga_ports`,
`lib/wiga_hal` ni `src/`. Si lo requiere, el diseño está mal y debe corregirse el diseño,
no el mapa.

---

## 6. Verificación del contrato

- Las reglas `MAP-V1` a `MAP-V6` se verifican **por el propio compilador**. Una prueba
  adicional en `test/test_domain/test_config` valida que las funciones `constexpr` de
  detección de duplicados devuelven el resultado correcto sobre mapas de ejemplo
  construidos a propósito con y sin colisiones.
- La regla `MAP-04` se verifica en la guía de validación: se cambia el mapa a una
  instalación de prueba con distinto número de válvulas y se comprueba que solo ese archivo
  aparece en el diff (ver [quickstart.md](../quickstart.md), escenario 5).
