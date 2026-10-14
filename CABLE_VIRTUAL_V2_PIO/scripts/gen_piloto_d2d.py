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
    r = ['| Regla | IF: condicion | THEN: 3 acciones con el MISMO codigo | Lock time |',
         '|---|---|---|---|']
    n = 1
    for f in filas:
        r.append('| %d | Channel %d **is True** | D2D `%s` = abrir V%02d, retardos 0 / 2000 / 4000 ms | 300000 |'
                 % (n, f['canal'], f['abrir'], f['v']))
        n += 1
        r.append('| %d | Channel %d **is False** | D2D `%s` = cerrar V%02d, retardos 0 / 2000 / 4000 ms | 300000 |'
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

## 0. Esto NO toca el firmware del ATmega

El Mega sigue siendo el mismo esclavo Modbus de siempre, en `Serial2`, sin
ningun cambio. Desde su lado no existe diferencia entre que le pregunte un
UC100 o tres: solo responde a quien le pregunte con el ID correcto. El cambio
de firmware para varios buses RS485 (necesario para llegar a 24 valvulas) NO
aplica a este piloto y se deja para cuando se escale.

## 1. Por que {n} valvulas y no mas, en esta fase

Con cierre EXPLICITO (el UC100 manda abrir y manda cerrar, sin depender del
temporizador del UC511) cada valvula gasta 2 reglas. {n} valvulas x 2 =
**{reglas} de las 16 reglas de un UC100**. {margen}

Este piloto NO depende de si el temporizador `Duration` del UC511 se rearma o
no, porque no se usa como mecanismo de cierre normal aqui: el UC100 cierra
explicitamente. El `Duration` se deja puesto solo como red de seguridad de
ultima instancia (ver seccion 6).

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

Si configuras el canal como `Coil` o `Discrete` en vez de `Holding Register`,
la condicion queda directamente en True/False en vez de un umbral en coma
flotante. El firmware lo admite: responde a FC01 y FC02 ademas de FC03/FC04.

**Las tres acciones de cada regla envian el MISMO codigo con retardos 0, 2000 y
4000 ms.** D2D no tiene acuse de recibo, asi que cada orden se emite tres veces
escalonada para que un paquete perdido no deje la valvula desincronizada.

## 6. Reparto de valvulas por UC511

{uc511_tabla}

En la regla de apertura de cada UC511, deja `Time Control` habilitado con
`Duration` = 900 s (15 minutos) como red de seguridad: si el enlace se cae con
la valvula abierta y ni la orden de cierre ni sus dos reintentos llegan, cierra
sola. La regla de cierre explicito no necesita temporizador.

## 7. Tiempos

| Parametro | Donde | Valor | Motivo |
|---|---|---|---|
| `Lock time` | condicion de cada regla del UC100 | 300000 ms, 5 min | Con 0 la condicion se detecta UNA sola vez y nunca se reafirma. |
| Retardos de las 3 acciones | cada regla del UC100 | 0 / 2000 / 4000 ms | Rafaga de tres envios sin acuse de recibo. |
| `Duration` | regla de apertura del UC511 | 900 s, 15 min | Red de seguridad, no el mecanismo de cierre normal. |
| `Reporting Interval` | General del UC100 | 1 minuto | Minimo configurable. NO dejarlo en los 20 de fabrica. |

**Antes de fijar el `Lock time`, mide la cadencia real de sondeo del UC100**
mirando la linea de estado del LCD: si se queda fija en `PEDIDO x TABLERO`, el
sondeo es mas rapido que 30 s; si alterna con `PEDIDO - SIN BUS`, es mas lento y
hay que subir estos tiempos.

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
5. Grabar en cada UC511 sus dos codigos D2D y el `Duration` de seguridad.
6. Probar valvula por valvula: energizar la entrada del tablero, cronometrar
   hasta que abre; desenergizar, cronometrar hasta que cierra.
7. Prueba de una hora con las {n} valvulas y caudal real antes de escalar.

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
