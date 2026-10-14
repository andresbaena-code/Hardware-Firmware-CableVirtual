# Feature Specification: Separación de comandos por UC511

**Feature Branch**: `003-retardo-apertura-cancelable`

**Created**: 2026-10-14

**Status**: Draft

**Input**: User description: "Retardo de apertura cancelable en el firmware del Mega (spec 003).
[...] Motivo: el UC511 necesita al menos 5 s entre comandos al mismo equipo; si el tablero
cierra una válvula de un bloque y abre la otra del mismo UC511 casi a la vez, el segundo
comando se pierde." Refinado en conversación con el responsable técnico (2026-10-14): en
lugar de un retardo fijo en la apertura, el controlador conoce a qué UC511 pertenece cada
válvula y separa los cambios por equipo. Cada UC511 lleva además una marca de
instalado / no instalado.

> El nombre de la rama y la carpeta conservan el título original ("retardo de apertura
> cancelable"); el diseño que se especifica aquí lo generaliza.

## Contexto

Esta especificación **modifica** la publicación de estado definida en la spec 002
(`specs/002-modbus-esclavo-rs485/`), que sigue vigente en todo lo demás.

**El problema, medido en campo durante el piloto de 8 válvulas:**

- Cada UC511 acciona **dos** válvulas latch con un pulso de 12 V / 50 ms. Entre dos
  comandos al **mismo** UC511 tienen que pasar **al menos 5 s**, sea apertura o cierre;
  si llegan antes, el segundo se pierde sin aviso (lo más probable es la recarga del
  condensador del pulso). A UC511 distintos no les pasa: responden en ~1 s cada uno.
- El cliente riega con pulsos fijos de 7 min, **en secuencia**, por bloques. Cada bloque
  es un UC511 con 2 válvulas, y las dos se riegan **una detrás de la otra**.
- En el cambio de una válvula a la otra del mismo bloque, el tablero baja una entrada y
  sube la siguiente casi a la vez. El UC100 ve los dos cambios en el mismo sondeo y emite
  los dos comandos con menos de un segundo entre ellos: **o la válvula saliente no
  cierra, o la entrante no abre**.
- Retrasar la orden en el UC100 no sirve: su *Delay Time* no se cancela si la condición
  desaparece durante la espera (medido).

**La solución:** el controlador sabe qué válvulas comparten UC511 y no publica dos
cambios del mismo equipo con menos de un intervalo de seguridad entre ellos. El cambio
que llega segundo espera su turno, y si el tablero lo revierte mientras espera, se
descarta. Válvulas de equipos distintos no se esperan entre sí.

**Instalación actual:** un solo UC100 con 4 UC511 (V01–V08). La instalación completa
prevista es de 24 válvulas, 12 UC511 y 3 UC100.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Cambio de válvula dentro de un mismo bloque sin perder comandos (Priority: P1)

El tablero termina el riego de la primera válvula de un bloque y empieza el de la
segunda, que cuelga del mismo UC511. El operador espera que la primera cierre y la
segunda abra, como si un cable las uniera al tablero.

**Why this priority**: es el defecto que hoy impide el piloto. Un comando perdido deja
una válvula regando de más (hasta el temporizador del UC511) o un sector sin regar, y
nadie se entera porque el radio no confirma.

**Independent Test**: en host, bajar la entrada de V01 y subir la de V02 en la misma
vuelta del lazo; el registro de V01 pasa a `0` en esa vuelta y el de V02 pasa a `1`
exactamente al cumplirse el intervalo, nunca antes. En campo, repetir el cambio 10 veces.

**Acceptance Scenarios**:

1. **Given** V01 en `1` y V02 en `0` (mismo UC511), **When** el tablero baja V01 y sube
   V02 a la vez, **Then** V01 pasa a `0` de inmediato y V02 pasa a `1` cuando se cumple
   el intervalo desde el cierre de V01.
2. **Given** el mismo cambio pero con V02 detectada una vuelta **antes** que la bajada de
   V01, **When** ambos cambios están pendientes, **Then** se publica primero el cierre de
   V01 y la apertura de V02 espera el intervalo.
3. **Given** un bloque regado en secuencia (V01 7 min, luego V02 7 min), **When** termina
   la secuencia, **Then** cada válvula ha abierto y cerrado una vez.

---

### User Story 2 - Válvulas de equipos distintos no se retrasan (Priority: P1)

El tablero termina el riego de la última válvula de un bloque y empieza el de la primera
del bloque siguiente (UC511 distinto).

**Why this priority**: retrasar sin necesidad acorta el riego. La separación solo tiene
sentido dentro de un mismo equipo.

**Independent Test**: en host, bajar V02 (UC511 #01) y subir V03 (UC511 #02) en la misma
vuelta; ambos registros cambian en esa misma vuelta.

**Acceptance Scenarios**:

1. **Given** V02 en `1` y V03 en `0` en equipos distintos, **When** el tablero baja V02 y
   sube V03 a la vez, **Then** los dos registros cambian sin espera.

---

### User Story 3 - Un pedido breve o revertido no llega al campo (Priority: P2)

Un cambio que está esperando su turno se revierte en el tablero antes de publicarse (un
operario que prueba un interruptor, un pedido que dura pocos segundos).

**Why this priority**: sin cancelación, ese cambio y su reversión llegarían al campo con
menos de 5 s entre ellos y el segundo se perdería, dejando la válvula en el estado
equivocado.

**Independent Test**: en host, con un cambio reciente en el UC511 #01, subir V02 y bajarla
a los 3 s; el registro de V02 no llega a valer `1` en ningún momento.

**Acceptance Scenarios**:

1. **Given** una apertura pendiente, **When** la entrada vuelve a "no pedido" antes de
   publicarse, **Then** la apertura se descarta y el registro sigue en `0`.
2. **Given** un cierre pendiente, **When** la entrada vuelve a "pedido" antes de
   publicarse, **Then** el cierre se descarta y el registro sigue en `1` sin pasar por
   `0`.
3. **Given** una válvula recién abierta (publicada en `1`), **When** el tablero la baja a
   los 3 s, **Then** el cierre espera el intervalo y se publica; nunca se pierde.

---

### User Story 4 - Equipos no instalados (Priority: P2)

La instalación del piloto solo tiene los UC511 #01 a #04. Las válvulas V09–V24 están
asignadas en la configuración a los UC511 #05–#12 previstos, que aún no existen.

**Why this priority**: dejar el mapa completo evita reconfigurar el firmware al crecer,
pero el sistema debe ser honesto sobre qué equipos existen. Si al crecer alguien olvida
marcar un equipo como instalado, la válvula no abre y se nota en la puesta en marcha:
el olvido falla hacia cerrado, nunca hacia una válvula sin protección.

**Independent Test**: en host, subir la entrada de V09 (UC511 #05 no instalado); su
registro permanece en `0` y se emite un aviso por consola.

**Acceptance Scenarios**:

1. **Given** un UC511 marcado como no instalado, **When** el tablero pide una de sus
   válvulas, **Then** su registro permanece en `0`, el panel la muestra como `-` y la
   consola avisa una vez por pedido.
2. **Given** ese mismo equipo marcado como instalado y el firmware recompilado, **When**
   el tablero pide la válvula, **Then** se publica como cualquier otra.

---

### User Story 5 - Un reinicio no cierra válvulas que están regando (Priority: P1)

El controlador se reinicia mientras una válvula está regando con su entrada en `1`.

**Why this priority**: FR-116 de la spec 002 es el requisito de seguridad más importante
del sistema: un solo sondeo que lea `0` cierra la válvula en campo.

**Independent Test**: en host, sembrar con una entrada en `1`; el registro vale `1` desde
el primer sondeo.

**Acceptance Scenarios**:

1. **Given** una entrada en `1` al arrancar, **When** el maestro hace su primer sondeo,
   **Then** el registro ya vale `1`.
2. **Given** el arranque terminado, **When** otra válvula del mismo UC511 cambia antes de
   cumplirse el intervalo, **Then** ese cambio espera su turno.

---

### Edge Cases

- **Las dos válvulas de un UC511 pedidas a la vez.** Se publica primero la que se pidió
  antes; si llegaron en la misma vuelta, la de menor índice. La otra espera el intervalo.
- **Espera máxima.** Un cambio espera lo que falte del intervalo en curso más, como
  mucho, dos turnos: el de un pedido más antiguo de la otra válvula y el de un cierre
  de esa misma válvula que lo adelante por prioridad. La espera es siempre **menor que
  tres intervalos** (24 s). Solo se acerca a ese límite si el tablero cambia las dos
  válvulas de un equipo varias veces en pocos segundos; en la secuencia del cliente la
  espera es de un intervalo. *(Corregido en la implementación: el plan decía dos
  intervalos, y una prueba de peor caso mostró una espera de 23 s.)*
- **Desbordamiento del reloj** (~49,7 días). El intervalo dura lo mismo antes, durante y
  después.
- **Rebote del interruptor.** El antirrebote existente actúa antes: la separación trabaja
  sobre la entrada ya estable.
- **Bus caído durante una espera.** La espera es independiente del maestro: el cambio se
  publica a su hora y el primer sondeo posterior ya lo lee.
- **Pedido revertido varias veces mientras espera.** Cada reversión cancela el pendiente;
  solo se publica el estado que siga vigente cuando llegue el turno.
- **Cableado distinto del mapa.** Si en campo un par de válvulas no corresponde al UC511
  declarado, la separación protege al equipo equivocado. La puesta en marcha debe
  verificar el mapa contra el cableado real.

## Requirements *(mandatory)*

### Functional Requirements

**Mapa de equipos**

- **FR-201**: Cada una de las 24 válvulas DEBE declarar en el mapa de instalación el
  UC511 que la acciona (número de equipo, único en toda la finca) y la salida del equipo
  (1 o 2).
- **FR-202**: La compilación DEBE fallar si una válvula no tiene equipo asignado, si un
  equipo tiene más de 2 válvulas, si dos válvulas comparten la misma salida del mismo
  equipo o si un número de equipo está fuera de rango.
- **FR-203**: Cada UC511 DEBE declarar en el mapa si está **instalado**. En la
  configuración del piloto lo están los equipos #01 a #04; los #05 a #12 se declaran según
  el plan de 24 válvulas (pares consecutivos) y marcados como no instalados.

**Separación de comandos**

- **FR-204**: Un cambio del estado publicado de una válvula (apertura o cierre) NO DEBE
  publicarse si en el mismo UC511 se publicó otro cambio hace menos del **intervalo de
  separación**. En ese caso queda pendiente y se publica al cumplirse el intervalo.
- **FR-205**: Si no hay un cambio reciente en su equipo, el cambio DEBE publicarse en la
  misma vuelta del lazo en que se detecta, sin espera.
- **FR-206**: Un cambio pendiente DEBE cancelarse si la entrada vuelve a su estado
  publicado antes de que se publique. Un cambio cancelado no ocupa turno ni cuenta como
  cambio publicado del equipo.
- **FR-207**: Si en un mismo equipo hay un cierre y una apertura pendientes, DEBE
  publicarse primero el cierre. Entre dos pendientes del mismo tipo, primero el que se
  pidió antes; a igualdad, el de menor índice.
- **FR-208**: Los cambios de válvulas de equipos distintos NO DEBEN esperarse entre sí.
- **FR-209**: La separación NO DEBE introducir nunca un `0` en una válvula publicada en
  `1` mientras su entrada siga pedida, ni un `1` en una válvula cuya entrada no lo pide.
  Solo retrasa cambios que el tablero ya pidió. FR-110 y FR-116 de la spec 002 siguen
  vigentes.
- **FR-210**: El intervalo de separación DEBE medirse con aritmética segura frente al
  desbordamiento del contador de tiempo.
- **FR-211**: La siembra inicial al arrancar NO DEBE esperar: cada válvula se publica con
  el estado de su entrada antes del primer sondeo (FR-116 de la spec 002). El instante de
  arranque cuenta como último cambio de cada equipo con alguna válvula sembrada en `1`.

**Equipos no instalados**

- **FR-212**: Una válvula cuyo UC511 no esté instalado DEBE publicar siempre `0`, ignore
  lo que pida el tablero. Su registro sigue existiendo y respondiendo (FR-118 de la
  spec 002).
- **FR-213**: Cuando el tablero pida una válvula de un equipo no instalado, la consola
  DEBE avisarlo una vez por pedido (no en cada vuelta).

**Configuración, interfaz y pruebas**

- **FR-214**: El intervalo de separación DEBE declararse en el módulo de configuración
  temporal con un valor de **8 s**, junto con la separación mínima medida del actuador
  (5 s). La compilación DEBE fallar si el intervalo es menor que esa separación mínima.
- **FR-215**: El panel DEBE mostrar el valor **publicado** de cada válvula (`0` mientras
  un cambio espera) y `-` para las válvulas de equipos no instalados.
- **FR-216**: La consola DEBE registrar, por válvula, cada cambio que queda pendiente,
  su publicación y su cancelación. Las trazas se desactivan en compilación sin alterar
  el comportamiento (Principio VI).
- **FR-217**: La lógica de separación DEBE ser código puro y comprobable en la suite de
  escritorio, con pruebas de: cambio sin espera, cambio con espera, cancelación de
  apertura y de cierre, prioridad del cierre, orden entre pendientes, independencia entre
  equipos, equipos no instalados, siembra al arrancar, espera máxima y desbordamiento del
  reloj.
- **FR-218**: Las guías del piloto (`docs/piloto-8-valvulas-d2d.md`) y de configuración de
  radio DEBEN reflejar el diseño acordado: reglas del UC100 con una sola acción y sin
  *Delay Time*, `Duration` del UC511 como red de seguridad, el mapa válvula → UC511 y el
  efecto de la separación en el pulso de riego.

### Excepciones documentadas a la spec 002

- **FR-110** dice que el registro vale `1` "mientras el tablero pida riego". Con esta
  especificación, un cambio puede publicarse hasta casi tres intervalos más tarde de lo que lo
  pide el tablero, y una válvula de un equipo no instalado vale siempre `0`. Ninguna de las
  dos produce un `0` transitorio en una válvula que está regando, por lo que **FR-116 se
  cumple sin cambios**.
- **FR-114b** limita el panel a `0` y `1`. Se añade `-` con un significado de
  configuración ("equipo no instalado"), no de confirmación del campo, por lo que la regla
  de honestidad de la interfaz (Principio II) se mantiene.

### Key Entities

- **UC511 (equipo actuador)**: número de equipo único en la finca, marca de instalado,
  y el instante de su último cambio publicado.
- **Asignación de válvula**: por válvula, el equipo y la salida (1 o 2).
- **Cambio pendiente**: por válvula, el estado que espera publicarse y el instante en que
  se pidió. Desaparece al publicarse o al cancelarse.
- **Intervalo de separación**: parámetro de instalación en milisegundos, común a todos los
  equipos (8 s).
- **Separación mínima del actuador**: límite medido del UC511 (5 s) contra el que se
  valida el intervalo en compilación.

### Mapa de la instalación piloto

| Válvula | Entrada | Registro | UC511 | Salida | Instalado |
|---|---|---|---|---|---|
| V01 | 1 | 0 | #01 | 1 | sí |
| V02 | 2 | 1 | #01 | 2 | sí |
| V03 | 3 | 2 | #02 | 1 | sí |
| V04 | 4 | 3 | #02 | 2 | sí |
| V05 | 5 | 4 | #03 | 1 | sí |
| V06 | 6 | 5 | #03 | 2 | sí |
| V07 | 7 | 6 | #04 | 1 | sí |
| V08 | 8 | 7 | #04 | 2 | sí |
| V09–V24 | 9–24 | 8–23 | #05–#12 (pares consecutivos) | 1 / 2 | no |

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-201**: En 10 cambios consecutivos de válvula dentro del mismo UC511, las dos
  válvulas responden las 10 veces (0 comandos perdidos).
- **SC-202**: En 10 cambios entre válvulas de UC511 distintos, ambos registros cambian en
  la misma vuelta del lazo (0 s de espera añadida).
- **SC-203**: Un pedido revertido mientras espera no produce ningún movimiento en campo en
  10 de 10 intentos; un cierre pedido 3 s después de una apertura llega al campo en 10 de
  10 intentos.
- **SC-204**: Tras un reinicio con válvulas regando, ningún sondeo lee `0` en esas
  válvulas (0 lecturas de `0` en 10 reinicios).
- **SC-205**: Pedir una válvula de un equipo no instalado deja su registro en `0` y genera
  exactamente un aviso por pedido.
- **SC-206**: En la secuencia del cliente, la primera válvula de cada bloque riega los
  7 min completos y la segunda 7 min menos el intervalo (6 min 52 s), con desviación menor
  a 2 s medida en campo.
- **SC-207**: El lazo de control sigue sin superar 2 ms por vuelta y el uso de SRAM sigue
  por debajo del 75 %.

## Assumptions

- La separación mínima de 5 s del UC511 es una medida de campo del piloto, no un dato del
  fabricante. El intervalo de 8 s deja 3 s de margen para la latencia del sondeo del UC100
  (~1 s medida).
- El cliente acepta que la segunda válvula de cada bloque riegue 8 s menos (6 min 52 s).
- El cableado del piloto coincide con el mapa de la guía del piloto (V01/V02 en UC511 #01,
  y así sucesivamente); se verifica en la puesta en marcha.
- La instalación completa seguirá el patrón de pares consecutivos; si no, se edita el mapa
  y se recompila.
- Cada UC511 conserva su temporizador de duración como red de seguridad independiente
  (Principio II, mecanismo b).
- El intervalo es común a todos los equipos; un intervalo por equipo queda fuera de alcance.
- El mapa de registros Modbus de la spec 002 no cambia; no se agrega ningún registro.
- Pasar a 3 UC100 y 3 buses RS485 es una especificación aparte; esta no depende de cuántos
  UC100 haya, porque la separación es por UC511.
