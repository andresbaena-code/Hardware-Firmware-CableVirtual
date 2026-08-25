# WIGA Cable Virtual — Firmware V2 (PlatformIO)

Firmware de control de 24 válvulas latch de riego por radio XBee ZigBee, para
**Arduino Mega 2560**.

El sistema sustituye el cableado físico entre el tablero de control de riego y las
válvulas distribuidas en la finca: lee 24 señales del tablero y las replica de forma
inalámbrica sobre las válvulas remotas. Para el operador debe comportarse como si el cable
existiera.

> **Esta es la carpeta que se abre en Visual Studio Code.** Las carpetas hermanas
> (`CABLE_VIRTUAL___LATCH_V1`, `WIGA_24IN_LCD`, `WIGA_24IN_LCDB1_5`) son los sketches
> originales de Arduino: no se compilan desde aquí y no deben modificarse.
> Ver [docs/firmware-anterior.md](docs/firmware-anterior.md).

---

## Estado

| | |
|---|---|
| Versión de firmware | 2.0.0 |
| Placa | Arduino Mega 2560 (ATmega2560) |
| Uso de RAM | 22,2 % (1 822 / 8 192 B) — el límite del proyecto es 75 % |
| Uso de Flash | 4,8 % (12 110 / 253 952 B) |
| Pruebas en host | 71, todas en verde, 18 s |

## Empezar

Abrir **esta carpeta** en VS Code. PlatformIO la detecta al instante porque contiene
`platformio.ini`.

```bash
pio run  -e megaatmega2560            # compilar
pio run  -e megaatmega2560 -t upload  # cargar en la placa
pio test -e native                    # 71 pruebas, sin hardware
pio device monitor -b 9600            # consola de diagnóstico
```

En Windows, si `pio` no está en el `PATH`:
`%USERPROFILE%\.platformio\penv\Scripts\pio.exe`

Guía completa en [docs/puesta-en-marcha.md](docs/puesta-en-marcha.md).

## Cómo funciona

Cada válvula es una máquina de estados independiente. Cuando la entrada del tablero
cambia de forma estable, la válvula pide turno; el planificador concede **un solo
disparo a la vez** con 1,5 s de separación, para no encadenar picos de corriente sobre
la batería de 12 V. La válvula no se da por buena hasta que el nodo remoto **confirma**
el estado alcanzado.

```
   VERIFIED ──cambia la entrada──► PENDING ──turno concedido──► AWAITING
      ▲                               ▲                            │
      │                               │                            │
      └────confirmación del nodo──────┼────────────────────────────┘
                                      │                            │
                                      │                     vence la espera
                                      │                            │
                              vence el reposo              ┌───────┴────────┐
                                      │              5 intentos       menos de 5
                                      │                    │               │
                                   BACKOFF ◄───────────────┘               │
                                  (60 min)                                 │
                                      ▲                                    │
                                      └────────────────────────────────────┘
```

Si un nodo no responde: 5 reintentos separados 30 s, y después **una hora de reposo**
antes de volver a intentar. Un cambio de entrada cancela el reposo de inmediato. El
resultado es un máximo de 5 disparos en la primera media hora y no más de uno por hora
después.

## Arquitectura

```
platformio.ini        entornos megaatmega2560, megaatmega2560_silent y native
src/main.cpp          raíz de composición — crea adaptadores y corre el lazo
lib/wiga_config/      mapa de finca y tiempos      ← lo único que se edita por instalación
lib/wiga_domain/      tipos, protocolo, tiempo     ┐
lib/wiga_control/     antirebote, reintentos,      │ sin dependencias de hardware:
                      planificador, supervisor     │ se prueban en el escritorio
lib/wiga_ports/       interfaces hacia hardware    ┘
lib/wiga_hal/         adaptadores Arduino/XBee/LCD ← única capa que toca hardware
test/                 71 pruebas en host
docs/                 puesta en marcha, configuración, diagnóstico, acta de banco
specs/                especificación Spec Kit de la migración
```

La separación **no es una convención**: el entorno de pruebas ignora `lib/wiga_hal`, así
que incluir `<Arduino.h>` desde el dominio o el control rompe la compilación de
inmediato. Se puede comprobar en 30 segundos siguiendo el escenario 3 de
[quickstart](specs/001-migracion-platformio/quickstart.md), y el CI lo verifica en cada
push.

## Dos cosas que parecen errores y no lo son

**1. El panel muestra el estado invertido respecto al byte del protocolo.** La etapa de
potencia de los nodos opera con **lógica inversa por transistor**: nivel eléctrico 1 =
válvula cerrada. El firmware modela eso con dos tipos distintos (`SignalLevel` y
`ValveState`) y una única conversión con nombre. El panel muestra el **estado físico**. Si
alguien "corrige" la inversión, rompe todas las instalaciones.

**2. Los radios usan modo API escapado (AP = 2).** No es opcional. Está verificado
leyendo el código de la librería XBee, que escapa siempre en transmisión. Transmitir en
AP = 1 haría que los nodos descartaran cualquier trama con un byte reservado, con fallos
intermitentes imposibles de diagnosticar.

Ambas decisiones están documentadas en
[research.md](specs/001-migracion-platformio/research.md).

## Configurar una finca nueva

Se edita **un solo archivo**: [`lib/wiga_config/installation_map.h`](lib/wiga_config/installation_map.h).
Un pin duplicado o un par (dirección, id) repetido **no compila**. Procedimiento en
[docs/configuracion-finca.md](docs/configuracion-finca.md).

## Diagnóstico en campo

Conectar un portátil al USB y abrir la consola a 9 600 baudios. El firmware informa su
versión, la causa del último reinicio y cada transición de estado.
Guía en [docs/diagnostico.md](docs/diagnostico.md).

## Antes de sustituir firmware en producción

Ejecutar los escenarios de banco y firmar el
[acta de pruebas](docs/acta-pruebas-banco.md). El **bloque 0 es bloqueante**: hay que
confirmar contra un nodo real que acepta el payload de 5 bytes con verificador.

## Documentación del proyecto

Este firmware se desarrolla con [GitHub Spec Kit](https://github.com/github/spec-kit).
Nada entra al código sin una especificación asociada.

| Documento | Qué contiene |
|---|---|
| [Constitución](.specify/memory/constitution.md) | los 7 principios que gobiernan el proyecto |
| [Especificación](specs/001-migracion-platformio/spec.md) | 35 requisitos funcionales, 3 no funcionales, 8 criterios de éxito |
| [Investigación](specs/001-migracion-platformio/research.md) | 14 decisiones técnicas con su porqué |
| [Plan técnico](specs/001-migracion-platformio/plan.md) | arquitectura, riesgos y puertas de calidad |
| [Modelo de datos](specs/001-migracion-platformio/data-model.md) | entidades, invariantes y las 11 transiciones |
| [Contratos](specs/001-migracion-platformio/contracts/) | protocolo con los nodos, puertos, mapa de finca |
| [Validación](specs/001-migracion-platformio/quickstart.md) | 11 escenarios de aceptación |
| [Tareas](specs/001-migracion-platformio/tasks.md) | descomposición del trabajo |
| [Acta de pruebas de banco](docs/acta-pruebas-banco.md) | formulario para firmar antes de producción |

## Integración continua

El workflow vive en `.github/workflows/ci.yml`, en la raíz del repositorio — GitHub
Actions solo lee los workflows desde ahí. Automatiza las puertas de calidad de la
constitución:

- la suite completa de pruebas en host;
- la compilación para el Mega, que **falla si la RAM supera el 75 %**;
- la comprobación de que la barrera de capas es estructural;
- la verificación de que todo requisito sigue citado en el código.

## Licencia

Software propietario de WIGA. Todos los derechos reservados.
