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

Abrir la carpeta en VS Code. PlatformIO detecta `platformio.ini` y descarga solo, la
primera vez, el toolchain de AVR y las dos librerías externas en las versiones fijadas.

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
RAM:   [==        ]  22.2% (used 1822 bytes from 8192 bytes)
Flash: [          ]   4.8% (used 12110 bytes from 253952 bytes)
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

70 pruebas, unos 18 segundos, **sin ninguna placa conectada**.

En Windows el entorno `native` usa la plataforma `windows_x86`, cuyo compilador descarga
y gestiona el propio PlatformIO. Por eso no hace falta instalar MinGW ni Visual Studio.
En Linux o macOS se usa el compilador del sistema:

```bash
pio test -e native_posix
```

### Por qué `wiga_hal` no se compila en las pruebas

`platformio.ini` declara `lib_ignore = wiga_hal` en los entornos de prueba. Eso hace que
la separación de capas sea una propiedad del build y no una convención: si alguien
incluye `<Arduino.h>`, `<XBee.h>` o `<LiquidCrystal_I2C.h>` desde `lib/wiga_domain` o
`lib/wiga_control`, `pio test` falla en el acto con *"No such file or directory"*.

Es un comportamiento **deseado**. Comprobarlo lleva 30 segundos:

```bash
# añadir #include <Arduino.h> en lib/wiga_control/valve_supervisor.h
pio test -e native      # debe FALLAR
# revertir
pio test -e native      # debe PASAR
```

## 5. Consola de diagnóstico

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
Si se añade un entorno de prueba nuevo, hay que repetir esa bandera.

**El LCD no muestra nada.**
Verificar la dirección I2C. El proyecto asume `0x27`; algunos módulos PCF8574 vienen en
`0x3F`. Se cambia en `lib/wiga_config/timing_config.h`. Un panel que no responde **no
detiene el control de válvulas**: es deliberado.

**El firmware arranca pero no transmite nada.**
Es lo esperado si ninguna entrada ha cambiado. El firmware **no** emite órdenes al
arrancar, para no golpear la batería tras cada reinicio. Conmutar una entrada del tablero
y observar el evento `TX` en consola.
