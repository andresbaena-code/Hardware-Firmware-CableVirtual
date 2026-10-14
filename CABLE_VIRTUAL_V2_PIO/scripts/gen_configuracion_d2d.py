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
    r = ['| Regla | IF: condicion | THEN: 3 acciones con el MISMO codigo | Lock time |',
         '|---|---|---|---|']
    n = 1
    for f in filas:
        if f['uc100'] != u:
            continue
        r.append('| %d | Channel %d **is True** | D2D `%s` = abrir V%02d, retardos 0 / 2000 / 4000 ms | 300000 |'
                 % (n, f['canal'], f['abrir'], f['v']))
        n += 1
        r.append('| %d | Channel %d **is False** | D2D `%s` = cerrar V%02d, retardos 0 / 2000 / 4000 ms | 300000 |'
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

## 0. LO PRIMERO: un comportamiento sin documentar del que depende la seguridad

Toda la red de seguridad de este diseno descansa en el temporizador `Duration`
del UC511. **Ningun manual de Milesight dice si una nueva orden de apertura
REARMA ese temporizador mientras esta contando.** Hay que medirlo con un UC511
en la mesa antes de instalar nada:

1. Graba una regla de apertura con `Time Control` habilitado y `Duration` = 60 s.
2. Envia la orden de apertura. Cuenta 30 segundos.
3. Envia la MISMA orden otra vez.
4. Cronometra cuando cierra la valvula.

- Cierra a los **90 s** desde el primer envio: el temporizador SE REARMA y el
  diseno de abajo es valido tal cual.
- Cierra a los **60 s**: NO se rearma. Entonces la reafirmacion periodica no
  sostiene la valvula abierta, cerraria cada `Duration` y volveria a abrir en la
  siguiente reafirmacion, con aleteo mecanico. Habria que subir `Duration` por
  encima del riego mas largo previsto y aceptar que el unico cierre rapido es la
  regla explicita de cierre.

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

Cada valvula gasta dos reglas, una por `is True` y otra por `is False`. Si
configuras el canal como `Coil` o `Discrete` en vez de `Holding Register`, la
condicion es directamente True/False en vez de un umbral en coma flotante, que
es mas limpio; el firmware lo admite porque responde a FC01 y FC02 ademas de
FC03 y FC04.

**Las tres acciones de cada regla envian el MISMO codigo con retardos 0, 2000 y
4000 ms.** No es un error de copia: D2D no tiene acuse de recibo, asi que cada
orden se emite tres veces escalonada para que un paquete perdido no deje la
valvula desincronizada. Las tres ranuras de accion existen justo para esto.

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

En cada UC511 la regla de apertura lleva `Time Control` habilitado y `Duration`
= 900 s. La de cierre no necesita temporizador.

## 7. Tiempos, y por que estos valores

| Parametro | Donde | Valor | Motivo |
|---|---|---|---|
| `Lock time` | condicion de cada regla del UC100 | 300000 ms, 5 min | Con 0 la condicion se detecta UNA sola vez y nunca se reafirma. Es el unico mecanismo de reafirmacion del sistema. |
| Retardos de las 3 acciones | cada regla del UC100 | 0 / 2000 / 4000 ms | Rafaga de tres envios sin acuse de recibo, escalonada para no colisionar en el aire. |
| `Duration` | regla de apertura del UC511 | 900 s, 15 min | Red de seguridad final: si el enlace muere con la valvula abierta, cierra sola. Tolera dos reafirmaciones perdidas seguidas y acota la inundacion a 15 minutos. |
| `Reporting Interval` | General de cada UC100 | 1 minuto | Es el minimo configurable. NO dejarlo en los 20 de fabrica. |

**El `Lock time` no puede ser mas rapido que la cadencia de sondeo del UC100**,
porque una regla solo se reevalua cuando llegan datos nuevos del canal. Esa
cadencia esta pendiente de medir: conecta el UC100 y mira la linea de estado del
LCD. Si se queda fija en `PEDIDO x TABLERO`, el sondeo es mas rapido que 30 s;
si alterna con `PEDIDO - SIN BUS`, es mas lento y hay que subir estos tiempos.

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

1. Medir si el temporizador `Duration` del UC511 se rearma (seccion 0).
2. Medir la cadencia de sondeo del UC100 con la linea de estado del LCD.
3. Confirmar el direccionamiento base 0 con el canal 9, `Address` 24.
4. Cambiar los quince equipos a AU915 y a la clave D2D propia.
5. Modificar el firmware para los tres buses RS485.
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
