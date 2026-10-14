"""Genera docs/configuracion-radio-d2d.md con los 48 codigos D2D calculados.

Las tablas se generan y se comprueban: unicidad de los 48 codigos, tope de 16
reglas por equipo y rangos de canal. Nada se transcribe a mano.
"""
import io
import os
import sys

VALVULAS = 24
POR_UC100 = 8          # 16 reglas / 2 reglas por valvula
POR_UC511 = 2          # 2 interfaces de solenoide por equipo

filas = []
for v in range(1, VALVULAS + 1):
    filas.append(dict(
        v=v,
        reg=v - 1,
        uc100=(v - 1) // POR_UC100 + 1,
        canal=(v - 1) % POR_UC100 + 1,
        uc511=(v - 1) // POR_UC511 + 1,
        slot=1 if v % 2 == 1 else 2,
        abrir='%04X' % ((v << 8) | 0x01),
        cerrar='%04X' % ((v << 8) | 0x00),
    ))

# --- comprobaciones antes de escribir nada -----------------------------------
cods = [f['abrir'] for f in filas] + [f['cerrar'] for f in filas]
assert len(cods) == 48, len(cods)
assert len(set(cods)) == 48, 'hay codigos D2D repetidos'
assert '0000' not in cods, 'el codigo 0000 puede confundirse con regla deshabilitada'
for f in filas:
    assert 1 <= f['canal'] <= 8
    assert 1 <= f['uc100'] <= 3
    assert 1 <= f['uc511'] <= 12
    assert f['reg'] == f['v'] - 1

r100 = {}
for f in filas:
    r100[f['uc100']] = r100.get(f['uc100'], 0) + 2
for u, n in r100.items():
    assert n <= 16, 'UC100 %d excede 16 reglas: %d' % (u, n)

r511 = {}
for f in filas:
    r511[f['uc511']] = r511.get(f['uc511'], 0) + 2
for u, n in r511.items():
    assert n <= 16, 'UC511 %d excede 16 reglas: %d' % (u, n)

print('comprobado: 48 codigos unicos | UC100 %d equipos, reglas %s | UC511 %d equipos, %d reglas cada uno'
      % (len(r100), sorted(r100.values()), len(r511), sorted(r511.values())[0]))


def tabla_canales(u):
    r = ['| Channel ID | Name | Slave ID | Address | Quantity | Type | Byte Order | Sign |',
         '|---|---|---|---|---|---|---|---|']
    for f in filas:
        if f['uc100'] != u:
            continue
        r.append('| %d | Valvula_%02d | 1 | **%d** | 1 | Holding Register(INT16) | AB | sin marcar |'
                 % (f['canal'], f['v'], f['reg']))
    r.append('| 9 | Version_FW | 1 | **24** | 1 | Holding Register(INT16) | AB | sin marcar |')
    return '\n'.join(r)


def tabla_reglas(u):
    r = ['| Regla | IF: condicion | THEN: una sola accion | Delay Time | Lockout |',
         '|---|---|---|---|---|']
    n = 1
    for f in filas:
        if f['uc100'] != u:
            continue
        r.append('| %d | Channel %d **Above 0.5** | D2D `%s` = abrir V%02d | 0 | sin marcar |'
                 % (n, f['canal'], f['abrir'], f['v']))
        n += 1
        r.append('| %d | Channel %d **Below 0.5** | D2D `%s` = cerrar V%02d | 0 | sin marcar |'
                 % (n, f['canal'], f['cerrar'], f['v']))
        n += 1
    return '\n'.join(r)


def tabla_uc511():
    r = ['| UC511 | Valvulas | Salida 1 | Salida 2 | Codigos D2D a grabar |', '|---|---|---|---|---|']
    for u in range(1, VALVULAS // POR_UC511 + 1):
        g = [f for f in filas if f['uc511'] == u]
        a, b = g[0], g[1]
        r.append('| #%02d | V%02d y V%02d | V%02d | V%02d | `%s` abre / `%s` cierra V%02d &nbsp;&nbsp; `%s` abre / `%s` cierra V%02d |'
                 % (u, a['v'], b['v'], a['v'], b['v'],
                    a['abrir'], a['cerrar'], a['v'], b['abrir'], b['cerrar'], b['v']))
    return '\n'.join(r)


def tabla_mapa():
    r = ['| Valvula | Registro | Bus / UC100 | Canal | UC511 | Salida | Abrir | Cerrar |',
         '|---|---|---|---|---|---|---|---|']
    for f in filas:
        r.append('| V%02d | %d | Bus %d | %d | #%02d | %d | `%s` | `%s` |'
                 % (f['v'], f['reg'], f['uc100'], f['canal'], f['uc511'], f['slot'], f['abrir'], f['cerrar']))
    return '\n'.join(r)


PLANTILLA = u"""# Configuracion de radio D2D para 24 valvulas independientes

> Los 48 codigos D2D, el reparto de valvulas y las tablas de canales de este
> documento se GENERAN con `scripts/gen_configuracion_d2d.py`, que comprueba
> automaticamente la unicidad de los 48 codigos, el tope de 16 reglas por equipo
> y los rangos de canal. No se transcriben a mano.

Arquitectura elegida: **Milesight D2D sin gateway**. Las 24 valvulas estan
repartidas por la finca, de modo que no hay un punto comun donde concentrar los
accionamientos y cada pareja de valvulas necesita su propio nodo de radio.

## 0. Como cierra una valvula, y que pone el firmware

Una valvula cierra por **cualquiera de las dos**: la orden de cierre que manda el
UC100 cuando el tablero baja la entrada, o el temporizador `Duration` del UC511
como red de seguridad. El `Duration` va por encima del pulso de riego (10 min
para pulsos de 7 min), asi que en operacion normal nunca actua.

**Separacion de comandos por UC511 (spec 003, FR-218).** Medido en el piloto: un
UC511 pierde sin aviso el segundo de dos comandos que le lleguen con menos de 5 s
entre ellos. A equipos distintos no les pasa. Por eso el firmware del Mega nunca
publica dos cambios de valvulas del mismo UC511 con menos de **8 s**; lo que
llega dentro del intervalo espera su turno y, si el tablero lo revierte antes,
se descarta. Para eso el firmware necesita saber que valvulas comparten UC511:
el reparto de la seccion 6 tiene que coincidir con
`lib/wiga_config/installation_map.h` y con el cableado real.

Cada UC511 lleva ademas una marca de instalado en ese mismo archivo. Las valvulas
de un UC511 no instalado publican siempre 0 y el LCD las muestra con `-`: al
crecer desde el piloto hay que marcar como instalados los equipos nuevos.

## 1. Requisitos previos, sin los cuales nada de esto funciona

**Plan de frecuencias AU915, no US915.** En Colombia la banda de uso libre es
915-928 MHz, y todos los canales de subida de US915 caen entre 902,3 y
914,9 MHz. Poner `LoRaWAN Settings > Channel` = AU915 en los tres UC100 y en los
doce UC511.

**Clave D2D unica y compartida.** Es un ajuste global por equipo y no admite una
clave por pareja. La misma clave en los quince equipos, y distinta de la de
fabrica (`5572404C696E6B4C6F52613230313823`). El aislamiento entre valvulas lo
dan exclusivamente los 48 codigos distintos: un codigo repetido por error abre
dos valvulas a la vez y el fallo es silencioso.

**RX2 identica en los quince equipos**: misma `RX2 Frequency` y mismo
`RX2 Data Rate`. Con 48 codigos en juego conviene un DR alto (DR10 a DR13) para
bajar el tiempo al aire. DR8 es el mas lento, no el mas prudente. La eleccion es
alcance frente a ocupacion: en AU915 no hay limite legal de ciclo de trabajo.

**Los UC511 deben ser hardware 4.x.** La tabla oficial de agentes D2D lo
condiciona a `Any (Hardware Version 4.x)`, y D2D exige Clase C. El UC512 no
sirve: solo tiene Clase A/B.

**`Pass-through` desactivado** en los tres UC100. Con `Two-way Pass-through`
habilitado los canales Modbus dejan de funcionar y con ellos las reglas de
canal, sin ningun error visible.

## 2. Tres buses RS485, uno por UC100

Hacen falta **tres UC100** porque cada uno admite 16 reglas IF-THEN y 24
valvulas con apertura y cierre independientes son 48 disparadores.

Los tres NO pueden compartir el mismo par: cada UC100 es maestro Modbus y tres
maestros en un par colisionan. El ATmega2560 tiene cuatro UART y hoy solo usa
`Serial2`, asi que se tienden tres segmentos independientes.

| Bus | UART del Mega | Valvulas | Transceptor |
|---|---|---|---|
| Bus 1 | `Serial2`, pines 16/17, DE/RE en PH3 = pin 6 | V01 a V08 | el que ya esta montado |
| Bus 2 | `Serial1`, pines 18/19 | V09 a V16 | nuevo, con su pin DE/RE |
| Bus 3 | `Serial3`, pines 14/15 | V17 a V24 | nuevo, con su pin DE/RE |

**Cambio de firmware necesario**, modesto y de bajo riesgo: `ModbusSlave` no
tiene entrada/salida propia, asi que basta instanciarla tres veces sobre **el
mismo** `RegisterFile`, con tres adaptadores RS485. Los tres UC100 ven los
mismos 32 registros y cada uno lee solo los ocho que le tocan. Hay que comprobar
con la invariante MAP-V9 que los pines 14, 15, 18, 19 y los dos nuevos DE/RE no
los use ninguna entrada del tablero.

Cada bus lleva terminacion de 120 ohm en sus extremos (el UC100 la trae en un
microinterruptor) y **referencia de masa comun tendida por fuera del
conector**, porque la bornera RS485 del UC100 es de 2 pines y no tiene GND.

## 3. Codigo D2D: un esquema que no se puede confundir

Byte alto = numero de valvula (0x01 a 0x18). Byte bajo = accion (0x01 abrir,
0x00 cerrar). El codigo se lee de un vistazo y ningun byte alto vale 0x00.

    0x0701  ->  valvula 7, abrir
    0x0700  ->  valvula 7, cerrar

## 4. Canales Modbus

Identicos en los tres equipos salvo la columna `Address`. **`Address` es decimal
y base 0**: para el registro 0 se teclea `0`.

### UC100 del Bus 1, valvulas V01 a V08

{c1}

### UC100 del Bus 2, valvulas V09 a V16

{c2}

### UC100 del Bus 3, valvulas V17 a V24

{c3}

El canal 9 lee el registro de version del firmware, que es una constante
distinta de cero: sirve para confirmar con un solo `Fetch` que el enlace esta
vivo y que el direccionamiento es base 0.

`Basic Serial Settings` en los tres: Baud Rate `9600`, Data Bit `8 bits`, Parity
`None`, Stop Bit `1 bits`, Execution Interval `50` ms, Max Resp Time `500` ms,
Max Retry Times `3`.

## 5. Reglas IF-THEN

Cada valvula gasta dos reglas: `Above 0.5` abre y `Below 0.5` cierra. El umbral
es 0,5 porque el registro solo vale 0 o 1, y asi no depende de como compare el
UC100 la igualdad en coma flotante.

**Una sola accion por regla, sin `Delay Time`.** ToolBox no deja repetir la
misma accion en una regla ("Action can not be repeated"), asi que la rafaga de
tres envios no es configurable. Y el `Delay Time` no sirve para separar
comandos: medido en el piloto, si la condicion desaparece durante la espera la
accion se ejecuta igual. La separacion la hace el firmware (seccion 0).

### UC100 del Bus 1

{r1}

### UC100 del Bus 2

{r2}

### UC100 del Bus 3

{r3}

## 6. Reparto de valvulas por UC511

Cada UC511 gobierna dos valvulas y gasta cuatro de sus 16 reglas, asi que sobra
holgura para horarios o corte por volumen de agua.

{u511}

En cada UC511:

- Regla de apertura: `Time Control` habilitado, `Duration` = 600 s (10 min).
- Regla de cierre: sin temporizador.
- `When Power is Restored` = `Device ON, Valve Off`.
- `Auto-Confirmed Mechanism` desactivado.
- Clase **C**.

## 7. Tiempos, y por que estos valores

| Parametro | Donde | Valor | Motivo |
|---|---|---|---|
| `Lockout` | condicion de cada regla del UC100 | sin marcar | Cada cambio del registro dispara una orden. Si se reafirma con la condicion sostenida esta pendiente de medir (prueba B4 del piloto). |
| `Delay Time` | accion de cada regla del UC100 | 0 | No se cancela si la condicion desaparece. |
| Separacion por UC511 | firmware del Mega (`kCommandSpacingMs`) | 8 s | El UC511 pierde el segundo de dos comandos a menos de 5 s. |
| `Duration` | regla de apertura del UC511 | 600 s, 10 min | Red de seguridad por encima del pulso de 7 min. |
| `Reporting Interval` | General de cada UC100 | 1 minuto | Es el minimo configurable. NO dejarlo en los 20 de fabrica. |

Latencia medida en el piloto, tablero -> valvula: ~1 s. Con pulsos de 7 min en
secuencia por bloques, la segunda valvula de cada UC511 riega 6 min 52 s.

## 8. Mapa completo

{mapa}

## 9. Lista de la compra

| Cantidad | Modelo exacto | Nota |
|---|---|---|
| 12 | UC511, banda 915 MHz, **hardware 4.x** | Exigir la version de hardware al proveedor. No UC512. |
| 2 | UC100, banda 915 MHz | Ya hay uno; hacen falta tres en total. |
| 2 | transceptor RS485 (MAX485 o equivalente) | Para los buses 2 y 3. |

El **UC501-915M** comprado por error no entra en esta arquitectura: no
implementa D2D y sus salidas son GPIO logicos de 20 mA. Queda disponible como
nodo de caudalimetros, presostatos o sensores de humedad, para lo cual si
necesitaria un gateway.

## 10. Orden de puesta en marcha

1. Validar el piloto de 8 valvulas ([piloto-8-valvulas-d2d.md](piloto-8-valvulas-d2d.md)).
2. Confirmar el direccionamiento base 0 con el canal 9, `Address` 24.
3. Cambiar los quince equipos a AU915 y a la clave D2D propia.
4. Modificar el firmware para los tres buses RS485, y marcar como instalados
   los UC511 #05 a #12 en `lib/wiga_config/installation_map.h`.
5. Verificar que el cableado de cada par coincide con la seccion 6.
6. Configurar el UC100 del Bus 1 completo y probar las ocho primeras valvulas.
7. Replicar en los buses 2 y 3 con `Export` / `Import` de la configuracion JSON.
8. Prueba de una hora con caudal real antes de dejarlo desatendido.
"""

destino = sys.argv[1] if len(sys.argv) > 1 else 'docs/configuracion-radio-d2d.md'
doc = PLANTILLA.format(
    c1=tabla_canales(1), c2=tabla_canales(2), c3=tabla_canales(3),
    r1=tabla_reglas(1), r2=tabla_reglas(2), r3=tabla_reglas(3),
    u511=tabla_uc511(), mapa=tabla_mapa(),
)
os.makedirs(os.path.dirname(destino) or '.', exist_ok=True)
io.open(destino, 'w', encoding='utf-8').write(doc)
print('escrito %s, %d bytes' % (destino, len(doc)))
