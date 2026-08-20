# Feature Specification: Migración del firmware Cable Virtual a PlatformIO

**Feature Branch**: `001-migracion-platformio`

**Created**: 2026-08-20

**Status**: Draft

**Input**: User description: "Migrar el firmware Arduino 'Cable Virtual' (sketch CABLE_VIRTUAL___LATCH_V1.ino) a un proyecto PlatformIO profesional en VS Code para Arduino Mega 2560, reestructurando el código monolítico en capas testeables sin cambiar el comportamiento observable en campo. El sistema lee 24 entradas digitales que replican el estado de un tablero PLC ('cable virtual') y, ante cada cambio, transmite por XBee ZigBee en modo API la orden de abrir o cerrar la válvula latch correspondiente en el nodo remoto, confirmando el estado solo cuando el nodo responde. Debe conservar: disparo secuencial de una sola válvula a la vez para no agotar la batería de 12V, reintentos acotados con espera corta y luego periodo de reposo prolongado, checksum en el payload, watchdog de 8 segundos, y visualización del estado de las 24 válvulas en un LCD I2C 20x4. Debe eliminar los bloqueos con delay() del lazo de control, centralizar el mapeo de direcciones/IDs/pines en un módulo de configuración único, y agregar trazas de diagnóstico por el puerto serie. Los sketches WIGA_24IN_LCD y WIGA_24IN_LCDB1_5 se archivan como legado documentado, no se migran."

## Contexto del Negocio

El sistema **Cable Virtual** sustituye el cableado físico entre un tablero de control
de riego y las válvulas distribuidas en la finca. Un controlador central lee 24 señales
del tablero y las replica de forma inalámbrica sobre las válvulas latch remotas. Para el
operador de riego el sistema debe comportarse como si el cable existiera: lo que marca el
tablero es lo que ocurre en campo.

El firmware actual funciona en instalaciones productivas, pero vive como un sketch
monolítico de un solo archivo, sin control de versiones, sin pruebas y con la
configuración de cada finca escrita a mano dentro del código. Esta funcionalidad traslada
ese firmware a una base mantenible sin poner en riesgo la operación existente.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - El campo sigue al tablero (Priority: P1)

El operador de riego acciona una salida en el tablero de control. La válvula
correspondiente en campo abre o cierra, y el panel del controlador confirma el cambio.
Si la válvula no responde, el controlador lo reintenta por su cuenta y deja constancia
visible de la falla, sin que el operador tenga que intervenir.

**Why this priority**: es la razón de existir del producto. Si esto se degrada, la
migración es un retroceso aunque el código quede impecable.

**Independent Test**: en banco, con un nodo remoto y un generador de señal en las
entradas, se conmuta cada una de las 24 entradas y se verifica que la válvula asociada
cambia de estado y que el panel refleja la confirmación del nodo.

**Acceptance Scenarios**:

1. **Given** el controlador operando en régimen y la válvula 7 cerrada, **When** la
   entrada 7 del tablero pasa a activa, **Then** el controlador transmite la orden de
   apertura a la válvula 7 y, al recibir la confirmación del nodo, muestra la válvula 7
   como abierta.
2. **Given** una orden transmitida a la válvula 3, **When** el nodo remoto no confirma
   dentro del tiempo de espera, **Then** el controlador reintenta la orden hasta el
   límite establecido y luego entra en reposo prolongado sin dejar de atender el resto de
   las válvulas.
3. **Given** dos entradas que cambian en el mismo instante, **When** el controlador
   procesa los cambios, **Then** transmite una orden a la vez, separadas por la ventana
   de exclusión, nunca de forma simultánea.
4. **Given** una válvula en reposo prolongado por reintentos agotados, **When** su entrada
   vuelve a cambiar de estado, **Then** el controlador abandona el reposo y atiende el
   nuevo cambio de inmediato.

---

### User Story 2 - Compilar y cargar desde VS Code (Priority: P1)

El técnico de firmware clona el repositorio, abre la carpeta en VS Code y compila y carga
el firmware en el Arduino Mega sin instalar librerías a mano ni ajustar rutas.

**Why this priority**: sin build reproducible no hay repositorio útil, y el resto de las
mejoras no se pueden entregar ni verificar.

**Independent Test**: en una máquina limpia con VS Code y PlatformIO, clonar el
repositorio y ejecutar la compilación; debe terminar sin errores y sin pasos manuales.

**Acceptance Scenarios**:

1. **Given** un equipo sin librerías de Arduino instaladas, **When** el técnico compila el
   proyecto, **Then** las dependencias se descargan automáticamente en las versiones
   fijadas y la compilación termina sin errores.
2. **Given** el firmware compilado, **When** el técnico lo carga en un Arduino Mega 2560,
   **Then** el controlador arranca y muestra su versión en el panel y en la consola.

---

### User Story 3 - Verificar la lógica sin hardware (Priority: P2)

El técnico de firmware modifica la política de reintentos y verifica en segundos, en su
propio equipo, que la lógica sigue siendo correcta, sin conectar radios ni válvulas.

**Why this priority**: es lo que hace sostenible el mantenimiento. Cada verificación en
campo cuesta un desplazamiento; en el equipo cuesta segundos.

**Independent Test**: ejecutar la suite de pruebas del proyecto en el equipo del técnico,
sin ninguna placa conectada, y obtener un resultado de aprobado/fallido.

**Acceptance Scenarios**:

1. **Given** el repositorio clonado y ninguna placa conectada, **When** se ejecuta la
   suite de pruebas, **Then** todas las pruebas de la lógica de control se ejecutan y
   reportan su resultado.
2. **Given** un cambio que rompe la política de disparo secuencial, **When** se ejecuta la
   suite, **Then** al menos una prueba falla e identifica la regla violada.

---

### User Story 4 - Configurar una instalación nueva (Priority: P2)

El responsable técnico pone en marcha el sistema en una finca nueva: ajusta las
direcciones de los radios, los identificadores de válvula y el mapeo de pines editando un
solo lugar, sin tocar la lógica de control.

**Why this priority**: cada instalación tiene su propio mapeo. Hoy ese ajuste implica
editar arreglos paralelos dentro del código de control, que es donde se originan los
errores de puesta en marcha.

**Independent Test**: cambiar la configuración a una instalación de prueba con distinto
número de válvulas y distintas direcciones, compilar y verificar que el sistema opera con
el nuevo mapeo sin ningún otro cambio.

**Acceptance Scenarios**:

1. **Given** una finca con 24 válvulas configurada, **When** el responsable modifica la
   dirección del radio del bloque 3, **Then** el cambio se hace en un único archivo y el
   resto del código no requiere modificación.
2. **Given** una configuración con una entrada duplicada o un pin repetido, **When** se
   compila el firmware, **Then** el error se detecta y se reporta antes de llegar a campo.

---

### User Story 5 - Diagnosticar en sitio (Priority: P3)

El técnico llega a una instalación con una válvula reportada como problemática, conecta un
portátil al puerto USB del controlador y determina en minutos si el problema es de radio,
de entrada del tablero o de la válvula.

**Why this priority**: acorta la visita a campo, pero el sistema opera sin ello.

**Independent Test**: provocar un fallo de radio en banco y confirmar que la consola
identifica la válvula afectada, el número de intento y el motivo del fallo.

**Acceptance Scenarios**:

1. **Given** el controlador operando, **When** el técnico abre la consola serie, **Then**
   ve la versión del firmware, la causa del último reinicio y las transiciones de estado a
   medida que ocurren.
2. **Given** una válvula que agota sus reintentos, **When** el técnico revisa la consola,
   **Then** encuentra el índice de la válvula, el estado solicitado y el conteo acumulado
   de fallos de ese canal.

---

### User Story 6 - Conservar el legado (Priority: P3)

Un técnico necesita entender por qué una instalación antigua se comporta distinto y
consulta las versiones anteriores del firmware con su explicación, en el mismo
repositorio.

**Why this priority**: preserva conocimiento sin cargar mantenimiento sobre código que ya
no evoluciona.

**Independent Test**: localizar en el repositorio los sketches anteriores y su documento
de diferencias respecto al firmware vigente.

**Acceptance Scenarios**:

1. **Given** el repositorio publicado, **When** un técnico busca las versiones anteriores,
   **Then** encuentra los sketches archivados junto a una nota que explica qué instalación
   los usa y en qué se diferencian del firmware vigente.

---

### Edge Cases

- **El nodo remoto nunca responde.** La válvula agota sus reintentos rápidos, entra en
  reposo prolongado y el panel la marca como no confirmada. El resto del sistema sigue
  operando con normalidad.
- **Llega una confirmación de un nodo o identificador no configurado.** El controlador la
  descarta sin alterar ningún estado y registra el evento.
- **Llega una confirmación de un estado distinto al solicitado.** El controlador no da la
  válvula por confirmada y mantiene la secuencia de reintentos.
- **Llega una trama con checksum inválido o longitud insuficiente.** Se descarta antes de
  interpretarla; nunca se leen posiciones fuera del contenido recibido.
- **Una entrada del tablero oscila (rebote o falso contacto).** El controlador no debe
  emitir una orden por cada oscilación; el estado debe estabilizarse antes de transmitir.
- **Reinicio por watchdog con válvulas abiertas.** Al arrancar, el controlador reconstruye
  su estado desde las entradas físicas sin emitir una ráfaga de órdenes sobre la batería.
- **El contador de tiempo interno desborda** (ocurre aproximadamente cada 49 días de
  operación continua). Ninguna espera debe alargarse ni dispararse por ese desbordamiento.
- **El panel LCD está desconectado o falla su bus.** El controlador debe seguir operando
  las válvulas; la falla de la interfaz visual no puede detener el control.
- **Llegan confirmaciones mientras se está transmitiendo una orden.** Ninguna confirmación
  válida puede perderse por estar el controlador ocupado transmitiendo.

## Requirements *(mandatory)*

### Requisitos Funcionales — Control de válvulas

- **FR-001**: El sistema DEBE leer periódicamente las 24 entradas digitales del tablero y
  determinar, para cada una, el estado solicitado de su válvula asociada.
- **FR-002**: El sistema DEBE considerar estable un cambio de entrada solo tras
  mantenerse durante un tiempo de validación configurable, para no transmitir órdenes ante
  rebotes o falsos contactos.
- **FR-003**: El sistema DEBE transmitir una orden a la válvula únicamente cuando el
  estado solicitado difiere del último estado confirmado por el nodo remoto.
- **FR-004**: El sistema DEBE transmitir como máximo una orden de válvula a la vez, y
  DEBE respetar una ventana de exclusión configurable antes de transmitir la siguiente
  (valor heredado: 1,5 s).
- **FR-005**: El sistema DEBE dar por confirmado el estado de una válvula solo al recibir
  del nodo remoto una respuesta válida cuyo estado coincida con el solicitado.
- **FR-006**: El sistema DEBE reintentar una orden no confirmada hasta un máximo
  configurable de intentos (valor heredado: 5), separados por un intervalo corto
  configurable (valor heredado: 30 s).
- **FR-007**: El sistema DEBE, al agotar los reintentos, pasar la válvula a un periodo de
  reposo prolongado configurable (valor heredado: 60 min) antes de volver a intentar,
  para proteger la batería y los solenoides.
- **FR-008**: El sistema DEBE abandonar de inmediato el reposo prolongado y reiniciar el
  conteo de intentos cuando la entrada correspondiente cambia de estado.
- **FR-009**: El sistema DEBE mantener de forma independiente, por cada una de las 24
  válvulas, su estado solicitado, su estado confirmado, su conteo de intentos y su
  temporizador; el fallo de una válvula NO DEBE afectar el servicio de las demás.
- **FR-010**: El sistema DEBE reconstruir su estado al arrancar a partir de la lectura de
  las entradas físicas, sin emitir una ráfaga de órdenes.
- **FR-011**: El sistema NO DEBE bloquear el lazo de control mientras espera respuestas,
  temporizaciones o acceso a la interfaz visual.
- **FR-012**: El sistema DEBE reiniciarse por watchdog si el lazo de control deja de
  progresar (plazo heredado: 8 s), y DEBE refrescar el watchdog en un único punto del lazo.
- **FR-013**: Las temporizaciones DEBEN comportarse correctamente ante el desbordamiento
  del contador de tiempo interno del controlador.

### Requisitos Funcionales — Comunicación con los nodos

- **FR-014**: El sistema DEBE transmitir cada orden al nodo remoto identificándolo por su
  dirección de radio de 64 bits y el identificador de válvula dentro de ese nodo.
- **FR-015**: Cada orden transmitida DEBE incluir un dígito verificador que permita al
  nodo remoto descartar tramas corruptas, con el mismo cálculo que usa el firmware actual
  (compatibilidad con los nodos ya instalados en campo).
- **FR-016**: El sistema DEBE validar longitud, encabezado e identificadores de toda
  respuesta recibida antes de interpretar su contenido, y descartar sin efecto las que no
  correspondan a una válvula configurada.
- **FR-017**: El sistema DEBE atender la recepción de respuestas con suficiente frecuencia
  para no perder confirmaciones, incluso mientras hay órdenes pendientes de transmisión.
- **FR-018**: El sistema NO DEBE descartar datos ya recibidos del radio al preparar una
  transmisión. [NEEDS CLARIFICATION: el firmware actual vacía el buffer de entrada del
  radio justo antes de transmitir, lo que puede perder confirmaciones que ya habían
  llegado. ¿Se corrige o se conserva por compatibilidad con el comportamiento observado?]

### Requisitos Funcionales — Interfaz visual

- **FR-019**: El sistema DEBE mostrar en el panel el estado de las 24 válvulas
  organizadas en tres grupos de ocho, más una línea de identificación del sistema.
- **FR-020**: El sistema DEBE distinguir visualmente al menos tres condiciones por
  válvula: cerrada, abierta y orden en curso sin confirmar.
- **FR-021**: El panel DEBE representar el estado abierto y cerrado de forma coherente con
  lo que ve el operador en el tablero. [NEEDS CLARIFICATION: el firmware actual imprime el
  estado invertido respecto al valor confirmado. ¿Es intencional para coincidir con la
  convención del tablero, o es un defecto a corregir?]
- **FR-022**: Un fallo de la interfaz visual NO DEBE detener ni retrasar el control de
  válvulas.

### Requisitos Funcionales — Configuración

- **FR-023**: Todo el mapeo de la instalación (direcciones de radio, identificadores de
  válvula, pines de entrada) y todos los parámetros de tiempo DEBEN residir en un único
  punto de configuración, separado de la lógica de control.
- **FR-024**: El número de válvulas DEBE derivarse de la configuración y no declararse por
  separado; una configuración incompleta o inconsistente DEBE impedir la compilación.
- **FR-025**: La configuración DEBE detectar en compilación pines de entrada duplicados y
  combinaciones dirección/identificador duplicadas.

### Requisitos Funcionales — Diagnóstico

- **FR-026**: El sistema DEBE informar al arrancar su versión de firmware, fecha de
  compilación y causa del último reinicio.
- **FR-027**: El sistema DEBE registrar cada transmisión de orden con el índice de
  válvula, el estado solicitado y el número de intento, y cada confirmación o expiración
  recibida.
- **FR-028**: El sistema DEBE mantener y poder informar contadores acumulados de fallos de
  comunicación por válvula.
- **FR-029**: El diagnóstico DEBE poder desactivarse en compilación sin alterar el
  comportamiento del control.

### Requisitos Funcionales — Proyecto y trazabilidad

- **FR-030**: El proyecto DEBE compilarse y cargarse desde VS Code con PlatformIO en un
  equipo limpio, con dependencias declaradas en versiones fijadas y sin pasos manuales.
- **FR-031**: La lógica de control DEBE poder ejecutarse y verificarse en el equipo del
  desarrollador sin hardware conectado.
- **FR-032**: Toda regla de control descrita en FR-001 a FR-018 DEBE tener al menos una
  prueba automatizada que la verifique, identificada con el código del requisito.
- **FR-033**: El repositorio DEBE conservar los sketches anteriores como material
  archivado, con una nota que indique a qué instalación corresponden y en qué se
  diferencian del firmware vigente.
- **FR-034**: El repositorio DEBE incluir documentación de puesta en marcha: requisitos
  del entorno, procedimiento de compilación y carga, mapa de configuración y guía de
  diagnóstico.

### Requisitos No Funcionales

- **NFR-001**: El consumo de memoria RAM reportado en compilación DEBE mantenerse por
  debajo del 75 % de la disponible en la placa, para dejar margen de pila.
- **NFR-002**: El firmware DEBE operar de forma continua sin degradación durante al menos
  90 días, incluyendo el desbordamiento del contador de tiempo interno.
- **NFR-003**: El comportamiento eléctrico sobre la batería NO DEBE empeorar respecto al
  firmware actual: el número de disparos de solenoide por unidad de tiempo ante un fallo
  persistente de radio DEBE ser igual o menor.

### Key Entities

- **Válvula**: unidad de control del sistema. Se identifica por su posición en la
  instalación (índice 0 a 23) y agrupa: entrada del tablero asociada, nodo remoto y
  ubicación dentro de ese nodo, estado solicitado, estado confirmado, intentos realizados
  y momento del próximo intento permitido.
- **Nodo remoto**: dispositivo de campo que acciona hasta ocho válvulas latch. Se
  identifica por su dirección de radio única.
- **Orden**: instrucción de apertura o cierre dirigida a una válvula de un nodo, protegida
  por dígito verificador.
- **Confirmación**: respuesta del nodo remoto que declara el estado alcanzado por una de
  sus válvulas. Es la única fuente de verdad sobre el estado real.
- **Mapa de instalación**: conjunto de asociaciones entrada del tablero ↔ nodo ↔ válvula
  que define una finca concreta.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: El 100 % de las 24 entradas del tablero produce el cambio esperado en su
  válvula asociada durante la prueba de aceptación en banco.
- **SC-002**: Un técnico que nunca ha visto el proyecto compila y carga el firmware en
  menos de 15 minutos partiendo de un equipo con VS Code recién instalado.
- **SC-003**: La suite de verificación de la lógica de control se ejecuta completa en
  menos de 30 segundos sin ningún hardware conectado.
- **SC-004**: Poner en marcha una instalación nueva requiere modificar exactamente un
  archivo de configuración y ningún archivo de lógica.
- **SC-005**: Ante un fallo persistente de radio en una válvula, el sistema realiza como
  máximo 5 disparos en la primera media hora y no más de 1 disparo por hora después,
  mientras las 23 válvulas restantes siguen operando sin retraso perceptible.
- **SC-006**: El controlador opera 90 días continuos sin reinicio no explicado ni pérdida
  de sincronía entre tablero y campo.
- **SC-007**: Un técnico identifica la causa de una válvula problemática (radio, entrada o
  válvula) en menos de 10 minutos usando únicamente la consola de diagnóstico.
- **SC-008**: Cada requisito funcional de control es rastreable hasta el módulo que lo
  implementa y la prueba que lo verifica, sin excepciones.

## Assumptions

- La placa objetivo es Arduino Mega 2560 y se conserva, según la constitución del
  proyecto. No se contempla cambio de arquitectura en esta funcionalidad.
- Los nodos remotos ya instalados en campo NO se modifican. El protocolo de aplicación
  entre controlador y nodo se conserva byte a byte para mantener compatibilidad.
- El sketch `CABLE_VIRTUAL___LATCH_V1.ino` es la línea base funcional. `WIGA_24IN_LCD` y
  `WIGA_24IN_LCDB1_5` se archivan sin migrar.
- Los valores de tiempo heredados (1,5 s de ventana de disparo, 30 s entre reintentos, 5
  intentos, 60 min de reposo, watchdog de 8 s) se toman como correctos y se preservan como
  valores por defecto configurables.
- El estado seguro de la instalación es la válvula cerrada.
- Las entradas del tablero se leen con resistencia de pull-up interna y el nivel bajo
  representa la solicitud de apertura, tal como en el firmware actual.
- El tiempo de validación antibote (FR-002) no existe explícitamente en el firmware base;
  se adopta un valor por defecto conservador y se ajusta en banco.
- El diagnóstico por consola es de solo lectura; no se contempla un intérprete de comandos
  en esta funcionalidad.

## Out of Scope

- Cambio de arquitectura de hardware (ESP32, conectividad WiFi o celular).
- Telemetría remota, servidor central o interfaz web.
- Modificación del firmware de los nodos remotos.
- Programación horaria o automatización de riego dentro del controlador: el tablero sigue
  siendo la fuente de decisión.
- Migración de los sketches `WIGA_24IN_LCD` y `WIGA_24IN_LCDB1_5`.
- Actualización de firmware por aire (OTA).

## Dependencies

- Disponibilidad de un Arduino Mega 2560, un módulo XBee y al menos un nodo remoto para la
  prueba de aceptación en banco.
- Confirmación del mapa de instalación vigente (direcciones de radio e identificadores) de
  al menos una finca en producción, para validar la configuración migrada.
- Acceso al repositorio remoto en GitHub para publicar el resultado.
