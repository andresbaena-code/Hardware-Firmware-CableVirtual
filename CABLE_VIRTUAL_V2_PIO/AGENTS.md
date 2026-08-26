# WIGA Cable Virtual

Firmware de control de 24 válvulas latch de riego por radio XBee ZigBee para Arduino
Mega 2560. Lee 24 entradas de un tablero PLC y replica el estado de forma inalámbrica
sobre válvulas remotas. Para el operador se comporta como si un cable físico conectara
el tablero con las válvulas.

## Stack

- Lenguaje: C++14 (`-std=gnu++14`)
- Placa: Arduino Mega 2560 (ATmega2560, 8 KB SRAM, 256 KB Flash)
- Framework: Arduino AVR via PlatformIO
- Comunicación: XBee ZigBee API modo escapado (AP=2) a 9600 baudios
- Pantalla: LCD I2C 20×4 (HD44780 + PCF8574)
- Tests: Unity sobre PlatformIO, entorno `native` para pruebas en host

## Comandos

- `pio run -e megaatmega2560` — compila para producción
- `pio run -e megaatmega2560_silent` — compila sin trazas de diagnóstico (FR-029)
- `pio run -e megaatmega2560 -t upload` — carga en la placa
- `pio test -e native` — ejecuta todas las pruebas en host (deben pasar antes de cada commit)
- `pio test -e native -f test_debouncer` — ejecuta una suite de pruebas específica
- `pio device monitor -b 9600` — consola de diagnóstico a 9600 baudios

En Windows, si `pio` no está en el PATH:
`%USERPROFILE%\.platformio\penv\Scripts\pio.exe`

## Estructura del proyecto

- `src/main.cpp` — raíz de composición. Crea adaptadores, inyecta puertos y ejecuta el lazo. Sin lógica de negocio.
- `lib/wiga_config/` — mapa de finca y tiempos. Lo único que se edita por instalación.
- `lib/wiga_domain/` — tipos (`SignalLevel`, `ValveState`), protocolo XBee, `valve_record`. Sin hardware.
- `lib/wiga_control/` — debouncer, retry policy, latch scheduler, valve supervisor. Sin hardware.
- `lib/wiga_ports/` — interfaces abstractas: `IClock`, `IRadioPort`, `IDisplayPort`, `IWatchdogPort`, `ILogPort`, `IDigitalInputPort`. Solo cabeceras.
- `lib/wiga_hal/` — adaptadores concretos para Arduino, XBee, LCD, watchdog, serial. Única capa que toca hardware.
- `test/mocks/` — dobles de prueba: `FakeClock`, `FakeRadio`, `FakeDisplay`, `FakeInputs`, `FakeWatchdog`, `FakeLog`.
- `test/test_domain/` — pruebas de protocolo, tipos y configuración.
- `test/test_control/` — pruebas de debouncer, retry policy, scheduler, overflow de tiempo.
- `test/test_integration/` — supervisor completo contra mocks, aislamiento de fallos, arranque sin ráfagas.
- `docs/` — puesta en marcha, configuración de finca, diagnóstico, firmware anterior, acta de pruebas.
- `specs/001-migracion-platformio/` — especificación, investigación, plan, modelo de datos, contratos, quickstart, tareas.
- `.specify/memory/constitution.md` — constitución del proyecto (7 principios).
- `.github/workflows/ci.yml` — integración continua: pruebas, compilación, barrera de capas, trazabilidad.

## Convenciones

- **Nomenclatura de tipos:** `PascalCase` — `ValveState`, `RetryPolicy`, `SignalLevel`.
- **Nomenclatura de funciones:** `camelCase` — `onRequestChanged()`, `transmitirOrden()`.
- **Variables miembro:** `camelCase` con `_` al final — `lastServed_`, `stable_`.
- **Constantes:** `kPascalCase` — `kValveCount`, `kLoopPeriodMs`.
- **Macros:** `WIGA_MAYUSCULAS` — `WIGA_LOG_LEVEL`.
- **Archivos:** `snake_case` — `retry_policy.h`, `valve_supervisor.cpp`.
- **Indentación:** 4 espacios. Llaves estilo K&R.
- **Comentarios:** explican el porqué, no el qué. En español.
- **Cadenas literales:** siempre con `F()` en código AVR para ahorrar SRAM.
- **Trazabilidad:** todo módulo lleva anotación `FR-xxx` del requisito que cubre. Toda prueba lleva el `FR-xxx` en su nombre.
- **Tests:** nombres descriptivos en español, una función por escenario, framework Unity.
- **Configuración:** los parámetros de instalación van en `lib/wiga_config/installation_map.h` y los de tiempo en `lib/wiga_config/timing_config.h`.
- **Lógica inversa:** el nivel eléctrico 1 = válvula cerrada (transistor inversor). Se modela con `SignalLevel` y `ValveState` como tipos distintos. No "corregir".

## No hagas

- No agregar `delay()` en el lazo de control. Usa máquinas de estados con `millis()` y `wiga::elapsed()`. La temporización debe ser no bloqueante, presupuesto ≤ 2 ms por vuelta.
- No incluir `<Arduino.h>`, `<XBee.h>`, `<LiquidCrystal_I2C.h>` ni `<avr/wdt.h>` en `lib/wiga_control/` o `lib/wiga_domain/`. Rompe las pruebas en host. La barrera de capas es estructural, no una convención.
- No hardcodear direcciones de radio, IDs de válvula o pines fuera de `lib/wiga_config/`. Todo va en `installation_map.h`.
- No vaciar el búfer del radio antes de transmitir. Eso descarta confirmaciones válidas (FR-018).
- No "corregir" la inversión de lógica en `SignalLevel`/`ValveState`. Es intencional y documentada (FR-021).
- No crear memoria dinámica (`new`, `malloc`, `String`, STL con heap). Todo el estado es estático y dimensionado en compilación.
- No olvidar `F()` en cadenas literales AVR. Cada byte de SRAM cuenta (8 KB total, límite 75%).
- No tocar el firmware de los nodos remotos. El protocolo es invariante byte a byte.
- No tocar los sketches originales en las carpetas hermanas (`CABLE_VIRTUAL___LATCH_V1/`, `WIGA_24IN_LCD/`, `WIGA_24IN_LCDB1_5/`). Son legado archivado.
- No subir `.env*`, credenciales o claves al repositorio.
- No instalar dependencias nuevas sin verificar que ya se usan en el proyecto o que son estrictamente necesarias.
- No modificar la lógica de control sin una especificación asociada en `specs/`.

## Flujo de trabajo

- Antes de una tarea no trivial, propón un plan y espera mi OK.
- Toda funcionalidad nueva sigue el ciclo Spec Kit: `/speckit-specify` → `/speckit-clarify` → `/speckit-plan` → `/speckit-tasks` → `/speckit-implement`.
- Una rama `NNN-nombre-corto` por especificación. Prohibido comprometer lógica de control directamente en `main`.
- Antes de cada commit, ejecuta `pio test -e native` y verifica que pasa.
- Antes de cada PR, ejecuta `pio run -e megaatmega2560` y verifica que compila sin errores.
- Reporta el uso de SRAM y Flash en cada commit o PR.
- Una tarea a la vez; al terminar, dime qué cambiaste para que lo revise.
- Si no estás seguro al 80%, pregunta. No inventes.
- Commits referencian la especificación: `feat(control): FR-006/FR-007 — reposo prolongado`.

## Documentación

| Documento | Ubicación |
|---|---|
| Constitución del proyecto | `.specify/memory/constitution.md` |
| Especificación (35 FR, 3 NFR) | `specs/001-migracion-platformio/spec.md` |
| 14 decisiones técnicas | `specs/001-migracion-platformio/research.md` |
| Plan técnico y arquitectura | `specs/001-migracion-platformio/plan.md` |
| Modelo de datos y transiciones | `specs/001-migracion-platformio/data-model.md` |
| Contratos (protocolo, puertos, mapa) | `specs/001-migracion-platformio/contracts/` |
| 11 escenarios de validación | `specs/001-migracion-platformio/quickstart.md` |
| Tareas descompuestas | `specs/001-migracion-platformio/tasks.md` |
| Puesta en marcha | `docs/puesta-en-marcha.md` |
| Configuración de finca nueva | `docs/configuracion-finca.md` |
| Diagnóstico en campo | `docs/diagnostico.md` |
| Firmware anterior (legado) | `docs/firmware-anterior.md` |
| Acta de pruebas de banco | `docs/acta-pruebas-banco.md` |
| CI (GitHub Actions) | `.github/workflows/ci.yml` |
