# Implementation Plan: Migración del firmware Cable Virtual a PlatformIO

**Branch**: `001-migracion-platformio` | **Date**: 2026-08-20 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/001-migracion-platformio/spec.md`

## Summary

Se traslada el firmware de control de 24 válvulas latch del sketch monolítico
`CABLE_VIRTUAL___LATCH_V1.ino` a un proyecto PlatformIO con arquitectura hexagonal, sin
alterar el protocolo con los nodos ya instalados en campo.

La lógica de control (política de reintentos, disparo secuencial, antirebote, códec del
payload) se aloja en librerías internas sin dependencia de Arduino, verificables en el
equipo del desarrollador con Unity. El acceso a hardware —radio XBee, panel LCD, entradas,
reloj, watchdog y consola— se declara como interfaces abstractas y se implementa en
adaptadores concretos que solo existen en la compilación para la placa.

Tres correcciones de fondo respecto al firmware base, derivadas de la investigación: se
elimina el vaciado del búfer de recepción que descartaba confirmaciones (R-003), la
temporización pasa de contar vueltas de lazo a tiempo real con aritmética resistente al
desbordamiento (R-005), y la lógica inversa por transistor se convierte en dos tipos
distintos que el compilador no deja mezclar (R-004).

## Technical Context

**Language/Version**: C++14 (`-std=gnu++14`), avr-gcc del toolchain de PlatformIO para la
placa; compilador nativo del equipo para las pruebas.

**Primary Dependencies**:
- `andrewrapp/XBee@0.0.0-alpha+sha.39f236ea18` — transporte XBee API modo escapado (AP=2).
  Fijada por sha, verificada en el registro de PlatformIO.
- `marcoschwartz/LiquidCrystal_I2C@1.1.4` — panel HD44780 sobre expansor PCF8574.
- `Wire`, `avr/wdt.h` — del framework Arduino AVR.
- Unity — incluido en PlatformIO, sin dependencia externa.

Las librerías del dominio, control, puertos y configuración **no tienen dependencias**.

**Storage**: N/A. El firmware no persiste estado; lo reconstruye de las entradas físicas al
arrancar (FR-010).

**Testing**: Unity sobre PlatformIO. Entorno `native` para la lógica pura (objetivo
principal, sin hardware); el mismo conjunto puede ejecutarse sobre la placa cuando se
requiera validar el toolchain AVR.

**Target Platform**: Arduino Mega 2560 (ATmega2560 @ 16 MHz, 8 KB SRAM, 256 KB Flash),
framework Arduino AVR. Entorno de desarrollo: Visual Studio Code con la extensión
PlatformIO (Core 6.1.19 verificado en el equipo).

**Project Type**: firmware embebido de un solo binario, con librerías internas.

**Performance Goals**:
- Cadencia del lazo de control: 15 ms nominales, con un presupuesto de **≤ 2 ms de trabajo
  por vuelta** (Principio I).
- Latencia entre cambio estable de entrada y transmisión de la orden: ≤ 1 vuelta de lazo
  más la ventana de exclusión pendiente.
- Ninguna confirmación recibida se pierde a la cadencia nominal (R-002).

**Constraints**:
- SRAM ocupada < 75 % de 8 192 B (NFR-001).
- Sin asignación dinámica de memoria en el camino de ejecución.
- Sin bloqueos > 2 ms; `delay()` prohibido fuera de `setup()`.
- Watchdog de 8 s activo, refrescado en un único punto.
- Protocolo con los nodos remotos invariante byte a byte (FR-015, R-012).
- Operación continua ≥ 90 días, atravesando el desbordamiento de `millis()` (NFR-002).

**Scale/Scope**: 24 válvulas sobre hasta 8 nodos remotos, 1 controlador por finca.
Aproximadamente 1 500 líneas de C++ repartidas en 5 librerías internas y un `main.cpp` de
composición.

## Constitution Check

*GATE: debe pasar antes de la Fase 0. Reevaluado tras el diseño de Fase 1.*

| Principio | Puerta | Evaluación inicial | Reevaluación post-diseño |
|---|---|---|---|
| **I. Determinismo del lazo** | Sin `delay()` ni bloqueos > 2 ms fuera de `setup()`; watchdog refrescado en un punto | ✅ El diseño sustituye `delay(15)` por planificador basado en `IClock`; el LCD escribe 1 celda por vuelta (R-007); la transmisión cabe en el búfer TX de 64 B sin bloquear | ✅ Sin cambios. Presupuesto por vuelta documentado en `quickstart.md` |
| **II. Seguridad de válvula (NO NEGOCIABLE)** | Disparo secuencial, reintentos acotados, confirmación obligatoria, sin ráfagas al reiniciar | ✅ `LatchScheduler` garantiza una orden a la vez; `RetryPolicy` acota intentos y aplica reposo; el estado confirmado solo cambia con respuesta válida del nodo; el arranque siembra estado sin transmitir | ✅ Reforzado: R-003 **reduce** el número de disparos frente al firmware base (NFR-003) |
| **III. Arquitectura en capas** | Dominio y control sin `<Arduino.h>`, `<XBee.h>`, `<LiquidCrystal_I2C.h>`, `<avr/wdt.h>` | ✅ La barrera es estructural: el entorno `native` no compila `lib/wiga_hal`, así que una inclusión indebida rompe el build | ✅ Sin cambios |
| **IV. Pruebas en host (NO NEGOCIABLE)** | Toda regla de negocio con prueba en `native`; pruebas junto al código | ✅ FR-001 a FR-018 son verificables sin hardware por diseño; los puertos tienen dobles de prueba | ✅ Trazabilidad FR → prueba definida en `data-model.md` y `contracts/` |
| **V. Configuración centralizada** | Un solo módulo de configuración; sin números mágicos; conteo derivado | ✅ `lib/wiga_config` concentra mapa y tiempos, con `static_assert` (R-010) | ✅ Sin cambios |
| **VI. Observabilidad** | Versión, causa de reinicio, transiciones y contadores por serie; desactivable en compilación | ✅ Puerto `ILogPort` con niveles, compilado fuera con `-D WIGA_LOG_LEVEL=0` | ✅ Sin cambios |
| **VII. Trazabilidad** | Ciclo Spec Kit; `FR-xxx` en módulo y prueba | ✅ Esta funcionalidad nace del ciclo; convención de anotación definida | ✅ Sin cambios |

| Restricción de plataforma | Evaluación |
|---|---|
| Placa Mega 2560, SRAM < 75 % | ✅ Presupuesto estimado en R-011: ~330 B de estado propio sobre 8 192 B |
| Build reproducible, dependencias fijadas | ✅ Ambas librerías externas fijadas por versión/sha y verificadas en el registro |
| Entornos `megaatmega2560` y `native` | ✅ Ambos definidos en `platformio.ini` |
| Sin memoria dinámica | ✅ Todo el estado es estático y dimensionado en compilación |
| Documentación en español, identificadores en inglés | ✅ Convención adoptada en todo el proyecto |

**Resultado de la puerta: PASA.** No hay violaciones que justificar; la sección
*Complexity Tracking* queda vacía a propósito.

## Project Structure

### Documentation (this feature)

```text
specs/001-migracion-platformio/
├── plan.md                      # Este archivo
├── spec.md                      # Especificación (34 FR, 3 NFR, 8 SC)
├── research.md                  # Fase 0 — 12 decisiones de investigación
├── data-model.md                # Fase 1 — entidades, estados y transiciones
├── quickstart.md                # Fase 1 — guía de validación ejecutable
├── contracts/
│   ├── application-protocol.md  # Protocolo de aplicación con los nodos remotos
│   ├── ports.md                 # Interfaces entre la lógica y el hardware
│   └── installation-map.md      # Contrato del archivo de configuración de finca
├── checklists/
│   └── requirements.md          # Checklist de calidad de la especificación (16/16)
└── tasks.md                     # Fase 2 — generado por /speckit-tasks
```

### Source Code (repository root)

```text
platformio.ini                   # entornos megaatmega2560 y native; dependencias fijadas
src/
└── main.cpp                     # única raíz de composición: instancia adaptadores,
                                 # inyecta puertos y ejecuta el lazo. Sin lógica de negocio.

lib/
├── wiga_config/                 # SIN dependencias de hardware
│   ├── installation_map.h       # mapa de finca: dirección + id + pin por válvula
│   ├── installation_map.cpp
│   └── timing_config.h          # tiempos, umbrales, geometría del panel
├── wiga_domain/                 # SIN dependencias de hardware
│   ├── valve_types.h            # SignalLevel, ValveState, ValveIndex, conversión R-004
│   ├── app_protocol.h/.cpp      # codificación/decodificación del payload y verificador
│   └── valve_record.h           # estado por válvula
├── wiga_control/                # SIN dependencias de hardware
│   ├── debouncer.h/.cpp         # filtro de estabilidad de entradas (FR-002)
│   ├── retry_policy.h/.cpp      # intentos, intervalo corto, reposo prolongado (FR-006/7/8)
│   ├── latch_scheduler.h/.cpp   # exclusión mutua de disparo (FR-004)
│   └── valve_supervisor.h/.cpp  # orquestador: une lo anterior con los puertos
├── wiga_ports/                  # SIN dependencias de hardware — solo cabeceras
│   ├── i_clock.h
│   ├── i_digital_input_port.h
│   ├── i_radio_port.h
│   ├── i_display_port.h
│   ├── i_watchdog_port.h
│   └── i_log_port.h
└── wiga_hal/                    # ÚNICA capa con dependencias de hardware
    ├── arduino_clock.h/.cpp
    ├── arduino_digital_input.h/.cpp
    ├── xbee_radio_adapter.h/.cpp
    ├── lcd_display_adapter.h/.cpp     # buffer de sombra, 1 celda por vuelta (R-007)
    ├── avr_watchdog.h/.cpp
    ├── serial_log.h/.cpp
    └── reset_cause.h/.cpp             # captura de MCUSR en .init3 (R-008)

test/
├── test_domain/                 # códec del payload, verificador, conversión de niveles
├── test_control/                # antirebote, política de reintentos, planificador
├── test_integration/            # supervisor completo contra dobles de prueba
└── mocks/                       # implementaciones de prueba de los puertos

legacy/                          # material archivado (FR-033)
├── README.md                    # a qué instalación corresponde cada versión
├── CABLE_VIRTUAL___LATCH_V1/
├── WIGA_24IN_LCD/
└── WIGA_24IN_LCDB1_5/

docs/
├── puesta-en-marcha.md          # entorno, compilación, carga (FR-034)
├── configuracion-finca.md       # cómo mapear una instalación nueva
└── diagnostico.md               # lectura de la consola serie, fallos típicos
```

**Structure Decision**: se adopta la estructura de librerías internas de PlatformIO
(`lib/`), no una carpeta plana bajo `src/`. La razón es que PlatformIO enlaza únicamente
las librerías incluidas: el entorno `native` compila `wiga_config`, `wiga_domain`,
`wiga_control` y `wiga_ports`, y **nunca** `wiga_hal`. Con eso, la barrera de capas del
Principio III deja de depender de la disciplina del desarrollador y pasa a ser una
propiedad del build: incluir `<Arduino.h>` en el dominio rompe la compilación de pruebas de
inmediato. Los sketches originales se mueven a `legacy/` sin modificar, cumpliendo FR-033.

## Fases del trabajo

| Fase | Contenido | Entregable |
|---|---|---|
| 0 | Investigación de compatibilidad, dependencias y técnicas | `research.md` ✅ |
| 1 | Modelo de datos, contratos e guía de validación | `data-model.md`, `contracts/`, `quickstart.md` ✅ |
| 2 | Descomposición en tareas ordenadas por dependencia | `tasks.md` (vía `/speckit-tasks`) |
| 3 | Implementación guiada por las tareas | código en `src/`, `lib/`, `test/` |
| 4 | Validación en banco y publicación | acta de aceptación, repositorio en GitHub |

## Riesgos y mitigaciones

| Riesgo | Impacto | Mitigación |
|---|---|---|
| Los nodos de campo no aceptan el payload de 5 bytes con verificador | Alto — sistema inoperante | Verificar en banco contra un nodo real **antes** de sustituir en producción (R-012). El firmware base ya lo usa, así que la evidencia apunta a que sí |
| `optiboot` limpia `MCUSR` antes de `.init3` | Bajo — se pierde la causa de reinicio | Documentar la limitación; el control no depende del dato (R-008) |
| El toolchain no acepta `-std=gnu++14` con bucles en `constexpr` | Medio — no compila la validación del mapa | Alternativa ya identificada: expresar las comprobaciones de forma recursiva, válida en C++11 (R-010) |
| El mapa heredado tiene direcciones repetidas de origen dudoso | Alto — una válvula obedece por otra | Contrastar el mapa contra el plano de la finca durante la puesta en marcha (R-010) |
| La corrección de R-003 cambia el comportamiento observado en campo | Medio | El cambio solo puede **reducir** reintentos, nunca aumentarlos; se valida contra NFR-003 en banco |
| No hay nodo remoto disponible para la prueba de aceptación | Medio — bloquea la validación final | Declarado como dependencia de la especificación; se puede avanzar todo el desarrollo y las pruebas en host sin él |

## Complexity Tracking

*No aplica.* La Constitution Check pasa sin violaciones: no hay desviaciones de principio
que justificar en esta funcionalidad.
