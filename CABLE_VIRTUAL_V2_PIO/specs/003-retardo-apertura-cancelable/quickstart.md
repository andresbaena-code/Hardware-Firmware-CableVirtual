# Quickstart: validar la separación de comandos por UC511

**Spec**: [spec.md](spec.md) · **Contrato**: [contracts/command-spacer.md](contracts/command-spacer.md)

Tres niveles, igual que en la spec 002: escritorio, banco y campo. No se pasa al
siguiente nivel sin aprobar el anterior.

En esta máquina `pio` no está en el PATH; se usa
`C:/Users/USUARIO/.platformio/penv/Scripts/platformio.exe` (abreviado `pio` abajo).

## Nivel 1 — Escritorio (sin hardware)

```bash
pio test -e native
```

Debe pasar la suite completa: las 92 pruebas de la spec 002 más las nuevas. Los
vectores 1–12 del [contrato](contracts/command-spacer.md#5-vectores-de-prueba-host)
tienen cada uno su prueba, con el `FR-2xx` en el nombre.

```bash
pio run -e megaatmega2560
```

Compila sin errores ni advertencias nuevas. Anotar RAM y Flash; la RAM debe seguir por
debajo del 75 %.

**Prueba de que el mapa inválido no compila** (una vez, a mano): asignar a V03 el
mismo `actuator`/`output` que V01, compilar y comprobar que falla con `MAP-V15`. Deshacer.

## Nivel 2 — Banco (Mega + MAX485 + maestro de escritorio, sin UC100)

Montaje: el de la spec 002 ([quickstart 002](../002-modbus-esclavo-rs485/quickstart.md)).
Maestro de escritorio leyendo los registros 0–8 cada 500 ms con marca de tiempo.
Consola serie a 9600 abierta.

| # | Acción en el tablero | Se debe ver |
|---|---|---|
| B-1 | subir V01 | reg 0 = `1` en el siguiente sondeo; consola `TX 0 1` |
| B-2 | con V01 arriba > 15 s: bajar V01 y subir V02 a la vez | reg 0 = `0` al instante; consola `WAIT 1`; reg 1 = `1` ~15 s después |
| B-3 | bajar V02 y subir V03 a la vez | reg 1 y reg 2 cambian en el mismo sondeo |
| B-4 | inmediatamente tras B-3, subir y bajar V04 en 3 s | reg 3 nunca vale `1`; consola `CANC 3` |
| B-5 | subir V09 | reg 8 sigue en `0`; LCD muestra `-` en V09; consola `NOINST 8` una sola vez |
| B-6 | con V01 arriba, resetear la placa | reg 0 = `1` en el primer sondeo tras el arranque |

## Nivel 3 — Campo (piloto: UC100 + UC511 #01–#04)

Configuración del UC100 y de los UC511 según [docs/piloto-8-valvulas-d2d.md](../../docs/piloto-8-valvulas-d2d.md),
actualizada por esta spec (FR-218). **Antes de empezar**, verificar que el cableado real
coincide con el mapa: V01/V02 en el UC511 #01, salidas 1/2, y así con los demás.

| # | Prueba | Aprobada si | Criterio |
|---|---|---|---|
| C-1 | 10 cambios V01→V02 en el UC511 #01 | las dos válvulas responden las 10 veces | SC-201 |
| C-2 | 10 cambios V02→V03 (equipos distintos) | sin espera añadida, 10 de 10 | SC-202 |
| C-3 | 10 pedidos revertidos dentro del intervalo | ninguna válvula se mueve | SC-203 |
| C-4 | 10 cierres pedidos 3 s después de abrir | las 10 cierran | SC-203 |
| C-5 | 10 reinicios con válvulas regando | ninguna cierra | SC-204 |
| C-6 | secuencia completa de un bloque (7 + 7 min) | primera 7:00, segunda 6:45 ± 2 s | SC-206 |

Registrar los resultados en `docs/acta-pruebas-banco.md`, en un bloque nuevo para la
spec 003.
