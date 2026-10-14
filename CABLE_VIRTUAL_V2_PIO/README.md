# WIGA Cable Virtual — Firmware V2 (PlatformIO)

Firmware de control de 24 válvulas latch de riego para **Arduino Mega 2560**, que publica
como **esclavo Modbus RTU** por RS485 el estado pedido por los interruptores de un tablero
de riego. Un puente Milesight **UC100-915M** hace de maestro Modbus y de controlador D2D
LoRa: lee los registros y, con sus reglas IF-THEN locales, emite de forma inalámbrica la
orden a los agentes **UC501** que accionan cada válvula.

El sistema sustituye el cableado físico entre el tablero y las válvulas distribuidas en la
finca. Para el operador debe comportarse como si el cable existiera.

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
| Comunicación | Modbus RTU esclavo por RS485, 9600 8N1, serial 2, MAX485 (DE/RE pin 6) |
| Uso de RAM | 20,5 % (1 682 / 8 192 B) — el límite del proyecto es 75 % |
| Uso de Flash | 4,6 % (11 806 / 253 952 B) |
| Pruebas en host | **92**, todas en verde (56 Modbus + control + dominio + integración) |
| Validación en banco | **T064 completado** 2026-09-03 (Mega + MAX485 + Docklight) |

## Empezar

Abrir **esta carpeta** en VS Code. PlatformIO la detecta al instante porque contiene
`platformio.ini`.

```bash
pio run  -e megaatmega2560            # compilar
pio run  -e megaatmega2560 -t upload  # cargar en la placa
pio test -e native                    # 92 pruebas, sin hardware
pio device monitor -b 9600            # consola de diagnóstico
```

En Windows, si `pio` no está en el `PATH`:
`%USERPROFILE%\.platformio\penv\Scripts\pio.exe`

Guía completa en [docs/puesta-en-marcha.md](docs/puesta-en-marcha.md).

## Cómo funciona

El controlador es un **esclavo Modbus**: nunca inicia una transacción y solo responde a las
peticiones que el UC100 le dirige por RS485.

```
[ Controlador (este firmware) ]      [ UC100 ]              [ UC501 ]
        ATmega2560                  D2D Controller         D2D Agent
        ESCLAVO Modbus              MAESTRO Modbus         receptor D2D
             │                           │                       │
             │  mantiene el registro     │                       │
             │  de cada válvula          │                       │
             │<---- sondeo FC03/FC01 ----│                       │
             │---- responde ------------>│                       │
             │                           │--- regla IF-THEN      │
             │                           │    dispara D2D ------->│
             │                           │    LoRa directo        │-- activa GPIO
             │                           │    SIN gateway         │   -> relé -> válvula
```

El registro de cada válvula es la **única salida** del sistema. El controlador lo mantiene
estable (`1` mientras el tablero pide riego, `0` si no) y el UC100 lo lee y actúa. Como el
protocolo D2D **no acusa recibo**, el firmware solo aporta la re-aserción: sosteniendo el
nivel, el UC100 vuelve a emitir el comando y un receptor que perdió un mensaje captura otro.

**El controlador no confirma el campo.** Ninguna confirmación del estado real puede llegarle
nunca. La seguridad ante orden perdida la da el auto-apagado temporizado del UC501.

## Arquitectura

```
platformio.ini         entornos megaatmega2560, _silent y native
src/main.cpp           raíz de composición — crea adaptadores y corre el lazo
lib/wiga_config/       mapa de finca y tiempos      ← lo único que se edita por instalación
lib/wiga_domain/       tipos (ValveState, SignalLevel)      ┐
lib/wiga_modbus/       esclavo Modbus, CRC16, registros     │ sin dependencias de hardware:
lib/wiga_control/      lectura del tablero, antirrebote     │ se prueban en el escritorio
lib/wiga_ports/        interfaces hacia hardware            ┘
lib/wiga_hal/          adaptadores RS485/LCD/reloj/watchdog ← única capa que toca hardware
test/                  92 pruebas en host
docs/                  puesta en marcha, configuración, diagnóstico, arquitectura, acta
specs/                 especificaciones Spec Kit (001 y 002)
```

La separación **no es una convención**: el entorno de pruebas ignora `lib/wiga_hal`, así
que incluir `<Arduino.h>` desde `wiga_modbus`, `wiga_domain` o `wiga_control` rompe la
compilación de inmediato. El CI lo verifica en cada push.

## Dos cosas que parecen errores y no lo son

**1. El controlador no responde hasta que le preguntan.** Es un esclavo. Su **silencio es
idéntico** ante "maestro apagado", "cable cortado" o "esclavo sano"; el registro de
diagnóstico «segundos sin sondeo» es lo que distingue los tres casos.

**2. Executar una escritura Modbus no cambia nada.** FC05, FC06, FC15 y FC16 responden
excepción `0x01`. El tablero es la única fuente de verdad: ninguna trama del bus puede
alterar el estado de una válvula.

## Configurar una finca nueva

Se edita **un solo archivo**: [`lib/wiga_config/installation_map.h`](lib/wiga_config/installation_map.h).
Un pin duplicado o un pin que pise el DE/RE del RS485 **no compila**. Procedimiento en
[docs/configuracion-finca.md](docs/configuracion-finca.md).

Los parámetros del bus (slave ID, baudios, tiempos) viven en
[`lib/wiga_config/timing_config.h`](lib/wiga_config/timing_config.h).

## Diagnóstico en campo

Sin abrir la carcasa, leyendo el **bloque de diagnóstico** (registros 24 a 31) con el
ToolBox de Milesight o un maestro de escritorio. Con portátil, por la consola a 9 600
baudios: versión, causa del último reinicio y cada cambio de válvula.
Guía en [docs/diagnostico.md](docs/diagnostico.md).

## Antes de sustituir firmware en producción

Ejecutar los escenarios de banco y firmar el
[acta de pruebas](docs/acta-pruebas-banco.md). El **escenario B-08 es decisivo**: una hora
de sondeo continuo con una válvula regando y **cero** lecturas de `0`. Debe completarse
también en el nivel 3 con el UC100 real.

## Documentación del proyecto

Este firmware se desarrolla con [GitHub Spec Kit](https://github.com/github/spec-kit).
Nada entra al código sin una especificación asociada.

| Documento | Qué contiene |
|---|---|
| [Constitución](.specify/memory/constitution.md) | los 7 principios que gobiernan el proyecto |
| [Especificación 002](specs/002-modbus-esclavo-rs485/spec.md) | esclavo Modbus — vigente |
| [Especificación 001](specs/001-migracion-platformio/spec.md) | migración a PlatformIO (XBee, superada) |
| [Contrato de registros](specs/002-modbus-esclavo-rs485/contracts/register-map.md) | mapa Modbus y verificación |
| [Contrato del bus](specs/002-modbus-esclavo-rs485/contracts/modbus-slave.md) | comportamiento en el bus |
| [Arquitectura](docs/ARQUITECTURA_DEL_FIRMWARE.md) | cómo funciona el sistema completo |
| [Validación](specs/002-modbus-esclavo-rs485/quickstart.md) | escenarios de escritorio, banco y campo |
| [Consulta 001](docs/firmware-anterior.md) | sketches originales archivados |
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
