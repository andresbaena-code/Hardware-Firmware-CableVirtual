# Fase 1 — Modelo de datos: Cable Virtual

**Feature**: `001-migracion-platformio` | **Fecha**: 2026-08-20
**Entrada**: [spec.md](./spec.md) · [research.md](./research.md)

Este documento define las entidades del dominio, sus invariantes y la máquina de estados
por válvula. Todo lo aquí descrito vive en `lib/wiga_domain` y `lib/wiga_control`, sin
dependencia de hardware.

---

## 1. Tipos fundamentales

### 1.1 `SignalLevel` — nivel eléctrico

```
enum class SignalLevel : uint8_t { Low = 0, High = 1 };
```

Representa lo que hay físicamente en un pin, o el byte de nivel dentro del payload del
protocolo. **Nunca** se usa directamente en la lógica de control ni se envía al panel.

### 1.2 `ValveState` — estado físico de la válvula

```
enum class ValveState : uint8_t { Closed = 0, Open = 1 };
```

Representa lo que hace la válvula en campo. Es el único tipo que la lógica de control, el
panel y las trazas manipulan.

### 1.3 Conversión — el único punto de inversión (FR-021, FR-021b, R-004)

La etapa de potencia de los nodos remotos y las entradas del tablero operan con **lógica
inversa por transistor**:

| `SignalLevel` | `ValveState` |
|---|---|
| `High` (1) | `Closed` |
| `Low` (0) | `Open` |

```
constexpr ValveState toValveState(SignalLevel level);
constexpr SignalLevel toSignalLevel(ValveState state);
```

**Invariante V-01**: `toSignalLevel(toValveState(x)) == x` para todo `x`, y
`toValveState(toSignalLevel(y)) == y` para todo `y`.

**Invariante V-02**: ninguna función fuera de este par convierte entre ambos tipos. Los
`enum class` impiden la conversión implícita, de modo que el compilador hace cumplir la
regla. *Esta inversión es una característica documentada del hardware, no un defecto: no
se "corrige".*

### 1.4 `ValveIndex`

```
using ValveIndex = uint8_t;   // 0 .. kValveCount-1
```

**Invariante V-03**: todo índice usado fuera del dominio se valida contra `kValveCount`
antes de indexar. Los índices provenientes de la radio (respuestas de nodos) se resuelven
mediante búsqueda en el mapa, nunca por indexación directa.

---

## 2. Entidades de configuración

### 2.1 `ValveMapping` — una entrada del mapa de instalación

| Campo | Tipo | Significado | Validación |
|---|---|---|---|
| `nodeAddressLsb` | `uint32_t` | 32 bits bajos de la dirección de 64 bits del nodo | ≠ 0 |
| `valveId` | `uint8_t` | posición de la válvula dentro del nodo | 0 ≤ id ≤ 7 |
| `inputPin` | `uint8_t` | pin del Mega conectado a la salida del tablero | único en todo el mapa |

Los 32 bits altos de la dirección son constantes para toda la red XBee (`0x0013A200`,
prefijo del fabricante) y viven en `timing_config.h`, no repetidos por válvula.

**Invariante C-01** (`static_assert`, FR-025): no hay dos entradas con el mismo `inputPin`.

**Invariante C-02** (`static_assert`, FR-025): no hay dos entradas con el mismo par
(`nodeAddressLsb`, `valveId`).

**Invariante C-03** (`static_assert`, FR-024): `kValveCount` se deriva del tamaño del
arreglo; no se declara por separado.

**Invariante C-04** (`static_assert`): `valveId ≤ 7` en todas las entradas.

### 2.2 `TimingConfig` — parámetros temporales

| Constante | Tipo | Valor por defecto | Origen | Requisito |
|---|---|---|---|---|
| `kLoopPeriodMs` | `uint16_t` | 15 | heredado | FR-001 |
| `kDebounceMs` | `uint16_t` | 50 | nuevo (R-006) | FR-002 |
| `kLatchGuardMs` | `uint16_t` | 1 500 | heredado | FR-004 |
| `kRetryIntervalMs` | `uint32_t` | 30 000 | heredado | FR-006 |
| `kBackoffIntervalMs` | `uint32_t` | 3 600 000 | heredado | FR-007 |
| `kMaxAttempts` | `uint8_t` | 5 | heredado | FR-006 |
| `kWatchdogTimeout` | — | 8 s | heredado | FR-012 |
| `kMaxFramesPerPoll` | `uint8_t` | 4 | nuevo (R-002) | FR-017 |
| `kDisplayCellsPerTick`| `uint8_t` | 1 | nuevo (R-007) | FR-022 |

**Invariante C-05**: `kDebounceMs ≥ kLoopPeriodMs`, para que el filtro tenga al menos una
muestra de ventana.

**Invariante C-06**: `kRetryIntervalMs < kBackoffIntervalMs`. El reposo prolongado debe
ser estrictamente mayor que el reintento corto, o la protección de batería no existe.

---

## 3. Entidad central: `ValveRecord`

Estado de una válvula. Existe un arreglo estático de `kValveCount` registros; no hay
asignación dinámica.

| Campo | Tipo | Significado |
|---|---|---|
| `requested` | `ValveState` | lo que pide el tablero, ya filtrado por antirebote |
| `confirmed` | `ValveState` | último estado confirmado por el nodo remoto |
| `phase` | `ValvePhase` | fase de la máquina de estados: `Verified`, `Pending`, `Awaiting` o `Backoff` |
| `attempts` | `uint8_t` | intentos consumidos en el ciclo de reintento actual |
| `nextAttemptAtMs` | `uint32_t` | marca de tiempo del próximo intento permitido |
| `radioFailures` | `uint16_t` | contador acumulado de fallos, para diagnóstico (FR-028) |

Tamaño real medido: 10 B por válvula, 240 B para 24 (R-011).

> **Nota de implementación (2026-08-20).** El diseño inicial preveía dos banderas
> separadas, `verified` y `waiting`. Al implementar la máquina de estados quedó claro que
> ambas son proyecciones de una misma variable de fase, y que mantenerlas por separado
> permite estados imposibles (`verified && waiting`). Se sustituyen por un único campo
> `phase` con las cuatro fases del diagrama de la sección 4, y `verified()` pasa a ser una
> consulta derivada (`phase == Verified`). El tamaño del registro no cambia y todas las
> invariantes se conservan.

**Invariante R-01** (FR-005): `phase` **solo** puede pasar a `Verified` desde `Awaiting` al
procesar una confirmación válida del nodo cuyo estado coincide con `requested`. La única
función que lo hace es `RetryPolicy::onConfirmed()`. *(La excepción es la siembra de
arranque T1, que no transmite nada.)*

**Invariante R-02** (FR-009): los registros son independientes. Ninguna operación sobre el
registro *i* modifica el registro *j*.

**Invariante R-03** (FR-006, FR-007): `attempts ≤ kMaxAttempts`. Al alcanzar el límite, el
siguiente intento se programa a `kBackoffIntervalMs` en lugar de `kRetryIntervalMs`, y
`attempts` no sigue creciendo.

**Invariante R-04** (FR-013): toda comparación temporal usa
`(uint32_t)(now - nextAttemptAtMs)` en aritmética modular sin signo. Prohibida la
comparación directa de marcas absolutas.

---

## 4. Máquina de estados por válvula

```
                    ┌──────────────────────────────────────────┐
                    │                                          │
                    ▼                                          │
             ┌─────────────┐                                   │
   arranque  │  VERIFIED   │  requested == confirmed           │
   (FR-010) ─▶  (en régimen)│  phase = Verified                 │
             └──────┬──────┘                                   │
                    │                                          │
      la entrada estable cambia (FR-002, FR-008)               │
      → requested = nuevo; phase = Pending;                    │
        attempts = 0                                           │
                    │                                          │
                    ▼                                          │
             ┌─────────────┐                                   │
             │  PENDING    │  espera turno de disparo          │
             │             │  (FR-004: uno a la vez)           │
             └──────┬──────┘                                   │
                    │  el planificador concede el turno        │
                    │  → transmite orden; attempts++;          │
                    │    nextAttemptAtMs = now + retry         │
                    ▼                                          │
             ┌─────────────┐   confirmación válida y coincide  │
             │  AWAITING   │───────────────────────────────────┘
             │ (esperando  │   (FR-005) → phase = Verified;
             │ confirmación)│              attempts = 0
             └──────┬──────┘
                    │  vence la espera sin confirmar
                    │
        ┌───────────┴────────────┐
        │                        │
  attempts < kMaxAttempts   attempts == kMaxAttempts
        │                        │
        ▼                        ▼
   vuelve a PENDING       ┌─────────────┐
   (reintento corto)      │  BACKOFF    │  radioFailures++
                          │ (reposo 60m)│  nextAttemptAtMs = now + backoff
                          └──────┬──────┘  (FR-007)
                                 │
                    ┌────────────┴─────────────┐
                    │                          │
          vence el reposo          la entrada cambia (FR-008)
                    │                          │
                    ▼                          ▼
              vuelve a PENDING          vuelve a PENDING
              (attempts se mantiene     (attempts = 0,
               en el límite)             reposo abandonado)
```

### Transiciones — tabla de verdad

| # | Estado origen | Evento | Estado destino | Efectos | Requisito |
|---|---|---|---|---|---|
| T1 | *(arranque)* | siembra desde entradas físicas | `VERIFIED` | `requested = confirmed = lectura`; **no transmite** | FR-010 |
| T2 | `VERIFIED` | entrada estable distinta | `PENDING` | `requested` nuevo; `attempts=0` | FR-002, FR-003 |
| T3 | `PENDING` | planificador concede turno | `AWAITING` | transmite; `attempts++`; espera `kRetryIntervalMs`; toma la ventana de exclusión | FR-004, FR-006 |
| T4 | `AWAITING` | confirmación válida y coincidente | `VERIFIED` | `confirmed = requested`; `attempts=0` | FR-005 |
| T5 | `AWAITING` | confirmación válida pero **no** coincidente | `AWAITING` | no confirma; mantiene la secuencia | Caso borde |
| T6 | `AWAITING` | vence espera, `attempts < kMaxAttempts` | `PENDING` | `radioFailures++` | FR-006 |
| T7 | `AWAITING` | vence espera, `attempts == kMaxAttempts` | `BACKOFF` | `radioFailures++`; espera `kBackoffIntervalMs` | FR-007 |
| T8 | `BACKOFF` | vence el reposo | `PENDING` | mantiene `attempts` en el límite | FR-007 |
| T9 | `BACKOFF` | entrada estable cambia | `PENDING` | `requested` nuevo; `attempts=0`; abandona el reposo | FR-008 |
| T10 | *(cualquiera)* | confirmación de válvula no mapeada | *(sin cambio)* | descarta y registra | FR-016 |
| T11 | `PENDING` | entrada estable vuelve al valor confirmado | `VERIFIED` | cancela la orden pendiente sin transmitir | FR-003 |

**Nota sobre T11**: si el operador conmuta y vuelve a conmutar antes de que la válvula
obtenga turno de disparo, no se transmite nada. Es un ahorro directo de batería que el
firmware base no tenía, y una consecuencia natural de FR-003 ("transmite solo cuando el
estado solicitado difiere del confirmado").

---

## 5. Entidades del planificador y del filtro

### 5.1 `LatchScheduler` — exclusión mutua de disparo (FR-004)

| Campo | Tipo | Significado |
|---|---|---|
| `guardUntilMs` | `uint32_t` | instante hasta el que no se permite otro disparo |
| `active` | `bool` | hay una ventana de exclusión vigente |
| `lastServed` | `ValveIndex` | última válvula atendida, para reparto equitativo |

**Invariante S-01** (FR-004, Principio II): `grant()` devuelve como máximo **una** válvula
por vuelta, y solo si `!active || (uint32_t)(now - guardUntilMs) < 0x80000000` evaluado con
aritmética modular segura.

**Invariante S-02** (equidad): el barrido de candidatas comienza en `lastServed + 1` y da
la vuelta de forma circular. Sin esto, una válvula de índice bajo que falla de forma
persistente monopoliza los turnos y las de índice alto nunca se atienden — defecto latente
en el firmware base, que recorría siempre desde 0 y hacía `break` en la primera candidata.

### 5.2 `Debouncer` — filtro de estabilidad (FR-002)

Un filtro por entrada.

| Campo | Tipo | Significado |
|---|---|---|
| `stable` | `SignalLevel` | último nivel considerado estable |
| `candidate` | `SignalLevel` | nivel que se está evaluando |
| `sinceMs` | `uint32_t` | instante en que apareció `candidate` |

**Invariante D-01**: `stable` solo cambia si `candidate` se mantuvo idéntico durante
`kDebounceMs` completos. Cualquier lectura distinta reinicia `sinceMs`.

**Invariante D-02**: el filtro opera sobre `SignalLevel`; la conversión a `ValveState`
ocurre después, en el supervisor, mediante la función de §1.3.

---

## 6. Entidades del protocolo de aplicación

Detalle completo del formato en [contracts/application-protocol.md](./contracts/application-protocol.md).

### 6.1 `ValveCommand` — orden saliente

| Campo | Tipo |
|---|---|
| `nodeAddressLsb` | `uint32_t` |
| `valveId` | `uint8_t` |
| `level` | `SignalLevel` |

Se serializa en 5 bytes con dígito verificador (FR-015, R-012).

### 6.2 `ValveReport` — confirmación entrante

| Campo | Tipo |
|---|---|
| `nodeAddressLsb` | `uint32_t` |
| `valveId` | `uint8_t` |
| `level` | `SignalLevel` |

**Invariante P-01** (FR-016): la decodificación valida longitud mínima, encabezado
(`0xAA`), código de operación (`0x14`) y, cuando la trama trae 5 bytes, el verificador.
Una trama que no pase todas las comprobaciones se descarta **antes** de leer sus campos.

**Invariante P-02** (FR-016): nunca se indexa más allá de la longitud reportada por la
trama recibida. El firmware base leía `data[0..3]` sin comprobar la longitud — con una
trama corta, lectura fuera de límites.

---

## 7. Entidad de presentación

### 7.1 `DisplayBuffer` — buffer de sombra del panel (R-007)

| Campo | Tipo | Significado |
|---|---|---|
| `cells[4][20]` | `char` | contenido deseado del panel |
| `dirty[10]` | `uint8_t` | 80 bits: una marca por celda pendiente de escribir |
| `cursor` | `uint8_t` | barrido circular de la próxima celda sucia a escribir |

**Invariante B-01** (FR-022, Principio I): se escriben como máximo
`kDisplayCellsPerTick` celdas por vuelta de lazo.

**Invariante B-02** (FR-022): ningún fallo de escritura del panel altera el estado de
control ni impide el avance del lazo.

**Representación por válvula** (FR-019, FR-020):

| Condición | Símbolo | Requisito |
|---|---|---|
| `VERIFIED` y `confirmed == Closed` | `0` | FR-020, FR-021 |
| `VERIFIED` y `confirmed == Open` | `1` | FR-020, FR-021 |
| `PENDING` o `AWAITING` | `?` | FR-020 |
| `BACKOFF` | `!` | FR-020 |

El símbolo se deriva de `ValveState` (estado **físico**), nunca de `SignalLevel`. Las tres
primeras filas del panel muestran ocho válvulas cada una; la cuarta, la identificación del
sistema y su versión.

---

## 8. Trazabilidad requisito → entidad → prueba

| Requisito | Entidad / regla | Prueba en `native` |
|---|---|---|
| FR-002 | `Debouncer`, D-01 | `test_control/test_debouncer` |
| FR-003 | Transiciones T2, T11 | `test_integration/test_supervisor` |
| FR-004 | `LatchScheduler`, S-01, S-02 | `test_control/test_scheduler` |
| FR-005 | Invariante R-01, transición T4 | `test_integration/test_supervisor` |
| FR-006 | R-03, transiciones T3, T6 | `test_control/test_retry_policy` |
| FR-007 | R-03, transición T7 | `test_control/test_retry_policy` |
| FR-008 | Transición T9 | `test_control/test_retry_policy` |
| FR-009 | Invariante R-02 | `test_integration/test_supervisor` |
| FR-010 | Transición T1 | `test_integration/test_supervisor` |
| FR-013 | Invariante R-04 | `test_control/test_time_wraparound` |
| FR-015 | `ValveCommand`, §6.1 | `test_domain/test_app_protocol` |
| FR-016 | P-01, P-02 | `test_domain/test_app_protocol` |
| FR-018 | Drenaje del radio (R-002, R-003) | `test_integration/test_supervisor` |
| FR-021 | V-01, V-02, §7.1 | `test_domain/test_valve_types` |
| FR-024 | C-03 | `static_assert` en compilación |
| FR-025 | C-01, C-02 | `static_assert` + `test_domain/test_config` |
