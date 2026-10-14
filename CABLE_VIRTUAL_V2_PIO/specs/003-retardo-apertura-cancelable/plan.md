# Implementation Plan: Separación de comandos por UC511

**Branch**: `003-retardo-apertura-cancelable` | **Date**: 2026-10-14 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/003-retardo-apertura-cancelable/spec.md`

## Summary

El UC511 pierde el segundo de dos comandos que le llegan con menos de 5 s de
separación. El controlador pasa a conocer a qué UC511 pertenece cada válvula (mapa de
instalación ampliado, con marca de equipo instalado) y a no publicar dos cambios del
mismo equipo con menos de 15 s (medido en banco; el plan original decía 8 s). Los cambios que llegan en el intervalo quedan pendientes,
se cancelan si el tablero los revierte, y se publican por turnos: primero cierres,
luego el más antiguo. La lógica vive en una clase pura nueva, `CommandSpacer`, que
`ValvePublisher` consulta antes de escribir el banco de registros. El mapa de registros
Modbus no cambia.

## Technical Context

**Language/Version**: C++14 (`-std=gnu++14`), avr-gcc del toolchain de PlatformIO

**Primary Dependencies**: Arduino AVR (solo en `lib/wiga_hal` y `src/`); LiquidCrystal_I2C 1.1.4. Sin dependencias nuevas.

**Storage**: N/A (estado en SRAM estática; sin EEPROM)

**Testing**: Unity en PlatformIO, entorno `native` (Windows)

**Target Platform**: Arduino Mega 2560 (ATmega2560V a 8 MHz), esclavo Modbus RTU en Serial2

**Project Type**: firmware embebido

**Performance Goals**: separación ≥ 15 s entre cambios del mismo UC511; espera máxima < 45 s; cero espera entre equipos distintos

**Constraints**: lazo ≤ 2 ms por vuelta; SRAM < 75 % (hoy 21,1 %); sin `delay()`, sin memoria dinámica; capas puras sin `<Arduino.h>`

**Scale/Scope**: 24 válvulas, 12 UC511 previstos (4 instalados en el piloto), 1 UC100 hoy

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principio | Verificación | Estado |
|---|---|---|
| I. Determinismo del lazo | Recorrido acotado de 24 válvulas + 12 equipos por vuelta, sin esperas; tiempo con `wiga::elapsed()` | ✅ |
| II. Seguridad de válvula | Sin actuación simultánea: mejora, separa por equipo. Sin `0` transitorio (FR-209). Red de seguridad del UC511 (`Duration`) se mantiene. `-` es configuración, no confirmación. Sin ráfagas al reiniciar: la siembra no emite y ocupa los equipos | ✅ |
| III. Capas | `CommandSpacer` en `lib/wiga_control`, sin hardware; el mapa en `lib/wiga_config` | ✅ |
| IV. Pruebas en host | Vectores 1–12 del contrato como pruebas Unity antes del código | ✅ |
| V. Configuración centralizada | Equipo/salida/instalado en `installation_map.h`; intervalos en `timing_config.h`; `kActuatorCount` derivado; validaciones `MAP-V13..V17`, `TC-01` | ✅ |
| VI. Observabilidad | Trazas `TX`, `WAIT`, `CANC`, `NOINST`, desactivables por nivel | ✅ |
| VII. Trazabilidad | FR-201..FR-218 en módulos y nombres de prueba | ✅ |

Desviaciones respecto a la spec 002 (FR-110 y FR-114b) documentadas en la spec, sin
violar ningún principio. **Gate superado.** Re-evaluado tras la fase 1: sin cambios.

## Project Structure

### Documentation (this feature)

```text
specs/003-retardo-apertura-cancelable/
├── spec.md
├── plan.md                     # este archivo
├── research.md                 # R-301..R-309
├── data-model.md
├── quickstart.md               # escritorio, banco, campo
├── contracts/
│   └── command-spacer.md       # SP-01..09, VP-31..35, MAP-V13..17, vectores
├── checklists/requirements.md
└── tasks.md                    # /speckit-tasks
```

### Source Code (repository root)

```text
lib/wiga_config/
├── installation_map.h          # MODIFICADO: actuator/output, kActuators[], MAP-V13..V17
├── installation_map.cpp        # MODIFICADO si hace falta acceso a equipos
└── timing_config.h             # MODIFICADO: kActuatorMinGapMs, kCommandSpacingMs, TC-01

lib/wiga_control/
├── command_spacer.h            # NUEVO
├── command_spacer.cpp          # NUEVO
├── valve_publisher.h           # MODIFICADO: posee un CommandSpacer, símbolo '-'
└── valve_publisher.cpp         # MODIFICADO: deseado -> spacer -> registros, trazas

lib/wiga_ports/
└── i_log_port.h                # MODIFICADO: etiquetas WAIT, CANC, NOINST

test/
├── test_control/test_command_spacer.cpp       # NUEVO: vectores 1–9, 12
├── test_domain/test_config.cpp                # MODIFICADO: mapa de equipos
└── test_integration/test_separacion_uc511.cpp # NUEVO: vectores 10–11, del pin al registro

docs/
├── piloto-8-valvulas-d2d.md    # MODIFICADO (FR-218)
├── configuracion-radio-d2d.md  # MODIFICADO (FR-218)
├── configuracion-finca.md      # MODIFICADO: cómo declarar equipos
└── acta-pruebas-banco.md       # MODIFICADO: bloque de la spec 003

scripts/gen_piloto_d2d.py, scripts/gen_configuracion_d2d.py   # MODIFICADOS: tiempos acordados
```

**Structure Decision**: se respeta la estructura en capas existente. La única unidad
nueva es `CommandSpacer` en `lib/wiga_control`; `src/main.cpp` no cambia porque
`ValvePublisher` absorbe la integración.

## Complexity Tracking

Sin violaciones de la constitución que justificar.
