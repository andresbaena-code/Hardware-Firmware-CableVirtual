# Feature Specification: El controlador de riego como esclavo Modbus RTU

**Feature Branch**: `002-modbus-esclavo-rs485`

**Created**: 2026-08-27

**Status**: Done (código completo y verificado en hardware, 2026-09-03)

**Input**: Corrección de premisa sobre el rol Modbus del controlador y sobre la
topología real de la instalación. La rama `001-migracion-platformio` implementó el
controlador como Modbus **maestro** que sondea nodos remotos. Es falso en los dos
extremos: el controlador es **esclavo**, y los nodos remotos no están en el bus.

**Cierre**: T001–T063 completados y T064 verificado en banco con hardware físico
(Mega 2560 + MAX485 + convertidor USB-RS485 + Docklight). Las tramas de referencia
respondieron tal cual, incluido el escenario B-08 (una hora de sondeo continuo con
cero lecturas de `0`). Ver `docs/acta-pruebas-banco.md`.

---

## Contexto: por qué existe esta especificación

La migración del transporte XBee a Modbus se implementó sin pasar por el ciclo de
especificación, en contra del Principio VII. Se construyó sobre una premisa invertida
que quedó escrita como un hecho en el código:

> `lib/wiga_ports/i_modbus_port.h:4` — *"El controlador es Modbus Master (cliente).
> Los nodos remotos son Slaves."*

Una auditoría encontró 81 defectos, 25 concentrados en el adaptador Modbus. **No son
defectos independientes: son consecuencias de implementar un motor de transacciones de
maestro donde el sistema necesita un esclavo.**

---

## Topología real de la instalación

Confirmada por el responsable técnico y contrastada con la documentación de Milesight.

```
[ Controlador de riego ]        [ UC100 ]              [ UC501 ]
    ATmega2560                 D2D Controller          D2D Agent
    ESCLAVO Modbus             MAESTRO Modbus          receptor D2D
         |                           |                       |
         |  (1) mantiene el registro |                       |
         |      de cada valvula      |                       |
         |                           |                       |
         |<---- (2) sondeo FC03 -----|                       |
         |----- (3) responde ------->|                       |
         |                           |                       |
         |                           |-- (4) regla IF-THEN   |
         |                           |   dispara D2D 2 bytes>|
         |                           |   LoRa directo,       |
         |                           |   SIN gateway, <1 s   |
         |                           |                       |-- (5) activa GPIO
         |                           |                       |   -> rele -> valvula
```

**El alcance de esta especificación es exclusivamente la etapa 1-3: el firmware del
ATmega2560.** El UC100 y el UC501 son equipos comprados, con comportamiento fijo y
configurable pero no modificable. El firmware no los programa: se limita a hablar el
protocolo que el UC100 espera encontrar en el bus.

### Consecuencia decisiva: el sistema es de lazo abierto

El protocolo Milesight D2D **no tiene confirmación de recepción**, por diseño y no por
omisión. Es un esquema unidireccional de difusión: si cada receptor respondiera con un
acuse, se producirían colisiones de radio al accionar varios agentes con un mismo
disparo; además el acuse añadiría latencia y consumo a nodos alimentados por batería.

De modo que **ninguna confirmación del estado real puede llegar nunca al controlador**.
No es un problema a resolver en el firmware: es una propiedad del medio. La seguridad
ante una orden perdida se obtiene por otras dos vías, y solo la segunda es firmware:

1. **Auto-apagado en el UC501** (modo pulso o temporizador local). Fuera de alcance.
2. **Re-aserción periódica desde el controlador**, para que el UC100 vuelva a disparar
   el comando D2D durante la ventana de riego y un receptor que perdió el primer
   mensaje capture uno posterior. **Esto sí es firmware y está en alcance.**

---

## User Scenarios & Testing *(mandatory)*

### User Story 1 - El puente lee lo que pide el tablero (Priority: P1)

El técnico conecta el controlador al puerto RS485 del UC100 y lo configura para sondear
un canal por válvula. El UC100 obtiene en cada sondeo el estado que los 24 interruptores
del tablero piden, y sus reglas locales disparan los comandos D2D correspondientes. El
operador gira un interruptor y, en menos de un segundo tras el siguiente sondeo, la
válvula del campo se mueve.

**Why this priority**: Es la única vía por la que la intención del operador sale del
tablero. Es también lo que hoy está roto de raíz: el controlador nunca contesta a un
sondeo porque cree que le toca preguntar a él.

**Independent Test**: Con un maestro Modbus de escritorio se sondea el controlador y se
comprueba que responde tramas válidas que reflejan la posición física de los 24
interruptores. No requiere UC100, ni UC501, ni radio.

**Acceptance Scenarios**:

1. **Given** el controlador energizado con los 24 interruptores en posición cerrada,
   **When** el maestro lee el bloque de registros de válvula, **Then** responde una
   trama bien formada, con verificador correcto, con las 24 válvulas en cerrada.
2. **Given** el controlador en régimen, **When** el operador cambia un interruptor y
   transcurre la ventana de antirrebote, **Then** el siguiente sondeo refleja el nuevo
   estado para esa válvula y solo para esa.
3. **Given** el maestro dirige una petición a un identificador de esclavo distinto del
   propio, **When** la trama llega al controlador, **Then** guarda silencio absoluto y
   no altera su estado interno.
4. **Given** llega una trama con verificador incorrecto, **When** el controlador la
   evalúa, **Then** la descarta en silencio, incrementa su contador de diagnóstico y no
   responde.
5. **Given** el maestro pide un bloque que excede las válvulas configuradas, **When** el
   controlador lo evalúa, **Then** responde con el código de excepción reglamentario y
   no con datos inventados.

---

### User Story 2 - La orden se re-afirma para sobrevivir a una pérdida de radio (Priority: P2)

Una válvula lleva veinte minutos regando. Un obstáculo temporal atenúa la señal LoRa
justo cuando el UC100 dispara. El comando se pierde y la válvula no reacciona. Sin
ninguna acción adicional el riego de ese sector no ocurriría y nadie se enteraría.

**Why this priority**: Es la única mitigación que el firmware puede aportar frente a un
medio sin acuse. Depende de la Historia 1, por eso es P2.

**Independent Test**: Se muestrea el registro de una válvula en riego sostenido a lo
largo de miles de sondeos consecutivos y se comprueba que **nunca** se lee un `0`. La
repetición del comando de radio la produce el puente al reevaluar su regla; el papel
del controlador es no romper esa condición.

**Acceptance Scenarios**:

1. **Given** una válvula pidiendo riego de forma sostenida, **When** el maestro la
   sondea repetidamente durante toda la ventana de riego, **Then** lee `1` en **todos**
   los sondeos, sin una sola excepción.
2. **Given** el controlador se reinicia por watchdog mientras una válvula riega,
   **When** vuelve a responder, **Then** publica de nuevo `1` para esa válvula, porque
   reconstruye el estado desde el interruptor físico del tablero.
3. **Given** una válvula en reposo, **When** transcurren horas de sondeo, **Then** su
   registro permanece estable en `0`.
4. **Given** el contador de tiempo del controlador desborda a los 49,7 días, **When**
   una válvula está regando en ese instante, **Then** su registro no parpadea ni
   cambia de valor.

---

### User Story 3 - El protocolo se verifica sin hardware (Priority: P3)

El desarrollador ejecuta la suite en su equipo y obtiene verificación real del tramado,
del verificador de redundancia, del direccionamiento y del análisis de peticiones, sin
conectar una placa ni un transceptor.

**Why this priority**: Es la causa raíz de que los 25 defectos entraran sin que nadie lo
notara. Toda la lógica de protocolo vive hoy en una carpeta que el entorno de pruebas
excluye, de modo que las 73 pruebas en verde no ejercitan ni una línea de ella.

**Independent Test**: La suite de escritorio construye y analiza tramas completas,
incluidas malformadas, y falla si se reintroduce cualquiera de los defectos catalogados.

**Acceptance Scenarios**:

1. **Given** la suite de escritorio, **When** se ejecuta sin hardware, **Then** ejercita
   tramado, verificador y direccionamiento, y todos los casos pasan.
2. **Given** se alimenta al analizador una trama cuyo campo de longitud declara más
   datos de los recibidos, **When** el controlador la procesa, **Then** la rechaza sin
   leer fuera de su memoria y sin reiniciarse.
3. **Given** se reintroduce deliberadamente cualquier defecto catalogado, **When** se
   ejecuta la suite, **Then** al menos un caso falla.

---

### Edge Cases

- **Trama troceada en el tiempo.** Debe ensamblarse usando el silencio entre tramas, no
  un tiempo fijo de espera.
- **Silencio insuficiente entre dos peticiones.** Deben tratarse como trama inválida, no
  como una orden a medias.
- **Ruido en la línea.** No debe interpretarse como petición, ni acumularse hasta
  desbordar memoria, ni dejar al receptor sordo a la siguiente petición legítima.
- **Campo de longitud mentiroso.** *(Defecto verificado: hoy un valor de 251 hace que la
  comprobación dé cero, la trama pase y se lean 254 bytes de un buffer de 32.)*
- **Colisión en el bus.** El controlador no debe tomar la línea mientras el maestro
  transmite, ni soltarla antes de terminar su propia respuesta.
- **Reinicio por watchdog.** Al volver, reconstruye su estado desde los interruptores
  físicos y responde al siguiente sondeo. Como no emite órdenes, no hay ráfaga posible.
- **Desbordamiento del reloj a los 49,7 días.** Ni el ensamblado de tramas, ni el
  silencio entre ellas, ni la re-aserción pueden quedarse colgados.
- **Sondeo muy rápido.** El maestro puede sondear cada 10 ms (su mínimo configurable).
  Responder no puede robarle tiempo al lazo de control ni al antirrebote.
- **Ausencia prolongada de maestro.** Si el UC100 se apaga o se corta el RS485, el
  controlador debe seguir funcionando y señalarlo, sin bloquearse esperando peticiones.

---

## Requirements *(mandatory)*

> Numeración desde **FR-101** para no colisionar con los `FR-001..FR-035` de la
> especificación 001, que siguen vigentes salvo los relativos al transporte XBee.

### Rol y tramado

- **FR-101**: El controlador DEBE comportarse como esclavo Modbus RTU estándar sobre
  RS485: nunca inicia una transacción, solo responde a peticiones dirigidas a su
  identificador.
- **FR-102**: El identificador de esclavo propio DEBE declararse en el módulo de
  configuración con validación de rango (1..247) en compilación. *(El puente no impone
  ninguno: es un campo por canal que se configura en el UC100 para que coincida. Se
  adopta **1** por convención.)*
- **FR-103**: El controlador DEBE ignorar en silencio toda petición dirigida a otro
  identificador, sin responder y sin alterar su estado.
- **FR-104**: El controlador DEBE validar el verificador de redundancia de cada trama y
  descartar en silencio las que no lo superen.
- **FR-105**: El controlador DEBE delimitar las tramas por el silencio reglamentario
  entre ellas, reensamblando peticiones fragmentadas.
- **FR-106**: El controlador DEBE responder con el código de excepción reglamentario
  ante función no soportada o dirección fuera de rango, en vez de callar o inventar.
- **FR-107**: El controlador DEBE rechazar toda trama cuyo campo de longitud sea
  incoherente con los datos recibidos, sin leer ni escribir fuera de sus buffers.
- **FR-108**: El controlador DEBE gobernar la dirección del transceptor RS485 de forma
  que nunca transmita mientras el maestro transmite, ni corte su propia respuesta.

### Estado publicado

- **FR-109**: El controlador DEBE exponer **un registro por válvula**, de modo que el
  maestro pueda configurar una condición IF-THEN independiente para cada una.
  *(Empaquetar varias válvulas en un registro impediría las reglas por válvula, porque
  el UC100 evalúa el valor del canal contra un umbral.)*
- **FR-110**: El valor publicado DEBE ser un **nivel**, no un pulso ni un código de
  orden: `1` mientras el tablero pida riego para esa válvula, `0` mientras no lo pida.
  El controlador **NUNCA** devuelve el valor a `0` por iniciativa propia mientras el
  riego siga pedido. *(Decisión del responsable técnico, 2026-08-27. Con las reglas
  `IF valor==1 → abrir` e `IF valor==0 → cerrar` del puente, cualquier retorno
  transitorio a `0` cerraría la válvula y cortaría el riego.)*
- **FR-111**: La conversión entre nivel eléctrico del pin del tablero y estado físico
  DEBE ocurrir en un único punto del dominio. Ningún adaptador puede construir a mano el
  nivel a partir de un bit del protocolo. *(Invariantes V-01 y V-02.)*
- **FR-112**: La inversión por transistor DEBE conservarse en la lectura del tablero:
  nivel eléctrico alto equivale a válvula cerrada. Corregirla rompería las instalaciones.
- **FR-113**: El controlador DEBE exponer contadores de diagnóstico del bus (tramas
  descartadas por verificador, por longitud, por tiempo; peticiones atendidas) al
  maestro y a la consola de depuración.
- **FR-114**: El controlador DEBE señalar en el panel y en la consola la **ausencia
  prolongada de sondeo**, que indica UC100 apagado o cable RS485 cortado. Es el fallo
  más probable en campo y el único que el controlador puede detectar por sí solo.
- **FR-114b**: El panel DEBE mostrar únicamente **`0` y `1`**, con una etiqueta que deje
  claro que representa **lo que el tablero pide**, no un estado confirmado del campo.
  Quedan retirados los símbolos `?` y `!`, que sugerían fases de confirmación que ya no
  existen. *(Regla de honestidad de la interfaz del Principio II v2.0.0. Decisión del
  responsable técnico, 2026-08-27.)*

### Re-aserción periódica

- **FR-115**: El controlador DEBE sostener de forma **estable** el nivel de cada válvula
  activa, de modo que el maestro lo lea igual en cada sondeo y su regla local, mediante
  el parámetro *lockout time*, vuelva a emitir el comando de radio. Un receptor que
  perdió un mensaje captura así uno posterior. *(La repetición la produce el puente al
  reevaluar una condición que sigue cumpliéndose; el controlador la habilita
  manteniendo el nivel, no alternándolo.)*
- **FR-116**: El nivel publicado NO DEBE presentar ningún valor transitorio o
  intermedio mientras el riego siga pedido. Ni la re-aserción, ni un reinicio, ni el
  refresco del panel, ni el desbordamiento del contador de tiempo pueden producir un
  paso momentáneo por `0`. **Es el requisito de seguridad más importante de esta
  especificación**: un solo sondeo que lea `0` cierra la válvula en campo.
- **FR-117**: Todo parámetro temporal asociado a la publicación y a la vigilancia del
  bus DEBE vivir en el módulo de configuración temporal, con su unidad explícita.
  *(Principio V.)*

### Cobertura y mapa

- **FR-118**: Las **24** válvulas configuradas DEBEN ser observables por el maestro.
  Ningún límite interno puede dejar un subconjunto permanentemente invisible. *(Hoy 12
  de 24 lo están.)*
- **FR-119**: El mapa de instalación DEBE asociar cada válvula con su pin de entrada del
  tablero y con su dirección de registro Modbus, ambas validadas en compilación:
  sin pines duplicados, sin direcciones duplicadas, sin huecos no intencionados.
- **FR-120**: Los campos heredados de la instalación XBee DEBEN conservarse como
  referencia histórica documentada, sin participar en ningún direccionamiento.

### Determinismo y verificabilidad

- **FR-121**: El procesamiento del bus DEBE ser no bloqueante: ninguna vuelta del lazo
  puede quedar esperando actividad del bus. *(Principio I.)*
- **FR-122**: Atender una petición NO DEBE degradar el muestreo de los interruptores por
  debajo de lo que exige la ventana de antirrebote.
- **FR-123**: La lógica de tramado, verificador, direccionamiento y análisis de
  peticiones DEBE ser código puro, sin dependencia del entorno Arduino, y DEBE
  ejecutarse en la suite de escritorio. *(Principio IV.)*
- **FR-124**: La suite de escritorio DEBE incluir un caso por cada defecto catalogado en
  la auditoría, de modo que su reintroducción haga fallar la suite.
- **FR-125**: El código que quede huérfano tras esta migración DEBE retirarse del
  binario y de la suite. *(El protocolo XBee de 5 bytes y sus 13 pruebas siguen
  compilándose sin que nadie los invoque; la maquinaria de confirmación y reintento
  queda sin fuente de datos.)*

### Key Entities

- **Trama Modbus**: unidad de intercambio. Identificador de esclavo, función, dirección
  de inicio, cantidad, carga y verificador de redundancia.
- **Bloque de válvulas**: un registro por válvula con su estado físico solicitado. Lo
  escribe la capa de control; lo lee el maestro. Es la única salida del sistema.
- **Bloque de diagnóstico**: contadores de salud del bus y marca del último sondeo.
- **Fila del mapa de instalación**: asocia una válvula con su pin del tablero, su
  dirección de registro y sus campos históricos de la instalación XBee.

---

## Success Criteria *(mandatory)*

- **SC-101**: Un maestro Modbus de escritorio sondea el controlador durante una hora
  continua y obtiene respuesta válida en el **100 %** de los sondeos bien formados, sin
  una sola trama malformada ni un silencio indebido.
- **SC-102**: Las **24** válvulas son observables por el maestro. Ninguna queda
  estructuralmente excluida. *(Línea base actual: 12 de 24 son inalcanzables.)*
- **SC-103**: Girar un interruptor del tablero se refleja en el registro correspondiente
  antes del siguiente sondeo, para una cadencia de sondeo de un segundo.
- **SC-104**: Ninguna vuelta del lazo de control supera los **2 ms** de ocupación
  atribuible al transporte, medido en banco. *(Línea base actual: 480 a 805 ms.)*
- **SC-105**: Una válvula en riego sostenido devuelve `1` en el **100 %** de los sondeos
  durante toda la ventana de riego, incluidos un reinicio por watchdog y el
  desbordamiento del contador de tiempo. **Cero** lecturas de `0`. Un solo fallo de este
  criterio corta el riego en campo.
- **SC-106**: La suite de escritorio ejercita la lógica de protocolo y **falla** al
  reintroducir cualquiera de los defectos catalogados, comprobado uno a uno.
- **SC-107**: Ninguna trama recibida, por malformada que sea, provoca lectura o
  escritura fuera de los buffers, comprobado con un corpus de tramas aleatorias y de
  casos límite del campo de longitud.
- **SC-108**: El consumo de SRAM se mantiene por debajo del **75 %**. *(NFR-001.)*
- **SC-109**: El controlador opera **90 días** continuos sin degradación, atravesando el
  desbordamiento del contador de tiempo a los 49,7 días. *(NFR-002.)*

---

## Restricciones verificadas del puente UC100-915M

De la documentación oficial (*UC100 Datasheet* y *UC100 User Guide*, Milesight, 2025).
**En todos sus modos, el equipo conectado a su RS485 es un esclavo Modbus.** Esto
confirma la premisa de esta especificación y refuta la del código actual.

| Recurso | Límite | Consecuencia para el firmware |
|---|---|---|
| Canales Modbus | **32** | 24 válvulas caben con un registro por válvula |
| Reglas IF-THEN | **16** por equipo | Limita las válvulas por UC100, no el firmware |
| Acciones por regla | 3 | — |
| Intervalo entre comandos | 10..1000 ms | El sondeo puede ser tan rápido como 10 ms |
| Tiempo máximo de respuesta | 10..60000 ms | Holgado para un esclavo que responde de inmediato |
| Serie | 1200..115200, 8 bits, paridad y parada configurables | Compatible con los 9600 8N1 heredados |

- Tipos de canal admitidos: `00=Coil`, `01=Discrete`, Input/Holding de 16, 32 y 64 bits.
  Es decir, FC01, FC02, FC03 y FC04 están disponibles.
- El puente **no está preprovisionado**: se configura para coincidir con lo que exponga
  el esclavo, de modo que el diseño del mapa es libre dentro de estos límites.
- El parámetro **lockout time** de las reglas IF-THEN gobierna la reevaluación: *"after
  the lockout time, UC100 will check if the value still reaches the threshold and matches
  the condition. 0 means this condition will only be detected once."* Es un segundo
  mecanismo de repetición, configurable en el puente y complementario a FR-115.
- El protocolo **Milesight D2D opera sin gateway** y su comando es un número
  hexadecimal de 2 bytes, con clave compartida entre controlador y agente.

---

## Assumptions

- **Rol del puente.** El UC100 es el maestro Modbus del bus y el único que habla primero.
  Fija la cadencia de sondeo; el controlador no la controla ni la necesita conocer.
- **Un solo maestro en el bus.** No hay otros esclavos ni otros maestros compartiendo el
  RS485 con el controlador.
- **Parámetros del enlace.** Se mantienen los heredados: 9600 baudios, 8N1, sobre el
  segundo puerto serie, con transceptor MAX485 de dirección conmutada por un pin. A 9600
  8N1 el silencio reglamentario entre tramas es de aproximadamente **4,0 ms**.
- **La lectura del tablero no cambia.** Los 24 pines, el antirrebote y la inversión por
  transistor se conservan tal cual.
- **El panel LCD se conserva**, con su semántica adaptada a que ya no existen las fases
  de espera de confirmación.
- **La seguridad ante orden perdida vive en el UC501** (modo pulso o temporizador
  local). El firmware solo aporta la re-aserción periódica.
- **Equipos de terceros fijos.** UC100 y UC501 están comprados; su comportamiento se
  toma como dado y no se modifica.

---

## Dependencies

- **Configuración del UC100.** Modo Modbus Channels, un canal por válvula, identificador
  de esclavo coincidente, parámetros serie coincidentes, reglas IF-THEN con su lockout
  time. Es tarea de puesta en marcha, no bloquea el diseño del firmware.
- **Especificación 001.** Sus requisitos siguen vigentes salvo los que describen el
  transporte XBee y el rol de maestro, que esta especificación sustituye.

---

## Out of Scope

- El firmware y la configuración del UC100 y del UC501. Son equipos comprados.
- El dimensionamiento de cuántos UC100 y UC501 hacen falta, y el reparto de válvulas
  entre ellos.
- El tipo de electroválvula y su etapa de potencia en campo.
- El temporizador de seguridad del UC501.
- Reasignar los pines físicos del tablero.
- Corregir la inversión por transistor. *(Es correcta; ver FR-112.)*

---

## Conformidad con la constitución

El conflicto con el **Principio II** quedó **resuelto por enmienda formal**: la
constitución pasó a la versión **2.0.0** el 2026-08-27, con aprobación del responsable
técnico. La regla *"Confirmación obligatoria"* se eliminó por ser imposible de cumplir
con el medio de radio real, y se sustituyó por el régimen que esta especificación
implementa.

| Regla del Principio II v2.0.0 | Dónde la satisface esta especificación |
|---|---|
| Sin actuación simultánea masiva | El controlador solo publica estado; la separación entre accionamientos la impone el intervalo entre canales del puente |
| Re-aserción acotada | FR-115, FR-116, FR-117 |
| Seguridad sin acuse de recibo | FR-115 en el controlador; el auto-apagado del actuador queda fuera de alcance por decisión del responsable técnico |
| Honestidad de la interfaz | FR-114b |
| Sin ráfagas al reiniciar | Se cumple por construcción: el controlador no emite órdenes, solo publica, y con semántica de nivel la republicación tras un reinicio es idempotente |

El **Principio IV** también se reforzó en la enmienda, exigiendo de forma explícita que
la lógica de protocolo sea código puro comprobable en `native`. Lo satisfacen FR-123 y
FR-124.

---

## Requisitos derogados de la especificación 001

Esta especificación **deroga** los requisitos siguientes. Todos describen el transporte
XBee o el ciclo de confirmación y reintento, que la topología real no puede sostener: el
protocolo Milesight D2D no acusa recibo, de modo que ninguna confirmación puede llegar
jamás al controlador.

Sus módulos se retiraron del binario y de la suite. Se listan aquí de forma explícita
para que el verificador de trazabilidad no los exija y para dejar constancia de qué se
retiró y por qué.

- **FR-003** — cambio de estado solicitado tras el antirrebote *(sustituido por FR-109)*
- **FR-004** — una orden de válvula a la vez *(el controlador ya no emite órdenes)*
- **FR-005** — estado real solo por confirmación del nodo *(imposible: D2D no acusa)*
- **FR-006** — expiración de la espera de confirmación *(no hay nada que esperar)*
- **FR-007** — reposo prolongado tras agotar reintentos *(no hay reintentos)*
- **FR-008** — reintentos rápidos disponibles de nuevo *(idem)*
- **FR-009** — fases de la máquina de estados de válvula *(reducida a dos estados)*
- **FR-011** — cadencia del lazo sin bloqueo *(sustituido por FR-121)*
- **FR-014** — formato de la orden saliente XBee *(sustituido por el mapa de registros)*
- **FR-015** — verificador del payload de 5 bytes *(sustituido por el CRC16 Modbus)*
- **FR-016** — decodificación de la confirmación entrante *(no hay confirmación)*
- **FR-017** — validación de la trama recibida *(sustituido por FR-104 y FR-107)*
- **FR-018** — drenar el búfer de recepción sin perder tramas *(sustituido por FR-105)*

Los demás requisitos de la especificación 001 **siguen vigentes**, en particular los de
lectura del tablero, antirrebote, panel, watchdog, diagnóstico y presupuesto de memoria.

---

## Deuda de proceso detectada

- La constitución declara **XBee ZigBee en modo API** como medio de comunicación. Queda
  desactualizada y requiere la misma enmienda.
- El verificador de trazabilidad (`.github/scripts/check_traceability.py`) solo lee
  `specs/001-migracion-platformio/spec.md`. Los `FR-101+` **no quedan cubiertos por la
  puerta de CI** hasta que se extienda.
- El CI ejecuta únicamente `pio test -e native_posix`; el entorno `native`, con el que se
  prueba en Windows, nunca se ejercita en integración continua.
