# WIGA Cable Virtual

Firmware de control de 24 válvulas latch de riego para Arduino Mega 2560. Lee 24 entradas
de un tablero PLC y publica el estado pedido como **esclavo Modbus RTU** por RS485. Un
puente Milesight **UC100-915M** (maestro Modbus + controlador D2D) lee esos registros y
emite por radio LoRa directo a los agentes **UC501** que accionan las válvulas. Para el
operador se comporta como si un cable físico conectara el tablero con las válvulas.

El controlador **nunca inicia una transacción ni emite órdenes de válvula**: solo pública
en registros lo que el tablero pide y contesta cuando el UC100 le pregunta. No existe
confirmación del campo (el protocolo D2D no acusa recibo).

## Stack

- Lenguaje: C++14 (`-std=gnu++14`)
- Placa: Arduino Mega 2560 (ATmega2560, 8 KB SRAM, 256 KB Flash)
- Framework: Arduino AVR via PlatformIO
- Comunicación: **Modbus RTU esclavo** a 9600 8N1, serial 2 (TXD2 pin 16, RXD2 pin 17),
  transceptor MAX485 con DE/RE en el **pin 6**
- Pantalla: LCD I2C 20×4 (HD44780 + PCF8574)
- Tests: Unity sobre PlatformIO, entorno `native` para pruebas en host

## Comandos

- `python -m platformio run -e megaatmega2560` — compila para producción
- `python -m platformio run -e megaatmega2560_silent` — compila sin trazas de diagnóstico
- `python -m platformio run -e megaatmega2560 -t upload` — carga en la placa
- `python -m platformio test -e native` — ejecuta todas las pruebas en host (deben pasar
  antes de cada commit)
- `python -m platformio test -e native -f test_modbus` — ejecuta una suite específica
- `python -m platformio device monitor -b 9600` — consola de diagnóstico a 9600 baudios

> En esta máquina `pio` **no está en el PATH**: usa siempre `python -m platformio`.

## Estructura del proyecto

- `src/main.cpp` — raíz de composición. Crea adaptadores, inyecta puertos y ejecuta el
  lazo. Sin lógica de negocio.
- `lib/wiga_config/` — mapa de finca y tiempos. Lo único que se edita por instalación.
- `lib/wiga_domain/` — tipos puros (`SignalLevel`, `ValveState`). Sin hardware.
- `lib/wiga_modbus/` — capa **pura**: esclavo Modbus, CRC16, banco de registros. Sin
  `<Arduino.h>`.
- `lib/wiga_control/` — debouncer, `ValvePublisher`. Sin hardware.
- `lib/wiga_ports/` — interfaces abstractas: `IClock`, `IByteStream`, `IDisplayPort`,
  `IWatchdogPort`, `ILogPort`, `IDigitalInputPort`. Solo cabeceras.
- `lib/wiga_hal/` — adaptadores concretos: `Rs485StreamAdapter`, LCD, reloj, watchdog,
  serial. Única capa que toca hardware.
- `test/mocks/` — dobles de prueba: `FakeClock`, `FakeByteStream`, `FakeDisplay`,
  `FakeInputs`, `FakeWatchdog`, `FakeLog`.
- `test/test_modbus/` — CRC, tramado, excepciones, direccionamiento, regresión de los
  defectos auditados.
- `test/test_domain/` — tipos, configuración, mapa.
- `test/test_control/` — debouncer, overflow de tiempo.
- `test/test_integration/` — del pin físico a la trama en el bus.
- `docs/` — puesta en marcha, configuración de finca, diagnóstico, arquitectura,
  firmware anterior, acta de pruebas, estándar y playbook.
- `specs/001-migracion-platformio/` — migración a PlatformIO (XBee, superada por 002).
- `specs/002-modbus-esclavo-rs485/` — **esclavo Modbus (vigente)**: spec, contratos,
  quickstart, tasks.
- `.specify/memory/constitution.md` — constitución del proyecto (7 principios).
- `.github/workflows/ci.yml` — integración continua: pruebas, compilación, barrera de
  capas, trazabilidad.

## Convenciones

- **Nomenclatura de tipos:** `PascalCase` — `ValveState`, `RegisterFile`, `SignalLevel`.
- **Nomenclatura de funciones:** `camelCase` — `panelRequest()`, `valveRegister()`.
- **Variables miembro:** `camelCase` con `_` al final — `lastServed_`, `stable_`.
- **Constantes:** `kPascalCase` — `kValveCount`, `kModbusSlaveId`.
- **Macros:** `WIGA_MAYUSCULAS` — `WIGA_LOG_LEVEL`, `WIGA_HOST_TEST`.
- **Archivos:** `snake_case` — `modbus_slave.h`, `valve_publisher.cpp`.
- **Indentación:** 4 espacios. Llaves estilo K&R.
- **Comentarios:** explican el porqué, no el qué. En español.
- **Cadenas literales:** siempre con `F()` en código AVR para ahorrar SRAM.
- **Trazabilidad:** todo módulo lleva anotación `FR-xxx` del requisito que cubre. Toda
  prueba lleva el `FR-xxx` en su nombre. Verificación por `.github/scripts/check_traceability.py`.
- **Tests:** nombres descriptivos en español, una función por escenario, framework Unity.
- **Configuración:** parámetros de instalación en `lib/wiga_config/installation_map.h` y
  parámetros de tiempo/bus en `lib/wiga_config/timing_config.h`.
- **Slave ID del controlador:** `kModbusSlaveId = 1`. La dirección de registro de una
  válvula **coincide con su índice** y se deriva con `valveRegister(index)`.
- **Interfaz de transporte:** usar `IByteStream` (no la vieja `IRadioPort` ni
  `IModbusPort`). El adaptador concreto es `Rs485StreamAdapter` (Serial2 + MAX485, pin 6).

## No hagas

- No agregar `delay()` en el lazo de control. Usa máquinas de estados con `millis()` y
  `wiga::elapsed()`. La temporización debe ser no bloqueante, presupuesto ≤ 2 ms por vuelta.
- No incluir `<Arduino.h>`, `<Wire.h>` ni `<avr/wdt.h>` en `lib/wiga_modbus/`,
  `lib/wiga_control/`, `lib/wiga_domain/` o `lib/wiga_config/`. Rompe las pruebas en host.
  La barrera de capas es estructural, no una convención.
- No hacer que **el esclavo inicie una transmisión que no sea respuesta a una petición
  válida** (regla MB-07 del contrato). Sin excepciones: el antiguo saludo del entorno
  `_banco` se retiró por eso.
- No usar `Serial1.flush()` en la respuesta. Para la respuesta mayor bloquearía 55,2 ms;
  el adaptador entrega los bytes y libera la línea en vueltas posteriores.
- No hardcodear pines, slave IDs ni direcciones de registro fuera de `lib/wiga_config/`.
- No "corregir" la lectura del tablero pensando que es una inversión por transistor. La
  conversión del nivel eléctrico a estado pedido ocurre en un único punto
  (`config::panelRequest`) según el cableado del tablero. Es cosa distinta de la etapa de
  potencia del nodo, que no interviene aquí.
- No permitir que el registro de una válvula presente un `0` transitorio mientras el riego
  siga pedido: un solo sondeo que lea `0` cierra la válvula en campo (FR-116).
- No crear memoria dinámica (`new`, `malloc`, `String`, STL con heap). Todo el estado es
  estático y dimensionado en compilación.
- No olvidar `F()` en cadenas literales AVR. Cada byte de SRAM cuenta (8 KB total, límite
  75%).
- No tocar los sketches originales en las carpetas hermanas (`CABLE_VIRTUAL___LATCH_V1/`,
  `WIGA_24IN_LCD/`, `WIGA_24IN_LCDB1_5/`). Son legado archivado.
- No subir `.env*`, credenciales o claves al repositorio.
- No instalar dependencias nuevas sin verificar que ya se usan en el proyecto o que son
  estrictamente necesarias.
- No modificar la lógica de control sin una especificación asociada en `specs/`.

## Flujo de trabajo

- Antes de una tarea no trivial, propón un plan y espera mi OK.
- Toda funcionalidad nueva sigue el ciclo Spec Kit: `/speckit-specify` →
  `/speckit-clarify` → `/speckit-plan` → `/speckit-tasks` → `/speckit-implement`.
- Una rama `NNN-nombre-corto` por especificación. Prohibido comprometer lógica de control
  directamente en `main`.
- Antes de cada commit, ejecuta `python -m platformio test -e native` y verifica que pasa.
- Antes de cada PR, ejecuta `python -m platformio run -e megaatmega2560` y verifica que
  compila sin errores.
- Reporta el uso de SRAM y Flash en cada commit o PR.
- Una tarea a la vez; al terminar, dime qué cambiaste para que lo revise.
- Si no estás seguro al 80%, pregunta. No inventes.
- Commits referencian la especificación: `feat(modbus): FR-109 — el puente lee el estado`.

## Documentación

| Documento | Ubicación |
|---|---|
| Constitución del proyecto | `.specify/memory/constitution.md` |
| **Especificación 002 (vigente)** | `specs/002-modbus-esclavo-rs485/spec.md` |
| Contrato de registros Modbus | `specs/002-modbus-esclavo-rs485/contracts/register-map.md` |
| Contrato del bus (esclavo) | `specs/002-modbus-esclavo-rs485/contracts/modbus-slave.md` |
| Validación (3 niveles) | `specs/002-modbus-esclavo-rs485/quickstart.md` |
| Tareas descompuestas | `specs/002-modbus-esclavo-rs485/tasks.md` |
| Especificación 001 (XBee, superada) | `specs/001-migracion-platformio/spec.md` |
| Arquitectura del firmware | `docs/ARQUITECTURA_DEL_FIRMWARE.md` |
| Configuración de finca nueva | `docs/configuracion-finca.md` |
| Puesta en marcha | `docs/puesta-en-marcha.md` |
| Diagnóstico en campo | `docs/diagnostico.md` |
| Firmware anterior (legado) | `docs/firmware-anterior.md` |
| Acta de pruebas de banco | `docs/acta-pruebas-banco.md` |
| Estándar de desarrollo firmware | `docs/ESTANDAR_DESARROLLO_FIRMWARE.md` |
| Playbook de desarrollo firmware | `docs/PLAYBOOK_DESARROLLO_FIRMWARE.md` |
| Plantilla de nueva spec | `docs/PLANTILLA_NUEVA_SPEC.md` |
| CI (GitHub Actions) | `.github/workflows/ci.yml` |
