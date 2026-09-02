# Arquitectura del Firmware — WIGA Cable Virtual

Documento de referencia para entender cómo funciona el sistema completo.
Para configurar una finca, ver [configuracion-finca.md](configuracion-finca.md).
Para poner en marcha, ver [puesta-en-marcha.md](puesta-en-marcha.md).

Especificación vigente: [`specs/002-modbus-esclavo-rs485`](../specs/002-modbus-esclavo-rs485/spec.md).
Contrato del bus: [`contracts/register-map.md`](../specs/002-modbus-esclavo-rs485/contracts/register-map.md).

---

## 1. Vista general del sistema

```
┌─────────────────────────────────────────────────────────────────────┐
│                        PANEL FÍSICO (tablero)                       │
│                                                                     │
│  Interruptor A0 ──┐                                                 │
│  Interruptor A1 ──┤                                                 │
│  ...              │  24 interruptores, uno por válvula              │
│  Interruptor A15 ─┘                                                 │
└────────────────────────────┬────────────────────────────────────────┘
                             │ pines digitales del Mega
                             ▼
┌─────────────────────────────────────────────────────────────────────┐
│                 ARDUINO MEGA 2560   ← ESCLAVO Modbus                │
│                                                                     │
│  ┌─────────────┐    ┌──────────────────┐    ┌──────────────────┐    │
│  │  Entradas   │───▶│  ValvePublisher  │───▶│  RegisterFile    │    │
│  │  + antirreb.│    │  (lee y publica) │    │  24 + 8 registros│    │
│  └─────────────┘    └────────┬─────────┘    └────────┬─────────┘    │
│                              │                       │              │
│                         ┌────▼─────┐         ┌───────▼──────────┐   │
│                         │   LCD    │         │  ModbusSlave     │   │
│                         │  (I2C)   │         │  (solo responde) │   │
│                         └──────────┘         └───────┬──────────┘   │
└──────────────────────────────────────────────────────┼──────────────┘
                                                       │ MAX485
                                                       │ RS485 9600 8N1
                                                       ▼
                                              ┌────────────────┐
                                              │    UC100       │
                                              │  MAESTRO Modbus│
                                              │  + controlador │
                                              │     D2D        │
                                              └────────┬───────┘
                                                       │ LoRa D2D directo
                                                       │ SIN GATEWAY
                              ┌────────────────────────┼───────────────┐
                              ▼                        ▼               ▼
                      ┌──────────────┐        ┌──────────────┐ ┌──────────────┐
                      │    UC501     │        │    UC501     │ │    UC501     │
                      │ agente D2D   │        │ agente D2D   │ │ agente D2D   │
                      │ GPIO → relé  │        │ GPIO → relé  │ │ GPIO → relé  │
                      │ → válvula    │        │ → válvula    │ │ → válvula    │
                      └──────────────┘        └──────────────┘ └──────────────┘
```

### Quién habla primero

| Equipo | Papel en el bus RS485 | Papel en la radio |
|---|---|---|
| **Arduino Mega** | **Esclavo.** Nunca inicia nada; solo responde | no participa |
| **UC100** | **Maestro.** Sondea al Mega cada pocos segundos | Controlador D2D: emite las órdenes |
| **UC501** | no participa | Agente D2D: recibe y acciona la válvula |

> **El controlador no manda a las válvulas.** Publica en registros lo que el tablero pide.
> Es el UC100 quien lee esos registros y, con sus reglas IF-THEN locales, emite el comando
> de radio. Por eso el sistema funciona sin internet y sin gateway.

### El sistema es de lazo abierto, y eso es una propiedad del medio

El protocolo Milesight D2D **no tiene acuse de recibo**, por diseño: es difusión
unidireccional, para que un solo disparo pueda accionar varios agentes sin colisiones de
radio, con latencia baja y sin gastar batería en confirmar.

De modo que **ninguna confirmación del estado real llega jamás al controlador**. No es un
defecto a corregir. La seguridad se obtiene por dos vías independientes:

1. **Nivel sostenido en el controlador**, para que el UC100 vuelva a emitir el comando
   mientras el riego siga pedido, y un receptor que perdió un mensaje capture otro.
2. **Auto-apagado temporizado en el UC501**, que cierra la válvula por su cuenta si deja
   de recibir órdenes.

Ambas son obligatorias. Ninguna basta sola. *(Constitución v2.0.0, Principio II.)*

---

## 2. Arquitectura de capas

```
┌─────────────────────────────────────────────────────────────┐
│  APLICACIÓN (src/main.cpp)                                  │
│  Solo arma y ejecuta. NO contiene lógica de negocio.         │
└─────────────────────────────┬───────────────────────────────┘
                              │ inyección de dependencias
┌─────────────────────────────▼───────────────────────────────┐
│  CONTROL (lib/wiga_control/)                                │
│  ValvePublisher, Debouncer                                  │
│  Leer entradas, antirrebotar, publicar.                     │
├─────────────────────────────────────────────────────────────┤
│  PROTOCOLO (lib/wiga_modbus/)          ← capa PURA          │
│  ModbusSlave, RegisterFile, CRC16                           │
│  Tramado, verificador, direccionamiento, excepciones.       │
├─────────────────────────────────────────────────────────────┤
│  DOMINIO (lib/wiga_domain/)                                 │
│  ValveState, SignalLevel, time_math                         │
└─────────────────────────────┬───────────────────────────────┘
                              │ interfaces abstractas (lib/wiga_ports/)
┌─────────────────────────────▼───────────────────────────────┐
│  HARDWARE (lib/wiga_hal/)                                   │
│  Rs485StreamAdapter, LcdDisplayAdapter, SerialLog,          │
│  ArduinoClock, AvrWatchdog                                  │
│  Único lugar que incluye <Arduino.h>.                       │
└─────────────────────────────────────────────────────────────┘
```

**Regla de oro:** `wiga_control`, `wiga_modbus`, `wiga_domain` y `wiga_config` **nunca**
incluyen `<Arduino.h>`. Si lo hacen, las pruebas de escritorio fallan. Eso es a propósito,
y el CI lo comprueba rompiéndolo a propósito en cada push.

`wiga_modbus` está del lado puro **por una razón concreta**: la implementación anterior
metió el protocolo en `wiga_hal`, que el entorno de pruebas excluye, y acumuló 25 defectos
con las 73 pruebas del proyecto en verde. Ninguna lo tocaba.

---

## 3. Flujo de `main.cpp`

### 3.1 `setup()` — una vez al encender

```
setup()
  ├─ 1. g_watchdog.disable()      apagar para inicializar sin resets
  ├─ 2. g_log.begin()             consola a 9600
  │     g_bus.begin()             Serial2 a 9600 8N1 + MAX485 en RECEPCIÓN
  │     g_display.begin()         I2C al LCD
  ├─ 3. Log: versión, fecha, causa del último reinicio
  ├─ 4. g_publisher.begin()
  │     │
  │     └─ Lee los 24 pines y SIEMBRA el banco de registros.
  │        Ocurre AQUÍ, antes de que loop() atienda el bus: si el esclavo
  │        contestara antes, devolvería 0 para una válvula que sí riega y
  │        el UC100 la cerraría.
  └─ 5. g_watchdog.enable()       8 s; si el lazo cuelga, resetea
```

### 3.2 `loop()` — para siempre

```
loop()
  ├─ g_watchdog.feed()
  │
  ├─ serviceBus(now)              ← EN CADA VUELTA, sin excepción
  │   ├─ avanzar transmisión pendiente
  │   ├─ liberar la línea si ya terminó
  │   ├─ drenar bytes recibidos → ModbusSlave
  │   └─ si hay respuesta lista, entregarla al hardware
  │
  ├─ ¿pasaron 15 ms?  →  g_publisher.tick()   (leer y publicar)
  │
  ├─ ¿pasó 1 s?       →  serviceDiagnostics() (contadores y salud)
  │
  └─ g_display.tick()             una celda del LCD por vuelta
```

El bus se atiende en **cada** vuelta porque el UC100 puede sondear cada 10 ms y no hay que
hacerle esperar. **Ninguna de estas llamadas bloquea.**

### 3.3 Por qué la respuesta no bloquea el lazo

Una respuesta de 24 registros son 53 bytes. A 9600 baudios eso son **55 ms en el cable**,
veintisiete veces el límite de 2 ms del Principio I.

El adaptador **no espera**. Afirma DE, entrega los bytes al búfer del puerto, y libera la
línea en una vuelta posterior cuando el tiempo estimado ha pasado y el búfer se ha vaciado.
El cable tarda lo suyo; el lazo sigue girando.

Está prohibido `flush()` en este firmware. Era lo que hacía la implementación
anterior, con un presupuesto real de 480 a 805 ms por vuelta.

---

## 4. El banco de registros

Es la **única fuente de verdad** del estado pedido. Lo escribe el control, lo leen el
protocolo y el panel.

| Dirección | Contenido |
|---|---|
| `0` … `23` | Estado pedido de cada válvula: `1` riega, `0` no riega |
| `24` | Versión de firmware |
| `25` | Segundos desde la última petición válida |
| `26` | Tramas descartadas por verificador |
| `27` | Tramas descartadas por longitud o formato |
| `28` | Peticiones válidas atendidas |
| `29` | Causa del último reinicio |
| `30` | Número de válvulas regando |
| `31` | Reservado |

Detalle completo y ejemplos de trama en
[`contracts/register-map.md`](../specs/002-modbus-esclavo-rs485/contracts/register-map.md).

### Funciones Modbus soportadas

| Código | Nombre | Vista |
|---|---|---|
| `0x01` | Read Coils | bits |
| `0x02` | Read Discrete Inputs | bits |
| `0x03` | Read Holding Registers | registros |
| `0x04` | Read Input Registers | registros |

**Ninguna escritura.** FC05, FC06, FC15 y FC16 responden excepción `0x01`. No es una
carencia: garantiza que el tablero sea la única fuente de verdad y que ninguna trama del
bus pueda corromperla.

---

## 5. Estados de una válvula

Sin confirmación del campo, la máquina de cuatro fases se reduce a dos estados:

```
        ┌───────────────┐   cambio estable del interruptor   ┌───────────────┐
        │  NO RIEGA     │ ─────────────────────────────────▶ │    RIEGA      │
        │  registro 0   │ ◀───────────────────────────────── │  registro 1   │
        └───────────────┘   cambio estable del interruptor   └───────────────┘
```

| Transición | Qué la provoca |
|---|---|
| **T1** | Arranque o reinicio: se siembra desde los pines físicos, sin emitir nada |
| **T2** | El antirrebote confirma un nivel distinto durante 50 ms |

**Desaparecieron** las fases de espera de confirmación, los reintentos y el reposo de una
hora. No hay nada que esperar.

---

## 6. La regla que no se puede romper

> **El registro de una válvula que riega no puede presentar JAMÁS un `0` transitorio.**

Un solo sondeo que lea `0` cierra la válvula en campo. Por eso:

- El registro solo se escribe cuando el antirrebote confirma un nivel **distinto**.
- Nunca se pone a cero por iniciativa del firmware: solo lo hace el operador.
- Al arrancar se siembra **antes** de atender el bus.
- La aritmética de tiempo es modular, así que el desbordamiento a los 49,7 días no lo altera.

Y una distinción que importa: **no responder es seguro; responder un valor equivocado no lo
es.** Durante un reinicio el maestro ve una expiración, no un `0`, y no dispara cierre.
Ante la duda, el esclavo calla.

---

## 7. La inversión por transistor

**ATENCIÓN:** la etapa de potencia opera con lógica inversa por transistor.

| Nivel eléctrico en el pin | Estado de la válvula |
|---|---|
| HIGH (1) | CERRADA |
| LOW (0) | ABIERTA |

**Si alguien "corrige" esta inversión, rompe todas las instalaciones.**

La conversión ocurre en **un único punto**, `lib/wiga_domain/valve_types.h`, y lo que se
publica en el registro es el estado físico ya resuelto: `1` significa riega. Ningún
adaptador ni la capa de protocolo construyen ese valor a mano.

Hacerlo a mano y en dos sentidos distintos fue el defecto más grave de la implementación
anterior: el recorrido orden → confirmación devolvía siempre el estado contrario.

---

## 8. Archivos clave

### Configuración (lo único que se edita para una finca nueva)

| Archivo | Qué contiene |
|---|---|
| `lib/wiga_config/installation_map.h` | Mapa: válvula → pin del tablero. Validado al compilar |
| `lib/wiga_config/timing_config.h` | Identificador de esclavo, tiempos, geometría del panel |

### Protocolo (capa pura, comprobable sin hardware)

| Archivo | Qué contiene |
|---|---|
| `lib/wiga_modbus/modbus_slave.cpp` | Ensamblado de trama, análisis, excepciones |
| `lib/wiga_modbus/register_file.cpp` | El banco de registros, con acceso acotado |
| `lib/wiga_modbus/modbus_crc.cpp` | CRC16 del estándar |

### Control y hardware

| Archivo | Qué contiene |
|---|---|
| `lib/wiga_control/valve_publisher.cpp` | Leer el tablero, antirrebotar, publicar |
| `lib/wiga_control/debouncer.cpp` | Antirrebote de 50 ms |
| `lib/wiga_hal/rs485_stream_adapter.cpp` | `Serial2` + MAX485 en PH3, transmisión no bloqueante |
| `lib/wiga_hal/lcd_display_adapter.cpp` | Panel 20x4 por I2C |

### Pruebas

| Carpeta | Qué verifica |
|---|---|
| `test/test_modbus/` | CRC, tramado, excepciones y **regresión de los defectos auditados** |
| `test/test_integration/` | Del pin físico a la trama en el bus |
| `test/test_control/`, `test/test_domain/` | Antirrebote, tipos y aritmética temporal |

---

## 9. De girar un interruptor a que la válvula obedece

```
1. El operador sube el interruptor A3 (pin 57)
                    │
                    ▼
2. pollInputs() lee el pin 57 → LOW
   El antirrebote confirma: nivel estable durante 50 ms
                    │
                    ▼
3. toValveState(Low) → Open        ← único punto de conversión
   registerValueOf(Open) → 1
   RegisterFile[3] = 1
                    │
                    ▼
4. El UC100 sondea:  01 03 00 03 00 01 74 0A
   El Mega responde: 01 03 02 00 01 79 84
                    │
                    ▼
5. La regla IF-THEN del UC100 ve que el canal vale 1
   y emite por radio el comando D2D de apertura
                    │
                    ▼
6. El UC501 lo recibe, activa su GPIO, el relé cierra
   y la electroválvula abre
                    │
                    ▼
7. El panel LCD muestra '1' en la posición 3
   (lo que el tablero PIDE, no una confirmación del campo)
```

**Si el comando de radio se pierde**, el UC100 lo vuelve a emitir mientras el registro siga
valiendo `1`, gobernado por el *lockout time* de su regla. Y si aun así no llegara, el
temporizador del UC501 cierra la válvula por seguridad.

---

## 10. Diagnóstico en campo

Sin portátil, leyendo el bloque de diagnóstico con el ToolBox:

| Síntoma | Registro | Interpretación |
|---|---|---|
| Una válvula no riega | el suyo (0-23) | Si vale `1`, el controlador cumple: mirar puente, radio o actuador |
| Nada responde | `25` | Si crece sin parar, nadie está preguntando: UC100 apagado o cable cortado |
| Comportamiento errático | `26`, `27` | Si suben, hay ruido eléctrico o desajuste de parámetros serie |
| Reinicios inesperados | `29` | Distingue watchdog de corte de alimentación |

En el propio panel, la línea de estado indica `PEDIDO x TABLERO` en operación normal y
`PEDIDO - SIN BUS` cuando el maestro lleva más de 30 s sin preguntar.

Con portátil, por la consola a 9600 baudios: versión, fecha de compilación, causa del
último reinicio, cada cambio de válvula y la salud del panel.

> El **watchdog es de 8 segundos** (`WDTO_8S`). Una versión anterior de este documento
> decía 2 s, y no era cierto.
