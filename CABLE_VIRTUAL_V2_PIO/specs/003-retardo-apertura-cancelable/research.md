# Research: Separación de comandos por UC511

**Spec**: [spec.md](spec.md) · **Plan**: [plan.md](plan.md) · **Fecha**: 2026-10-14

No quedaron incógnitas `NEEDS CLARIFICATION` en el contexto técnico. Este documento
registra las decisiones de diseño, con su porqué y las alternativas descartadas.

---

## R-301 — Dónde se hace la separación

**Decisión**: en el controlador (Mega), en una clase nueva y pura
`CommandSpacer` dentro de `lib/wiga_control/`. `ValvePublisher` le entrega el estado
pedido de cada válvula y escribe en el banco de registros lo que el separador le
autoriza.

**Rationale**: el controlador es el único punto que ve las 24 entradas a la vez y que
puede retener un cambio sin emitir nada. Una clase aparte deja la lógica de turnos
comprobable sin puertos ni banco de registros, y `ValvePublisher` sigue siendo una
tubería simple.

**Alternativas descartadas**:
- *Delay Time del UC100*: medido en campo, no se cancela si la condición desaparece
  durante la espera.
- *Varias acciones por regla del UC100*: ToolBox prohíbe acciones repetidas y el UC100
  no sabe qué otras reglas apuntan al mismo UC511.
- *Retardo fijo en la apertura* (versión 1 de la spec): no cubre pedidos de 8–13 s ni
  pedidos simultáneos en un mismo equipo, y retrasa sin necesidad las válvulas de
  equipos distintos.
- *Meter la lógica dentro de `ValvePublisher`*: mezclaría muestreo, turnos y panel en
  una clase, y las pruebas de turnos necesitarían montar entradas falsas.

## R-302 — Cómo se declara el mapa de equipos

**Decisión**: se añaden dos campos a `ValveMapping` en
`lib/wiga_config/installation_map.h`: `actuator` (número de UC511, 1..N) y `output`
(1 o 2). Los equipos se declaran en un arreglo propio `kActuators[]`, indexado por
número de equipo, con la marca `installed`. `kActuatorCount` se **deriva** del tamaño
de ese arreglo (Principio V).

Validaciones en compilación (nuevas reglas `MAP-V13..MAP-V17`):

| Regla | Falla si |
|---|---|
| MAP-V13 | una válvula tiene `actuator` fuera de 1..`kActuatorCount` |
| MAP-V14 | una válvula tiene `output` distinto de 1 o 2 |
| MAP-V15 | dos válvulas comparten (`actuator`, `output`) |
| MAP-V16 | un equipo tiene más de 2 válvulas (implícito por V14+V15, se declara igual para que el mensaje sea claro) |
| MAP-V17 | un equipo declarado no tiene ninguna válvula (hueco no intencionado) |

**Rationale**: mantiene un único archivo de configuración por finca, como ya exige el
proyecto, y convierte errores de mapa en errores de compilación.

**Alternativas descartadas**: derivar el equipo del índice (`índice / 2`): rompe en
cuanto un par no sea consecutivo y oculta la asignación a quien lee el mapa.

## R-303 — Estado que guarda el separador

**Decisión**:
- Por válvula: estado publicado, estado pendiente (si hay), instante del pedido.
- Por equipo: instante del último cambio publicado y si existe alguno.

Memoria estimada: 24 × (1 + 1 + 4) + 12 × (4 + 1) ≈ **204 B** de SRAM (hoy 1731 B,
21,1 %; quedaría en torno al 24 %, lejos del 75 %).

**Rationale**: es el mínimo que permite cancelar, ordenar por antigüedad y medir el
intervalo. Sin memoria dinámica.

**Alternativas descartadas**: un número de secuencia en lugar del instante del pedido
ahorraría 48 B, pero añade un contador con su propio desbordamiento. No compensa con el
margen de SRAM actual.

## R-304 — Algoritmo de turnos

**Decisión**: en cada vuelta de control (cada 15 ms):

1. Para cada válvula, el estado **deseado** es el pedido del tablero ya filtrado por el
   antirrebote, forzado a cerrado si su equipo no está instalado.
2. Si deseado ≠ publicado y no hay pendiente → se crea un pendiente con el instante
   actual. Si deseado = publicado y hay pendiente → se **cancela**.
3. Para cada equipo cuyo último cambio tenga al menos el intervalo de antigüedad (o que
   no tenga ninguno): de sus pendientes se publica **uno**: primero cierres, luego el más
   antiguo, a igualdad el de menor índice. Se anota el instante como último cambio.

Un pendiente cuyo equipo está libre se publica en la misma vuelta en que se crea
(FR-205): los pasos 2 y 3 ocurren en la misma llamada.

**Espera máxima** *(corregida en la implementación)*: un pendiente espera lo que falte
del intervalo en curso, más como mucho dos turnos de la otra válvula de su equipo: uno
por antigüedad (un pedido suyo más antiguo) y otro por prioridad (un cierre que la otra
válvula pida después). No puede haber un tercero: el siguiente pedido de la otra
válvula sería más nuevo, y si nuestro pendiente es un cierre, un cierre ajeno no le gana
por prioridad. Cota: **menos de 3 × intervalo** (24 s).

El plan decía 2 × intervalo. La prueba
`test_FR207_peor_caso_un_cierre_adelanta_dos_veces` reproduce una espera de 23 s, y
`test_FR207_espera_maxima_bajo_pedidos_al_azar` verifica la cota en una hora simulada.

Alternativa descartada: ordenar solo por antigüedad, con el cierre como desempate.
Acotaría la espera a 2 intervalos, pero en el caso más común del campo -el tablero
detecta la subida de B una vuelta antes que la bajada de A- abriría B primero y dejaría
las dos válvulas abiertas durante un intervalo.

## R-305 — Arranque

**Decisión**: `begin()` siembra el estado publicado desde las entradas (sin esperar,
FR-116 de la spec 002) y anota el instante de arranque como último cambio de cada equipo
que tenga alguna válvula sembrada en `1`.

**Rationale**: si el tablero y el UC100 comparten alimentación, tras un corte el UC100
puede emitir las aperturas en su primer sondeo. Anotarlas como cambio protege a un
segundo cambio de ese mismo equipo en los segundos siguientes. Los equipos con todas sus
válvulas en `0` no emiten nada al arrancar y quedan libres.

## R-306 — Equipos no instalados

**Decisión**: deseado = cerrado siempre. Cuando el antirrebote confirma un pedido de
riego en una de sus válvulas se emite **una** traza de aviso. El aviso sale del flanco
del antirrebote, así que ocurre una vez por pedido sin necesidad de memoria adicional.

**Rationale**: decisión del responsable técnico: el olvido de marcar un equipo como
instalado falla hacia cerrado y se detecta en la puesta en marcha.

## R-307 — Panel y trazas

**Decisión**:
- Nuevo símbolo `symbol::kNotInstalled = '-'`. El panel muestra el valor **publicado**
  (`0`/`1`) o `-`.
- Nuevas etiquetas de traza en `logtag`: `WAIT` (cambio retenido por turno), `CANC`
  (pendiente cancelado), `NOINST` (pedido sobre equipo no instalado). La publicación
  sigue usando `TX`.

**Rationale**: Principio VI, cada transición se cuenta. El `-` es configuración, no
confirmación del campo: respeta la regla de honestidad de la interfaz.

## R-308 — Constantes de tiempo

**Decisión** en `lib/wiga_config/timing_config.h`:

```text
kActuatorMinGapMs  = 5000   // separación mínima medida del UC511
kCommandSpacingMs  = 8000   // intervalo de separación aplicado
static_assert(kCommandSpacingMs >= kActuatorMinGapMs)
```

El intervalo se mide con `wiga::elapsed()`, seguro frente al desbordamiento (FR-210).

## R-309 — Presupuesto del lazo

**Decisión**: el trabajo por vuelta es un recorrido de 24 válvulas más 12 equipos con
operaciones enteras. Muy por debajo de 2 ms en un ATmega2560 a 8 MHz. No requiere
medición especial más allá del escenario de presupuesto ya existente.
