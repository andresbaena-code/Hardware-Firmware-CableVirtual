# Firmware V2 — Cable Virtual (PlatformIO)

**Esta es la carpeta que se abre en Visual Studio Code.**

Proyecto PlatformIO del firmware de control de 24 válvulas latch por XBee, para Arduino
Mega 2560. Las carpetas hermanas del repositorio (`CABLE_VIRTUAL___LATCH_V1`,
`WIGA_24IN_LCD`, `WIGA_24IN_LCDB1_5`) son los sketches originales de Arduino: no se
compilan desde aquí y no deben modificarse.

## Abrir en VS Code

Dos formas, ambas válidas:

- **Abrir esta carpeta** (`CABLE_VIRTUAL_V2_PIO`) directamente. PlatformIO la detecta al
  instante porque contiene `platformio.ini`.
- **Abrir `WIGA-CableVirtual.code-workspace`** en la raíz del repositorio. Muestra a la vez
  el firmware y la documentación, y PlatformIO sigue funcionando.

## Comandos

```bash
pio run  -e megaatmega2560            # compilar
pio run  -e megaatmega2560 -t upload  # cargar en la placa
pio test -e native                    # 71 pruebas, sin hardware
pio device monitor -b 9600            # consola de diagnóstico
```

En Windows, si `pio` no está en el `PATH`:
`%USERPROFILE%\.platformio\penv\Scripts\pio.exe`

## Qué hay dentro

```
platformio.ini        entornos megaatmega2560 y native; dependencias fijadas
src/main.cpp          raíz de composición — sin lógica de negocio
lib/wiga_config/      mapa de finca y tiempos   ← lo único que se edita por instalación
lib/wiga_domain/      tipos, protocolo, tiempo  ┐ sin dependencias de hardware:
lib/wiga_control/     lógica de control         │ se prueban en el escritorio
lib/wiga_ports/       interfaces hacia hardware ┘
lib/wiga_hal/         adaptadores Arduino/XBee/LCD ← única capa que toca hardware
test/                 71 pruebas en host
```

## Documentación

Está en la raíz del repositorio, un nivel arriba:

- [Puesta en marcha](../docs/puesta-en-marcha.md)
- [Configurar una instalación](../docs/configuracion-finca.md)
- [Diagnóstico en campo](../docs/diagnostico.md)
- [Firmware anterior](../docs/firmware-anterior.md)
- [Especificación completa](../specs/001-migracion-platformio/)
