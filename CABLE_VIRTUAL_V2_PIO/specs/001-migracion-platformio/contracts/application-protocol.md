# Contrato — Protocolo de aplicación Cable Virtual ↔ Nodo remoto

**Feature**: `001-migracion-platformio` | **Versión del contrato**: 1.0 | **Fecha**: 2026-08-20

Este contrato es **inmutable** en el alcance de esta funcionalidad. Los nodos remotos ya
están desplegados en campo y no se modifican (FR-015, R-012). Cualquier cambio aquí exige
intervención de hardware en la finca.

---

## 1. Capa de transporte — XBee ZigBee

| Parámetro | Valor | Origen |
|---|---|---|
| Modo de operación | **API con escape (AP = 2)** | R-001, verificado en el código de la librería |
| Puerto serie del radio | `Serial1` | heredado |
| Velocidad | 9 600 baudios | heredado |
| Dirección de 64 bits | `0x0013A200` (alta) + LSB por nodo | heredado |
| Trama de envío | ZB Transmit Request (API 0x10) | librería `XBee` |
| Trama de recepción | ZB Receive Packet (API 0x90) | librería `XBee` |
| `frameId` | `0` | heredado — **desactiva el acuse de transmisión del radio** |

> **Nota de diseño**: con `frameId = 0` el módulo XBee no emite `ZB TX Status`. La
> confirmación de entrega es responsabilidad exclusiva del protocolo de aplicación (la
> respuesta del nodo). Se conserva el valor heredado para no cambiar el tráfico de la red.

**Regla de conformidad TR-01**: el firmware NO DEBE transmitir en modo API sin escape
(AP = 1). Los nodos instalados descartarían cualquier trama que contenga `0x7E`, `0x7D`,
`0x11` o `0x13` en su carga.

---

## 2. Payload de la orden (controlador → nodo)

Longitud: **5 bytes**.

| Offset | Nombre | Valor | Descripción |
|---|---|---|---|
| 0 | `header` | `0xAA` | encabezado fijo |
| 1 | `opcode` | `0x14` | operar válvula |
| 2 | `valveId` | `0x00`–`0x07` | válvula dentro del nodo |
| 3 | `level` | `0x00` / `0x01` | **nivel eléctrico**, lógica inversa (§4) |
| 4 | `check` | calculado | dígito verificador |

### Cálculo del verificador

```
check = 0xFF - ((header + opcode + valveId + level) & 0xFF)
```

La resta se realiza en 8 bits sin signo. Es el mismo cálculo del firmware base
(`CABLE_VIRTUAL___LATCH_V1.ino`, función `transmitirOrden`), reproducido byte a byte.

**Ejemplo normativo** — abrir la válvula 3 (nivel `Low` = 0, ver §4):

```
header  = 0xAA
opcode  = 0x14
valveId = 0x03
level   = 0x00
suma    = 0xAA + 0x14 + 0x03 + 0x00 = 0xC1
check   = 0xFF - 0xC1 = 0x3E
payload = AA 14 03 00 3E
```

**Ejemplo normativo** — cerrar la válvula 7 (nivel `High` = 1):

```
suma    = 0xAA + 0x14 + 0x07 + 0x01 = 0xC6
check   = 0xFF - 0xC6 = 0x39
payload = AA 14 07 01 39
```

**Regla de conformidad TX-01**: el firmware DEBE transmitir exactamente 5 bytes en este
orden. No se añaden campos.

---

## 3. Payload de la confirmación (nodo → controlador)

El nodo responde con el estado que efectivamente alcanzó su válvula.

| Offset | Nombre | Valor | Obligatorio |
|---|---|---|---|
| 0 | `header` | `0xAA` | sí |
| 1 | `opcode` | `0x14` | sí |
| 2 | `valveId` | `0x00`–`0x07` | sí |
| 3 | `level` | `0x00` / `0x01` | sí |
| 4 | `check` | verificador | **opcional** (ver RX-03) |

La identidad del nodo emisor **no viaja en el payload**: se toma de los 32 bits bajos de la
dirección de 64 bits de origen de la trama XBee (`getRemoteAddress64().getLsb()`).

### Reglas de validación en recepción

| Id | Regla | Acción si falla | Requisito |
|---|---|---|---|
| RX-01 | La longitud de la trama es ≥ 4 bytes | descartar sin leer campos | FR-016, P-02 |
| RX-02 | `header == 0xAA` y `opcode == 0x14` | descartar | FR-016 |
| RX-03 | Si la longitud es ≥ 5, el verificador debe cuadrar | descartar y contar como error de radio | FR-016 |
| RX-04 | `level` es `0x00` o `0x01` | descartar | FR-016 |
| RX-05 | El par (dirección de origen, `valveId`) existe en el mapa de instalación | descartar y registrar en la consola | FR-016 |
| RX-06 | El `level` recibido corresponde al estado solicitado para esa válvula | **no confirmar**; mantener la secuencia de reintentos | FR-005, T5 |

**Nota sobre RX-03 (tolerancia deliberada)**: se aceptan confirmaciones de 4 bytes sin
verificador. Las versiones anteriores del sistema (`WIGA_24IN_LCD`,
`WIGA_24IN_LCDB1_5`) transmitían 4 bytes, y no hay garantía documentada de que todos los
nodos en campo respondan con 5. Rechazar tramas de 4 bytes convertiría un nodo antiguo
funcional en un nodo mudo. El verificador se valida cuando está presente.

**Regla de conformidad RX-07**: el firmware NO DEBE actualizar el estado confirmado de una
válvula por el solo hecho de haber transmitido una orden. La confirmación del nodo es la
única fuente de verdad (FR-005, invariante R-01).

---

## 4. Convención de niveles — lógica inversa por transistor

El byte `level` transporta el **nivel eléctrico** de la etapa de potencia del nodo, que
opera con lógica inversa por transistor:

| `level` en el protocolo | Estado físico de la válvula |
|---|---|
| `0x01` (`High`) | **cerrada** |
| `0x00` (`Low`) | **abierta** |

**Regla de conformidad NV-01**: la conversión entre nivel eléctrico y estado físico ocurre
en un único punto del código (`toValveState` / `toSignalLevel`, ver
[data-model.md §1.3](../data-model.md)). Ninguna otra función interpreta el byte `level`.

**Regla de conformidad NV-02**: el panel LCD muestra el **estado físico**, no el nivel.
Esta inversión es una característica del hardware confirmada por el responsable técnico, no
un defecto. *Quien la vea en el código y la "corrija" romperá la instalación.*

Las entradas del tablero siguen la misma convención: se leen con pull-up interno, y el
nivel bajo corresponde a la solicitud de apertura.

---

## 5. Comportamiento temporal del diálogo

| Parámetro | Valor | Requisito |
|---|---|---|
| Órdenes simultáneas en vuelo | **1** como máximo, en todo el sistema | FR-004 |
| Separación mínima entre órdenes a válvulas distintas | 1 500 ms | FR-004 |
| Espera de confirmación antes de reintentar | 30 000 ms | FR-006 |
| Reintentos rápidos por cambio de estado | 5 | FR-006 |
| Reposo tras agotar reintentos | 3 600 000 ms (60 min) | FR-007 |
| Cancelación del reposo | inmediata si la entrada cambia | FR-008 |

**Regla de conformidad TM-01**: ante un fallo persistente de radio en una válvula, el
número de transmisiones no puede superar 5 en la primera media hora ni 1 por hora después
(SC-005, NFR-003).

---

## 6. Casos límite y su tratamiento normativo

| Caso | Tratamiento | Requisito |
|---|---|---|
| Confirmación de un nodo desconocido | descartar, registrar, no alterar estado | RX-05, FR-016 |
| Confirmación de un `valveId` fuera de rango en un nodo conocido | descartar, registrar | RX-05 |
| Confirmación duplicada de una válvula ya confirmada | idempotente: sin efecto | FR-005 |
| Confirmación con nivel distinto al solicitado | no confirma; sigue la secuencia de reintentos | RX-06, T5 |
| Trama corrupta detectada por la capa XBee | descartar; contador de errores de radio++ | FR-028 |
| Delimitador de inicio inesperado (parser desincronizado) | el parser se reinicia solo; contador++ | R-003 |
| Varias confirmaciones acumuladas en el búfer | drenar hasta 4 tramas por vuelta de lazo | FR-017, R-002 |
| Búfer de recepción con datos al momento de transmitir | **se conserva y se procesa**; nunca se vacía | FR-018, R-003 |

---

## 7. Verificación del contrato

Toda regla `TX-*`, `RX-*` y `NV-*` de este documento tiene prueba automatizada en
`test/test_domain/test_app_protocol`, ejecutable en el entorno `native` sin hardware.

Las reglas `TR-*` y `TM-*` se verifican en banco durante la prueba de aceptación
documentada en [quickstart.md](../quickstart.md), porque dependen del comportamiento real
del radio y de los nodos.

**Antes de sustituir firmware en una instalación productiva** debe confirmarse contra un
nodo real que el payload de 5 bytes con verificador es aceptado (R-012). El firmware base
ya lo usa, pero la evidencia debe registrarse en el acta de aceptación.
