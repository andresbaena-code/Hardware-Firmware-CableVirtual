# Puesta en marcha

Cómo pasar de un equipo vacío a un Arduino Mega con el firmware cargado. Debería tomar
menos de 15 minutos.

---

## 1. Requisitos del entorno

| Componente | Versión | Nota |
|---|---|---|
| Visual Studio Code | cualquiera reciente | |
| Extensión **PlatformIO IDE** | cualquiera reciente | VS Code la propone al abrir el proyecto |
| Git | cualquiera | |
| Compilador de C++ | **no hace falta** | PlatformIO trae el suyo, ver §4 |

No hay que instalar el IDE de Arduino, ni librerías a mano, ni MinGW, ni Visual Studio
Build Tools.

## 2. Clonar y abrir

```bash
git clone https://github.com/andresbaena-code/Hardware-Firmware-CableVirtual
```

El proyecto PlatformIO vive en la subcarpeta **`CABLE_VIRTUAL_V2_PIO/`**. Las otras tres
carpetas del repositorio son los sketches originales de Arduino y no se compilan.

Dos formas de abrirlo en VS Code:

- Abrir la carpeta `CABLE_VIRTUAL_V2_PIO` directamente. PlatformIO la detecta al instante
  porque contiene `platformio.ini`.
- Abrir el archivo `WIGA-CableVirtual.code-workspace` de la raíz. Muestra el firmware y la
  documentación a la vez, y PlatformIO sigue funcionando.

La primera vez, PlatformIO descarga solo el toolchain de AVR y las dos librerías externas
en las versiones fijadas.

**Todos los comandos `pio` de esta guía se ejecutan desde `CABLE_VIRTUAL_V2_PIO/`.**

Si `pio` no está en el `PATH` de Windows, el ejecutable está en:

```
%USERPROFILE%\.platformio\penv\Scripts\pio.exe
```

## 3. Compilar y cargar

```bash
pio run -e megaatmega2560
```

Debe terminar en `SUCCESS` y reportar el uso de memoria:

```
RAM:   [==        ]  20.5% (used 1682 bytes from 8192 bytes)
Flash: [          ]   4.6% (used 11806 bytes from 253952 bytes)
```

> **Puerta de calidad:** el uso de RAM debe mantenerse por debajo del **75 %**. Si un
> cambio lo sube por encima, no se integra. El margen restante es la pila.

Para cargar en la placa, con el Mega conectado por USB:

```bash
pio run -e megaatmega2560 -t upload
```

PlatformIO detecta el puerto solo. Si hay varios dispositivos, `pio device list` los
muestra y se fija el puerto con `upload_port` en `platformio.ini`.

## 4. Ejecutar las pruebas

```bash
pio test -e native
```

73 pruebas, unos 18 segundos, **sin ninguna placa conectada**.

En Windows el entorno `native` usa la plataforma `windows_x86`, cuyo compilador descarga
y gestiona el propio PlatformIO. Por eso no hace falta instalar MinGW ni Visual Studio.
En Linux o macOS se usa el compilador del sistema:

```bash
pio test -e native_posix
```

### Por qué `wiga_hal` no se compila en las pruebas

`platformio.ini` declara `lib_ignore = wiga_hal` en los entornos de prueba. Eso hace que
la separación de capas sea una propiedad del build y no una convención: si alguien
incluye `<Arduino.h>` o `<LiquidCrystal_I2C.h>` desde `lib/wiga_domain`,
`lib/wiga_control`, `lib/wiga_config` o `lib/wiga_modbus`, `pio test` falla en el acto
con *"No such file or directory"*.

Es un comportamiento **deseado**. Comprobarlo lleva 30 segundos:

```bash
# añadir #include <Arduino.h> en lib/wiga_modbus/modbus_slave.h
pio test -e native      # debe FALLAR
# revertir
pio test -e native      # debe PASAR
```

`lib/wiga_modbus` es la que más importa mantener pura. La migración anterior metió el
protocolo Modbus dentro de `wiga_hal`, que este mecanismo excluye, y acumuló **25
defectos** con las 73 pruebas del proyecto en verde: ninguna lo tocaba. El CI comprueba
la barrera en las dos capas en cada push.

## 5. Verificar el bus sin el UC100

Antes de conectar el puente conviene comprobar que el controlador responde como esclavo
Modbus. Hace falta un adaptador USB-RS485 y un maestro de escritorio (`mbpoll`,
QModMaster o el ToolBox de Milesight).

```
   Arduino Mega 2560            MAX485                 Adaptador USB-RS485
   TXD2  PH1 (pin 16) ────────▶ DI
   RXD2  PH0 (pin 17) ◀──────── RO           A ◀──────▶ A
   DE/RE PH3 (pin  6) ────────▶ DE + RE      B ◀──────▶ B
                                             GND ◀────▶ GND
```

```bash
mbpoll -m rtu -b 9600 -P none -d 8 -s 1 -a 1 -t 4 -r 1 -c 24 COM5
```

Debe devolver 24 valores que reflejen la posición real de los interruptores del tablero.
Mover un interruptor debe cambiar solo su registro, tras la ventana de antirrebote.

El procedimiento completo de banco, con los once escenarios y los vectores de trama, está
en [quickstart.md](../specs/002-modbus-esclavo-rs485/quickstart.md).

> **El escenario decisivo es el B-08**: una hora de sondeo continuo con una válvula
> regando y **cero** lecturas de `0`. Un solo `0` corta el riego en campo.

## 6. Consola de diagnóstico

```bash
pio device monitor -b 9600
```

Al arrancar debe verse el nombre del firmware, la versión, la fecha de compilación y la
causa del último reinicio. Interpretación de la salida en
[diagnostico.md](diagnostico.md).

---

## Problemas frecuentes

**«No se encuentra la placa» al cargar.**
Comprobar con `pio device list` que el Mega aparece. En Windows suele faltar el driver
CH340 si la placa es un clon. El Mega original usa el driver ATmega16U2, que Windows
instala solo.

**La compilación falla con `-std=gnu++14`.**
El proyecto necesita C++14 porque la validación del mapa de finca usa `constexpr` con
bucles. Si el toolchain diera problema, las comprobaciones pueden reescribirse de forma
recursiva, válida en C++11. Está previsto en
[research.md R-010](../specs/001-migracion-platformio/research.md).

**Las pruebas no encuentran los dobles de prueba.**
Los `fake_*.h` viven en `test/mocks` y se exponen con `-Itest/mocks` en `platformio.ini`.

**El controlador no responde a ningún sondeo.**
Comprobar en este orden: que el identificador de esclavo del UC100 coincide con
`kModbusSlaveId` (por defecto `1`); que los parámetros serie coinciden (9600 8N1); que
A y B no están cruzados; y que el pin DE/RE es el 2. Una petición dirigida a otro
identificador produce **silencio absoluto**, que es el comportamiento correcto y se
confunde fácilmente con una avería.
Si se añade un entorno de prueba nuevo, hay que repetir esa bandera.

**El LCD no muestra nada.**
Verificar la dirección I2C. El proyecto asume `0x27`; algunos módulos PCF8574 vienen en
`0x3F`. Se cambia en `lib/wiga_config/timing_config.h`. Un panel que no responde **no
detiene el control de válvulas**: es deliberado.

**El firmware arranca pero no transmite nada.**
Es lo esperado si ninguna entrada ha cambiado. El firmware **no** emite órdenes al
arrancar, para no golpear la batería tras cada reinicio. Conmutar una entrada del tablero
y observar el evento `TX` en consola. Ver [ARQUITECTURA_DEL_FIRMWARE.md](ARQUITECTURA_DEL_FIRMWARE.md) §8.
