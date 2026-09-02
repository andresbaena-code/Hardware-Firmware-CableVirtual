# Fase 1 — Modelo de datos

**Feature**: 002-modbus-esclavo-rs485 | **Fecha**: 2026-08-27

---

## 1. Panorama

El modelo se simplifica de forma drástica respecto a la especificación 001. Al no existir
confirmación del campo, desaparece toda la maquinaria de espera y reintento, y con ella
la mayor parte del estado por válvula.

```
                 ┌──────────────────────────────────────────┐
   24 pines  ──▶ │  Debouncer[24]   (wiga_control)          │
   del tablero   └────────────────┬─────────────────────────┘
                                  │ SignalLevel estable
                                  ▼
                 ┌──────────────────────────────────────────┐
                 │  toValveState()  (wiga_domain)           │  ← ÚNICO punto
                 │  la inversión por transistor vive aquí   │    de conversión
                 └────────────────┬─────────────────────────┘
                                  │ ValveState
                                  ▼
                 ┌──────────────────────────────────────────┐
                 │  RegisterFile    (wiga_modbus, puro)     │  ← la única
                 │  32 registros de 16 bits                 │    fuente de verdad
                 └───────┬──────────────────────┬───────────┘
                         │ lectura              │ lectura
                         ▼                      ▼
            ┌────────────────────┐   ┌────────────────────────┐
            │  ModbusSlave       │   │  Panel LCD             │
            │  (wiga_modbus)     │   │  (wiga_hal)            │
            └────────────────────┘   └────────────────────────┘
```

**Lo que desaparece del modelo 001**: `ValvePhase` con sus cuatro fases, `attempts`,
`nextAttemptAtMs`, `radioFailures`, `RetryParams`, el estado del planificador de disparo
y la estructura `ValveReport`. Todo ello existía para gestionar confirmaciones.

---

## 2. Entidades

### 2.1 `RegisterFile` *(nueva, `lib/wiga_modbus`)*

El banco de registros que el controlador publica. Es la **única fuente de verdad** del
estado pedido: la escribe la capa de control y la leen el protocolo y el panel.

| Campo | Tipo | Descripción |
|---|---|---|
| `values_[kRegisterCount]` | `uint16_t` | Contenido de los registros |

| Operación | Contrato |
|---|---|
| `set(index, value)` | Escribe. Índice fuera de rango: no hace nada, nunca desborda |
| `get(index)` | Lee. Índice fuera de rango: devuelve `0`, nunca desborda |
| `coilBit(index)` | Devuelve `true` si el registro de válvula `index` vale distinto de cero |
| `count()` | `kRegisterCount`, derivado de la configuración |

**Reglas de validación**

- `kRegisterCount` se **deriva** del número de válvulas más el bloque de diagnóstico.
  Nunca se escribe dos veces (Principio V).
- Todo acceso está acotado. Es la lección directa del defecto verificado en el que un
  campo de longitud de `251` hacía que la comprobación diera cero y se leyeran 254 bytes
  de un búfer de 32.
- Ninguna interrupción escribe el banco: el control y el protocolo se ejecutan
  cooperativamente en el mismo hilo, así que no hay lectura rota posible.

### 2.2 `ValveMapping` *(modificada, `lib/wiga_config`)*

| Campo | Tipo | Estado | Descripción |
|---|---|---|---|
| `inputPin` | `uint8_t` | **conservado** | Pin del Mega conectado a la salida del tablero |
| `registerAddress` | `uint16_t` | **NUEVO** | Dirección del registro Modbus de esta válvula |
| `nodeAddressLsb` | `uint32_t` | **histórico** | Dirección XBee de la instalación original. No participa en ningún direccionamiento |
| `valveId` | `uint8_t` | **histórico** | Posición dentro del nodo XBee original. No participa |

**Validación en compilación** (amplía MAP-V1..MAP-V6):

| Regla | Comprobación |
|---|---|
| MAP-V3 | Sin pines de entrada duplicados *(se conserva)* |
| **MAP-V7** | Sin direcciones de registro duplicadas |
| **MAP-V8** | Toda dirección de registro dentro del bloque de válvulas |
| **MAP-V9** | El pin DE/RE del transceptor no coincide con ningún pin de entrada |

MAP-V9 cierra un hueco real: hoy nada impide que un `inputPin` del mapa pise el pin de
control de dirección del bus.

**Decisión superada**: la columna `slaveId` por nodo, acordada cuando se creía que el
controlador era maestro de diez nodos. El bus tiene **un solo esclavo**, que es el propio
controlador, con un único identificador que vive en `timing_config.h`.

### 2.3 Estado por válvula en el supervisor

Se reduce a lo imprescindible:

| Elemento | Tipo | Bytes ×24 | Descripción |
|---|---|---:|---|
| `debouncers_` | `Debouncer` | 168 | Antirrebote, **sin cambios** respecto a 001 |
| `shownSymbol_` | `char` | 24 | Último símbolo escrito al panel, para no repetir escrituras |

`ValveRecord` **desaparece como estructura**. El estado pedido de una válvula es
exactamente el valor de su registro; mantener una copia paralela sería duplicar la fuente
de verdad, justo lo que el Principio V prohíbe.

### 2.4 `ModbusSlave` *(nueva, `lib/wiga_modbus`)*

Máquina de estados de recepción y composición de respuesta. **Sin entrada/salida propia**:
recibe bytes, devuelve bytes. Esa ausencia de E/S es lo que la hace comprobable en
`env:native`.

| Campo | Tipo | Descripción |
|---|---|---|
| `slaveId_` | `uint8_t` | Identificador propio, 1..247 |
| `rx_[kRxCapacity]` | `uint8_t` | Petición en curso |
| `rxLen_` | `uint8_t` | Bytes acumulados |
| `lastByteAtMs_` | `uint32_t` | Marca del último byte, para el silencio entre tramas |
| `overflow_` | `bool` | La trama excedió la capacidad; se descarta hasta el próximo silencio |
| `tx_[kTxCapacity]` | `uint8_t` | Respuesta compuesta |
| `txLen_` | `uint8_t` | Longitud de la respuesta pendiente |
| `stats_` | `BusStats` | Contadores de diagnóstico |

| Operación | Contrato |
|---|---|
| `onByte(b, nowMs)` | Acumula un byte recibido |
| `poll(nowMs)` | Resincroniza si venció el silencio con una trama parcial |
| `hasResponse()` | Hay una respuesta lista para transmitir |
| `response()`, `responseLength()` | Bytes a transmitir |
| `responseSent()` | El adaptador confirma que la envió |

**Estados de recepción**

```
   Idle ──primer byte──▶ Receiving ──longitud completa──▶ Validating
     ▲                       │                                 │
     │                       │ silencio t3.5 con trama parcial │
     │                       ▼                                 │
     └────────────────── (descarta) ◀──────CRC malo, u otro ID──┘
                                                               │
                                                     CRC bien, ID propio
                                                               │
                                                               ▼
                                                          Responding
```

**Reglas invariantes**

| Regla | Enunciado |
|---|---|
| MB-01 | Una petición dirigida a otro identificador se descarta **en silencio**: no se responde ni se altera nada |
| MB-02 | Una petición con CRC incorrecto se descarta en silencio y suma al contador de CRC |
| MB-03 | Toda longitud declarada se valida **antes** de usarse como índice, con aritmética que no puede desbordar |
| MB-04 | Una función no soportada responde excepción `0x01` |
| MB-05 | Un rango fuera del mapa responde excepción `0x02` |
| MB-06 | Toda función de escritura responde excepción `0x01`: el controlador es la única fuente de verdad |
| MB-07 | El esclavo **nunca** inicia una transmisión que no sea respuesta a una petición válida |

### 2.5 `BusStats` *(nueva)*

| Campo | Tipo | Se publica en |
|---|---|---|
| `validRequests` | `uint16_t` | Registro de diagnóstico |
| `crcErrors` | `uint16_t` | Registro de diagnóstico |
| `formatErrors` | `uint16_t` | Registro de diagnóstico |
| `lastRequestAtMs` | `uint32_t` | Alimenta la antigüedad publicada y la señal de bus mudo |

Todos los contadores **saturan** en su máximo en vez de dar la vuelta: un contador que
vuelve a cero miente sobre la salud del bus.

Corrige de paso que hoy `framingErrors()` se incremente en seis puntos y no lo lea nadie.

---

## 3. Transiciones de estado de una válvula

La máquina de cuatro fases se reduce a **dos estados y una transición**, porque no hay
nada que esperar:

```
        ┌───────────────┐   cambio estable del interruptor   ┌───────────────┐
        │  NO RIEGA     │ ─────────────────────────────────▶ │    RIEGA      │
        │  registro 0   │ ◀───────────────────────────────── │  registro 1   │
        └───────────────┘   cambio estable del interruptor   └───────────────┘
```

| Transición | Qué la provoca | Efecto |
|---|---|---|
| **T1** | Arranque o reinicio | Se siembra desde los pines físicos. **No** se emite ninguna orden: publicar no es emitir |
| **T2** | El antirrebote confirma un nivel distinto durante 50 ms | Se escribe el registro |

**Lo que ya no existe**: transmitir y esperar (T3), confirmar (T4), confirmación
discordante (T5), expirar y reintentar (T6), agotar intentos y entrar en reposo (T7),
salir del reposo (T8), y cancelar una orden en vuelo (T9, T11).

---

## 4. FR-116: por qué el registro no puede presentar un `0` transitorio

Es el requisito de seguridad dominante. Un solo sondeo que lea `0` cierra la válvula en
campo. El modelo lo garantiza por construcción, y cada garantía tiene su prueba:

| Momento de riesgo | Por qué es seguro | Prueba que lo blinda |
|---|---|---|
| **Arranque** | `setup()` siembra el banco desde los pines antes de que `loop()` atienda el bus. El esclavo no puede responder antes de estar sembrado | Sembrar y comprobar que el primer sondeo ya devuelve el estado físico correcto |
| **Reinicio por watchdog** | Idéntico al arranque. Durante el reinicio el Mega **no responde**, y una expiración **no es un `0`**: el maestro no dispara cierre ante un silencio | Reiniciar con válvulas regando y comprobar que se republica `1` |
| **Desbordamiento de `millis()`** | El valor deriva de `Debouncer::stable()`, que solo cambia cuando un nivel **distinto** se sostiene 50 ms. El desbordamiento no puede fabricar un cambio de nivel | Prueba de wraparound de extremo a extremo, inexistente hoy |
| **Refresco del panel** | El panel **lee** el banco, nunca lo escribe | Verificado por el sentido de las dependencias |
| **Lectura rota** | Ninguna interrupción escribe el banco; todo es cooperativo en un hilo | Verificado por diseño |

La distinción entre **no responder** y **responder `0`** es el corazón de la seguridad de
este sistema, y conviene que quede escrita: ante la duda, el esclavo calla.

---

## 5. Presupuesto de memoria

| Entidad | Bytes | Nota |
|---|---:|---|
| `RegisterFile` (32 × 16 bits) | 64 | 24 válvulas + 8 de diagnóstico |
| Búfer de recepción | 16 | La petición mayor soportada son 8 B |
| Búfer de respuesta | 64 | La respuesta mayor son 53 B |
| Estado de `ModbusSlave` | 16 | Índices, marcas, banderas |
| `BusStats` | 10 | |
| `Debouncer[24]` | 168 | Sin cambios |
| `shownSymbol_[24]` | 24 | Sin cambios |
| **Total del modelo nuevo** | **362** | |

Frente al modelo 001, que gastaba 240 B solo en `ValveRecord[24]` más el estado de la
política de reintentos y del planificador. El cambio queda **neutro o ligeramente
favorable**, sobre una línea base verificada de 1682 B de 8192 (20,5 %).
