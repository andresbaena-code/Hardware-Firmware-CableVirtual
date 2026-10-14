# Data Model: Separación de comandos por UC511

**Spec**: [spec.md](spec.md) · **Research**: [research.md](research.md)

## 1. Configuración (en compilación)

### 1.1 Asignación de válvula — amplía `ValveMapping`

| Campo | Tipo | Valores | Requisito |
|---|---|---|---|
| `nodeAddressLsb`, `valveId` | sin cambios | históricos XBee | FR-120 (002) |
| `inputPin` | sin cambios | pin del tablero | FR-119 (002) |
| `actuator` | `uint8_t` | 1..`kActuatorCount` | FR-201 |
| `output` | `uint8_t` | 1 o 2 | FR-201 |

### 1.2 Equipo actuador (UC511) — nuevo `ActuatorConfig`

| Campo | Tipo | Significado | Requisito |
|---|---|---|---|
| `installed` | `bool` | el equipo existe en campo | FR-203 |

Se declara en `kActuators[]`; la posición en el arreglo + 1 es el número de equipo.
`kActuatorCount` se deriva del tamaño del arreglo.

### 1.3 Constantes de tiempo

| Constante | Valor | Unidad | Requisito |
|---|---|---|---|
| `kActuatorMinGapMs` | 5000 | ms | FR-214 |
| `kCommandSpacingMs` | 8000 | ms | FR-214 |

### 1.4 Validaciones

`MAP-V13..MAP-V17` (ver [research.md R-302](research.md#r-302--cómo-se-declara-el-mapa-de-equipos))
y `static_assert(kCommandSpacingMs >= kActuatorMinGapMs)`.

### 1.5 Mapa de la configuración entregada

| Índice | Válvula | Pin | UC511 | Salida |
|---|---|---|---|---|
| 0–1 | V01–V02 | A0, A1 | 1 | 1, 2 |
| 2–3 | V03–V04 | A2, A3 | 2 | 1, 2 |
| 4–5 | V05–V06 | A4, A5 | 3 | 1, 2 |
| 6–7 | V07–V08 | A6, A7 | 4 | 1, 2 |
| 2k, 2k+1 (k = 4..11) | V09–V24 | sin cambios | k+1 | 1, 2 |

`kActuators[]`: equipos 1–4 `installed = true`; 5–12 `installed = false`.

## 2. Estado en ejecución (`CommandSpacer`)

### 2.1 Por válvula

| Campo | Tipo | Significado |
|---|---|---|
| `published` | `ValveState` | lo que el banco de registros muestra |
| `pending` | `bool` | hay un cambio esperando turno |
| `pendingSinceMs` | `uint32_t` | instante en que se pidió el cambio |

El estado pendiente es siempre el contrario de `published`, así que no se guarda.

### 2.2 Por equipo

| Campo | Tipo | Significado |
|---|---|---|
| `lastChangeMs` | `uint32_t` | instante del último cambio publicado |
| `hasLastChange` | `bool` | `false` hasta el primer cambio o la siembra con alguna válvula en `1` |

## 3. Máquina de estados de una válvula

```text
                 deseado ≠ publicado
   ┌─────────┐ ─────────────────────▶ ┌───────────┐
   │ ESTABLE │                        │ PENDIENTE │
   └─────────┘ ◀───────────────────── └───────────┘
        ▲        deseado = publicado        │
        │        (CANCELADO, traza CANC)    │ equipo libre y es su turno
        │                                   ▼
        └──────────────────────────── PUBLICADO (traza TX)
                                       publicado := deseado
                                       equipo.lastChangeMs := ahora
```

- Si el equipo está libre en la vuelta en que se crea el pendiente, la transición a
  PUBLICADO ocurre en esa misma vuelta (FR-205). Si no, se emite la traza `WAIT`.
- **Equipo libre**: `!hasLastChange` o `elapsed(ahora, lastChangeMs, kCommandSpacingMs)`.
- **Turno dentro del equipo**: cierre antes que apertura; luego `pendingSinceMs` más
  antiguo; luego menor índice. Como máximo **un** cambio por equipo y por vuelta.

## 4. Deseado de una válvula

```text
deseado = (equipo instalado) ? panelRequest(nivel estable) : Closed
```

## 5. Invariantes

| Id | Invariante | Requisito |
|---|---|---|
| SP-I1 | Dos cambios publicados del mismo equipo están separados al menos `kCommandSpacingMs` (salvo la siembra) | FR-204 |
| SP-I2 | `published` solo cambia hacia el `deseado` vigente; nunca se publica un estado que el tablero ya no pide | FR-206, FR-209 |
| SP-I3 | Ningún pendiente espera `3 × kCommandSpacingMs` o más | caso borde "espera máxima" |
| SP-I4 | Una válvula de equipo no instalado tiene siempre `published = Closed` | FR-212 |
| SP-I5 | Tras `begin()`, `published` de cada válvula = deseado según las entradas | FR-211 |
