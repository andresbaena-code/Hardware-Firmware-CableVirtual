"""Genera docs/piloto-8-valvulas-d2d.md: piloto de N valvulas en UN solo UC100.

Mismo esquema de codigos D2D que el documento de despliegue completo
(docs/configuracion-radio-d2d.md), pero acotado a un unico UC100 sobre el bus
Serial2 ya existente. No hace falta ningun cambio de firmware para esto: solo
aplica cuando N*2 <= 16 reglas, que es el limite de UN UC100.

Las tablas se generan y se comprueban antes de escribir nada.
"""
import io
import os
import sys

VALVULAS = int(sys.argv[1]) if len(sys.argv) > 1 else 8
POR_UC511 = 2  # 2 interfaces de solenoide por equipo

assert VALVULAS * 2 <= 16, (
    'con %d valvulas y cierre explicito hacen falta %d reglas, mas de las 16 '
    'de un solo UC100. Esto ya no es un piloto de un equipo: usa el generador '
    'del despliegue completo (gen_configuracion_d2d.py).' % (VALVULAS, VALVULAS * 2)
)
assert 1 <= VALVULAS <= 24

filas = []
for v in range(1, VALVULAS + 1):
    filas.append(dict(
        v=v,
        reg=v - 1,
        canal=v,
        uc511=(v - 1) // POR_UC511 + 1,
        slot=1 if v % 2 == 1 else 2,
        abrir='%04X' % ((v << 8) | 0x01),
        cerrar='%04X' % ((v << 8) | 0x00),
    ))

# --- comprobaciones antes de escribir nada -----------------------------------
cods = [f['abrir'] for f in filas] + [f['cerrar'] for f in filas]
assert len(cods) == VALVULAS * 2
assert len(set(cods)) == len(cods), 'hay codigos D2D repetidos'
assert '0000' not in cods, 'el codigo 0000 puede confundirse con regla deshabilitada'
for f in filas:
    assert 1 <= f['canal'] <= 8, 'canal %d fuera de 1-8: no cabe en un solo UC100' % f['canal']

reglas_usadas = VALVULAS * 2
assert reglas_usadas <= 16
uc511_usados = (VALVULAS - 1) // POR_UC511 + 1
for u in range(1, uc511_usados + 1):
    n = sum(1 for f in filas if f['uc511'] == u)
    assert n <= POR_UC511

print('comprobado: %d codigos unicos | 1 UC100, %d/16 reglas | %d UC511, %d o menos valvulas cada uno'
      % (len(cods), reglas_usadas, uc511_usados, POR_UC511))


def tabla_canales():
    r = ['| Channel ID | Name | Slave ID | Address | Quantity | Type | Byte Order | Sign |',
         '|---|---|---|---|---|---|---|---|']
    for f in filas:
        r.append('| %d | Valvula_%02d | 1 | **%d** | 1 | Holding Register(INT16) | AB | sin marcar |'
                 % (f['canal'], f['v'], f['reg']))
    r.append('| 9 | Version_FW | 1 | **24** | 1 | Holding Register(INT16) | AB | sin marcar |')
    return '\n'.join(r)


def tabla_reglas():
    r = ['| Regla | IF: condicion | THEN: una sola accion | Delay Time | Lockout |',
         '|---|---|---|---|---|']
    n = 1
    for f in filas:
        r.append('| %d | Channel %d **Above 0.5** | D2D `%s` = abrir V%02d | 0 | sin marcar |'
                 % (n, f['canal'], f['abrir'], f['v']))
        n += 1
        r.append('| %d | Channel %d **Below 0.5** | D2D `%s` = cerrar V%02d | 0 | sin marcar |'
                 % (n, f['canal'], f['cerrar'], f['v']))
        n += 1
    r.append('| | | **%d de 16 reglas usadas** | |' % (n - 1))
    return '\n'.join(r)


def tabla_uc511():
    r = ['| UC511 | Valvulas | Salida 1 | Salida 2 | Codigos D2D a grabar |', '|---|---|---|---|---|']
    for u in range(1, uc511_usados + 1):
        g = [f for f in filas if f['uc511'] == u]
        if len(g) == 2:
            a, b = g
            r.append('| #%02d | V%02d y V%02d | V%02d | V%02d | `%s` abre / `%s` cierra V%02d &nbsp;&nbsp; `%s` abre / `%s` cierra V%02d |'
                     % (u, a['v'], b['v'], a['v'], b['v'],
                        a['abrir'], a['cerrar'], a['v'], b['abrir'], b['cerrar'], b['v']))
        else:
            a = g[0]
            r.append('| #%02d | V%02d (sola, salida 2 libre) | V%02d | libre | `%s` abre / `%s` cierra V%02d |'
                     % (u, a['v'], a['v'], a['abrir'], a['cerrar'], a['v']))
    return '\n'.join(r)


def tabla_mapa():
    r = ['| Valvula | Registro | Canal | UC511 | Salida | Abrir | Cerrar |',
         '|---|---|---|---|---|---|---|']
    for f in filas:
        r.append('| V%02d | %d | %d | #%02d | %d | `%s` | `%s` |'
                 % (f['v'], f['reg'], f['canal'], f['uc511'], f['slot'], f['abrir'], f['cerrar']))
    return '\n'.join(r)


PLANTILLA = u"""# Piloto: {n} valvulas independientes con un solo UC100

> Los codigos D2D y las tablas de este documento se GENERAN con
> `scripts/gen_piloto_d2d.py {n}`, que comprueba automaticamente la unicidad de
> los codigos y que {n} valvulas con cierre explicito (2 reglas cada una) caben
> en las 16 reglas de UN UC100. No se transcriben a mano.

Alcance: probar el enlace UC100 -> D2D -> UC511 -> valvula latch de extremo a
extremo, con {n} valvulas reales, ANTES de comprar el resto del material para
las 24. Usa el UC100 y el bus RS485 que YA ESTAN FUNCIONANDO.

## 0. Lo que pone el firmware del ATmega

El Mega es el mismo esclavo Modbus de siempre, en `Serial2`. Desde su lado no
existe diferencia entre que le pregunte un UC100 o tres. El cambio de firmware
para varios buses RS485 (necesario para llegar a 24 valvulas) NO aplica a este
piloto y se deja para cuando se escale.

Lo que SI aporta el firmware desde la spec 003 (FR-218) es la **separacion de
comandos por UC511**: un UC511 pierde sin aviso el segundo de dos comandos que
le lleguen demasiado juntos (medido: tras una apertura, un cierre a 8 s se
pierde y a 15 s llega), asi que el Mega nunca publica dos cambios de valvulas
del mismo UC511 con menos de **15 s**. En el cambio de V01 a V02 del mismo
bloque, V01 cierra al instante y V02 abre 15 s despues. Un pulso de menos de
15 s se alarga: el cierre sale 15 s despues de la apertura. Por eso el
reparto de valvulas por UC511 de la seccion 6 tiene que coincidir con el mapa
de `lib/wiga_config/installation_map.h` y con el cableado real.

Ademas, el firmware solo publica las valvulas de los UC511 marcados como
instalados (#01 a #{uc511_n} en este piloto). Las demas quedan en 0 y el LCD las
muestra con `-`.

## 1. Por que {n} valvulas y no mas, en esta fase

Con cierre EXPLICITO (el UC100 manda abrir y manda cerrar, sin depender del
temporizador del UC511) cada valvula gasta 2 reglas. {n} valvulas x 2 =
**{reglas} de las 16 reglas de un UC100**. {margen}

El cierre lo manda el UC100 cuando el tablero baja la entrada. El temporizador
`Duration` del UC511 queda como red de seguridad: la valvula cierra por
**cualquiera de las dos**, la orden de cierre o el tiempo (ver seccion 6).

## 2. Requisitos previos, identicos a los del despliegue completo

**Plan de frecuencias AU915, no US915.** En Colombia la banda de uso libre es
915-928 MHz; todos los canales de subida de US915 caen entre 902,3 y
914,9 MHz. Poner `LoRaWAN Settings > Channel` = AU915 en el UC100 y en cada
UC511 del piloto.

**Clave D2D unica y compartida**, distinta de la de fabrica
(`5572404C696E6B4C6F52613230313823`). Es un ajuste global por equipo: la misma
clave en el UC100 y en todos los UC511 del piloto.

**RX2 identica**: misma `RX2 Frequency` y mismo `RX2 Data Rate` en el UC100 y
en cada UC511.

**Los UC511 deben ser hardware 4.x.** La tabla oficial de agentes D2D lo
condiciona a `Any (Hardware Version 4.x)`. El UC512 no sirve para esto.

**`Pass-through` desactivado** en el UC100.

## 3. Codigo D2D

Mismo esquema que el despliegue completo, para que no haya que volver a
aprenderlo despues: byte alto = numero de valvula, byte bajo = accion (0x01
abrir, 0x00 cerrar).

    0x0301  ->  valvula 3, abrir
    0x0300  ->  valvula 3, cerrar

## 4. Canales Modbus, en el UC100 que ya tienes

`Address` es decimal y base 0: para el registro 0 se teclea `0`. Estas {n}
valvulas usan los registros 0 a {ultimo_reg}, que son las primeras {n} entradas
del tablero (fila E1 del LCD).

{canales}

El canal 9 lee el registro de version del firmware: sirve para confirmar con un
`Fetch` que el enlace esta vivo y que el direccionamiento es base 0, antes de
tocar nada de radio.

`Basic Serial Settings`: Baud Rate `9600`, Data Bit `8 bits`, Parity `None`,
Stop Bit `1 bits`, Execution Interval `50` ms, Max Resp Time `500` ms, Max
Retry Times `3`.

## 5. Reglas IF-THEN, {reglas} de 16

{reglas_tabla}

El umbral es 0,5 porque el registro solo vale 0 o 1: `Above 0.5` es "vale 1" y
`Below 0.5` es "vale 0", sin depender de como compare el UC100 la igualdad en
coma flotante.

**Una sola accion por regla, sin `Delay Time`.**

- ToolBox no deja repetir la misma accion en una regla ("Action can not be
  repeated"), asi que la rafaga de tres envios no es configurable.
- El `Delay Time` del UC100 NO sirve para separar comandos: medido en el
  piloto, si la condicion desaparece durante la espera la accion se ejecuta
  igual. La separacion la hace el firmware (seccion 0).

## 6. Reparto de valvulas por UC511

{uc511_tabla}

En cada UC511:

- Regla de apertura: `Time Control` habilitado con `Duration` = **600 s (10
  min)**. El riego normal dura 7 min y lo cierra la orden del tablero; si esa
  orden se pierde, el UC511 cierra solo a los 10 min.
- Regla de cierre: sin temporizador.
- `When Power is Restored` = `Device ON, Valve Off`.
- `Auto-Confirmed Mechanism` desactivado.
- Clase **C** (escucha permanente; en clase A no recibe las ordenes D2D).

## 7. Tiempos

| Parametro | Donde | Valor | Motivo |
|---|---|---|---|
| `Lockout` | condicion de cada regla del UC100 | sin marcar | Cada cambio del registro dispara una sola orden. Si se reafirma o no con la condicion sostenida esta pendiente de medir (prueba B4). |
| `Delay Time` | accion de cada regla del UC100 | 0 | No se cancela si la condicion desaparece: no sirve para separar comandos. |
| Separacion por UC511 | firmware del Mega (`kCommandSpacingMs`) | 15 s | Tras una apertura, el UC511 pierde un cierre a 8 s; a 15 s llega. |
| `Duration` | regla de apertura del UC511 | 600 s, 10 min | Red de seguridad por encima del pulso de 7 min. |
| `Reporting Interval` | General del UC100 | 1 minuto | Minimo configurable. NO dejarlo en los 20 de fabrica. |

**Efecto en el riego:** con pulsos de 7 min en secuencia, la primera valvula de
cada bloque riega 7 min y la segunda 6 min 45 s, porque abre 15 s despues de que
cierre su vecina del mismo UC511. Las valvulas de UC511 distintos no se esperan.

Latencia medida tablero -> valvula: ~1 s (el UC100 sondea casi continuamente).

## 8. Mapa completo del piloto

{mapa}

## 9. Lista de la compra para ESTE piloto

| Cantidad | Modelo exacto | Nota |
|---|---|---|
| {uc511_n} | UC511, banda 915 MHz, **hardware 4.x** | No UC512. |
| 0 | UC100 | Ya lo tienes, y este piloto no necesita otro. |

## 10. Orden de puesta en marcha

1. Cambiar el UC100 y los {uc511_n} UC511 a AU915 y a la clave D2D propia.
2. Confirmar el direccionamiento base 0 con el canal 9, `Address` 24.
3. Medir la cadencia de sondeo del UC100 con la linea de estado del LCD.
4. Dar de alta los {n} canales y las {reglas} reglas de la seccion 5.
5. Grabar en cada UC511 sus dos codigos D2D, el `Duration` de seguridad y los
   ajustes de la seccion 6.
6. Verificar que el cableado coincide con la seccion 8 (que V01 y V02 esten de
   verdad en las salidas 1 y 2 del UC511 #01, y asi con los demas).
7. Probar valvula por valvula: energizar la entrada del tablero, cronometrar
   hasta que abre; desenergizar, cronometrar hasta que cierra.
8. Probar el cambio dentro de un bloque (V01 -> V02) 10 veces: V01 cierra al
   instante, V02 abre ~15 s despues, las dos responden siempre.
9. Prueba de una hora con las {n} valvulas y caudal real antes de escalar.

Cuando el piloto quede validado, escalar a las 24 valvulas es el documento
[configuracion-radio-d2d.md](configuracion-radio-d2d.md): ese si necesita los
tres UC100 en tres buses RS485 separados y el cambio de firmware
correspondiente.
"""

destino = sys.argv[2] if len(sys.argv) > 2 else 'docs/piloto-8-valvulas-d2d.md'
margen = ('Cabe justo, sin ninguna regla de reserva.' if reglas_usadas == 16
          else 'Quedan %d reglas libres para lo que haga falta despues.' % (16 - reglas_usadas))
doc = PLANTILLA.format(
    n=VALVULAS, reglas=reglas_usadas, margen=margen, ultimo_reg=VALVULAS - 1,
    canales=tabla_canales(), reglas_tabla=tabla_reglas(), uc511_tabla=tabla_uc511(),
    mapa=tabla_mapa(), uc511_n=uc511_usados,
)
os.makedirs(os.path.dirname(destino) or '.', exist_ok=True)
io.open(destino, 'w', encoding='utf-8').write(doc)
print('escrito %s, %d bytes' % (destino, len(doc)))
