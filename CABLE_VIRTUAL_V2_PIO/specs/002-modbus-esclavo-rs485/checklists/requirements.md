# Specification Quality Checklist: El controlador de riego como esclavo Modbus RTU

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-08-27
**Feature**: [spec.md](../spec.md)

## Content Quality

- [x] No implementation details (languages, frameworks, APIs)
- [x] Focused on user value and business needs
- [x] Written for non-technical stakeholders
- [x] All mandatory sections completed

## Requirement Completeness

- [x] No [NEEDS CLARIFICATION] markers remain
- [x] Requirements are testable and unambiguous
- [x] Success criteria are measurable
- [x] Success criteria are technology-agnostic (no implementation details)
- [x] All acceptance scenarios are defined
- [x] Edge cases are identified
- [x] Scope is clearly bounded
- [x] Dependencies and assumptions identified

## Feature Readiness

- [x] All functional requirements have clear acceptance criteria
- [x] User scenarios cover primary flows
- [x] Feature meets measurable outcomes defined in Success Criteria
- [x] No implementation details leak into specification

## Notes

### RESUELTO: el conflicto con el Principio II se cerró por enmienda formal

La constitución pasó a la versión **2.0.0** el 2026-08-27, con aprobación del
responsable técnico. La regla *"Confirmación obligatoria"* se eliminó por imposible de
cumplir con el medio real, y se sustituyó por el régimen de re-aserción acotada más
auto-apagado del actuador, más una regla nueva de honestidad de la interfaz.

El Principio IV se reforzó a la vez, exigiendo que la lógica de protocolo sea código
puro comprobable en `native`. La especificación ya no está en conflicto y queda lista
para `/speckit.plan`.

### Decisiones del responsable técnico incorporadas (2026-08-27)

- **Semántica del registro: nivel.** `1` mientras el tablero pida riego, `0` cuando no.
  El controlador nunca lo devuelve a `0` por iniciativa propia. La repetición del
  comando de radio la produce el *lockout time* de la regla del puente, no un
  parpadeo del valor. Recogido en FR-110, FR-115 y FR-116.
- **Panel: solo `0` y `1`, con etiqueta de pedido.** Se retiran los símbolos `?` y `!`.
  Recogido en FR-114b.

Se descartó una variante de registro como código de orden (`0` nada, `1` abrir, `2`
cerrar) que habría permitido ráfagas explícitas controladas por firmware. Con las reglas
`IF valor==1 → abrir` e `IF valor==0 → cerrar` ya previstas para el puente, esa variante
exigía condiciones de rango y dejaba la válvula sin reemisión tras un reinicio.

### Historial de correcciones de premisa

Esta especificación se corrigió dos veces a medida que se conoció la topología real. Se
deja constancia porque cada corrección invalidó trabajo anterior:

| Premisa | Origen | Estado |
|---|---|---|
| El controlador es Modbus **maestro** y sondea a los nodos | Código de la rama 001 | **Refutada** por el responsable técnico y por la documentación de Milesight |
| El controlador es esclavo y **recibe confirmaciones** del maestro | Primer borrador de esta spec | **Refutada**: D2D no tiene ACK, el sistema es de lazo abierto |
| El controlador es esclavo, publica estado y **re-afirma periódicamente** | Descripción del responsable técnico + documentación | **Vigente** |

### Hallazgo decisivo de la documentación del fabricante

En los tres modos de operación del UC100, el equipo conectado a su RS485 es un esclavo:

- Modbus Channels: *"UC100 supports to work as a Modbus RTU Client (Master) to poll the
  data from the RS485 device"*.
- Active Pass-through: *"the RS485 device can only react according to server commands"*.

Queda refutada de forma definitiva la afirmación del código en
`lib/wiga_ports/i_modbus_port.h:4` y `lib/wiga_hal/rs485_modbus_adapter.h:4`.

### Verificación de contenido

Las afirmaciones cuantitativas no son estimaciones: se verificaron numéricamente contra
el código de la rama `001-migracion-platformio` durante la auditoría previa.

| Afirmación | Dónde se usa | Cómo se verificó |
|---|---|---|
| El round-trip devuelve siempre el estado contrario | FR-111, auditoría | Tabla de verdad: FC05 escribe `rawOf(level)`, FC02 lee `NOT levelFromRaw(bit)` |
| 12 de 24 válvulas inalcanzables | FR-118, SC-102 | `kMaxFramesPerPoll=4` limita a valveId 0..3; 12 índices tienen id ≥ 4 |
| `byteCount` 251 lee 254 B de un buffer de 32 | Edge case, FR-107, SC-107 | `(uint8_t)(3+251+2) == 0`, la comprobación pasa |
| Ningún nodo cae en el slave ID 1, el único sondeado | Contexto | `(LSB%247)+1` da 220, 133, 74, 105, 123, 69, 140, 89, 150, 80 |
| Presupuesto real de 480-805 ms por vuelta frente a 15 ms | SC-104 | Busy-wait de `waitResponse` más `delay(5)` por reintento |

### Decisiones anteriores que quedaron superadas

- **Columna `slaveId` explícita en el mapa** (decidida cuando se creía que el controlador
  era maestro de 10 nodos). Ya no aplica: el bus tiene **un solo esclavo**, que es el
  propio controlador, con un único identificador. El mapa necesita en su lugar la
  dirección de **registro Modbus** por válvula (FR-119).
- **Elección entre Modbus Channels y Active Pass-through.** Resuelta: es Modbus
  Channels, porque las reglas IF-THEN locales del UC100 son las que disparan D2D sin
  gateway.

### Alcance acotado por el responsable técnico

El UC100 y el UC501 son equipos comprados. Esta especificación cubre **solo el firmware
del ATmega2560**. Quedan explícitamente fuera el dimensionamiento de equipos, el tipo de
electroválvula y el temporizador de seguridad del actuador.
