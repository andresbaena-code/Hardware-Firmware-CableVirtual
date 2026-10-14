# Piloto: 8 valvulas independientes con un solo UC100

> Los codigos D2D y las tablas de este documento se GENERAN con
> `scripts/gen_piloto_d2d.py 8`, que comprueba automaticamente la unicidad de
> los codigos y que 8 valvulas con cierre explicito (2 reglas cada una) caben
> en las 16 reglas de UN UC100. No se transcriben a mano.

Alcance: probar el enlace UC100 -> D2D -> UC511 -> valvula latch de extremo a
extremo, con 8 valvulas reales, ANTES de comprar el resto del material para
las 24. Usa el UC100 y el bus RS485 que YA ESTAN FUNCIONANDO.

## 0. Esto NO toca el firmware del ATmega

El Mega sigue siendo el mismo esclavo Modbus de siempre, en `Serial2`, sin
ningun cambio. Desde su lado no existe diferencia entre que le pregunte un
UC100 o tres: solo responde a quien le pregunte con el ID correcto. El cambio
de firmware para varios buses RS485 (necesario para llegar a 24 valvulas) NO
aplica a este piloto y se deja para cuando se escale.

## 1. Por que 8 valvulas y no mas, en esta fase

Con cierre EXPLICITO (el UC100 manda abrir y manda cerrar, sin depender del
temporizador del UC511) cada valvula gasta 2 reglas. 8 valvulas x 2 =
**16 de las 16 reglas de un UC100**. Cabe justo, sin ninguna regla de reserva.

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

`Address` es decimal y base 0: para el registro 0 se teclea `0`. Estas 8
valvulas usan los registros 0 a 7, que son las primeras 8 entradas
del tablero (fila E1 del LCD).

| Channel ID | Name | Slave ID | Address | Quantity | Type | Byte Order | Sign |
|---|---|---|---|---|---|---|---|
| 1 | Valvula_01 | 1 | **0** | 1 | Holding Register(INT16) | AB | sin marcar |
| 2 | Valvula_02 | 1 | **1** | 1 | Holding Register(INT16) | AB | sin marcar |
| 3 | Valvula_03 | 1 | **2** | 1 | Holding Register(INT16) | AB | sin marcar |
| 4 | Valvula_04 | 1 | **3** | 1 | Holding Register(INT16) | AB | sin marcar |
| 5 | Valvula_05 | 1 | **4** | 1 | Holding Register(INT16) | AB | sin marcar |
| 6 | Valvula_06 | 1 | **5** | 1 | Holding Register(INT16) | AB | sin marcar |
| 7 | Valvula_07 | 1 | **6** | 1 | Holding Register(INT16) | AB | sin marcar |
| 8 | Valvula_08 | 1 | **7** | 1 | Holding Register(INT16) | AB | sin marcar |
| 9 | Version_FW | 1 | **24** | 1 | Holding Register(INT16) | AB | sin marcar |

El canal 9 lee el registro de version del firmware: sirve para confirmar con un
`Fetch` que el enlace esta vivo y que el direccionamiento es base 0, antes de
tocar nada de radio.

`Basic Serial Settings`: Baud Rate `9600`, Data Bit `8 bits`, Parity `None`,
Stop Bit `1 bits`, Execution Interval `50` ms, Max Resp Time `500` ms, Max
Retry Times `3`.

## 5. Reglas IF-THEN, 16 de 16

| Regla | IF: condicion | THEN: 3 acciones con el MISMO codigo | Lock time |
|---|---|---|---|
| 1 | Channel 1 **is True** | D2D `0101` = abrir V01, retardos 0 / 2000 / 4000 ms | 300000 |
| 2 | Channel 1 **is False** | D2D `0100` = cerrar V01, retardos 0 / 2000 / 4000 ms | 300000 |
| 3 | Channel 2 **is True** | D2D `0201` = abrir V02, retardos 0 / 2000 / 4000 ms | 300000 |
| 4 | Channel 2 **is False** | D2D `0200` = cerrar V02, retardos 0 / 2000 / 4000 ms | 300000 |
| 5 | Channel 3 **is True** | D2D `0301` = abrir V03, retardos 0 / 2000 / 4000 ms | 300000 |
| 6 | Channel 3 **is False** | D2D `0300` = cerrar V03, retardos 0 / 2000 / 4000 ms | 300000 |
| 7 | Channel 4 **is True** | D2D `0401` = abrir V04, retardos 0 / 2000 / 4000 ms | 300000 |
| 8 | Channel 4 **is False** | D2D `0400` = cerrar V04, retardos 0 / 2000 / 4000 ms | 300000 |
| 9 | Channel 5 **is True** | D2D `0501` = abrir V05, retardos 0 / 2000 / 4000 ms | 300000 |
| 10 | Channel 5 **is False** | D2D `0500` = cerrar V05, retardos 0 / 2000 / 4000 ms | 300000 |
| 11 | Channel 6 **is True** | D2D `0601` = abrir V06, retardos 0 / 2000 / 4000 ms | 300000 |
| 12 | Channel 6 **is False** | D2D `0600` = cerrar V06, retardos 0 / 2000 / 4000 ms | 300000 |
| 13 | Channel 7 **is True** | D2D `0701` = abrir V07, retardos 0 / 2000 / 4000 ms | 300000 |
| 14 | Channel 7 **is False** | D2D `0700` = cerrar V07, retardos 0 / 2000 / 4000 ms | 300000 |
| 15 | Channel 8 **is True** | D2D `0801` = abrir V08, retardos 0 / 2000 / 4000 ms | 300000 |
| 16 | Channel 8 **is False** | D2D `0800` = cerrar V08, retardos 0 / 2000 / 4000 ms | 300000 |
| | | **16 de 16 reglas usadas** | |

Si configuras el canal como `Coil` o `Discrete` en vez de `Holding Register`,
la condicion queda directamente en True/False en vez de un umbral en coma
flotante. El firmware lo admite: responde a FC01 y FC02 ademas de FC03/FC04.

**Las tres acciones de cada regla envian el MISMO codigo con retardos 0, 2000 y
4000 ms.** D2D no tiene acuse de recibo, asi que cada orden se emite tres veces
escalonada para que un paquete perdido no deje la valvula desincronizada.

## 6. Reparto de valvulas por UC511

| UC511 | Valvulas | Salida 1 | Salida 2 | Codigos D2D a grabar |
|---|---|---|---|---|
| #01 | V01 y V02 | V01 | V02 | `0101` abre / `0100` cierra V01 &nbsp;&nbsp; `0201` abre / `0200` cierra V02 |
| #02 | V03 y V04 | V03 | V04 | `0301` abre / `0300` cierra V03 &nbsp;&nbsp; `0401` abre / `0400` cierra V04 |
| #03 | V05 y V06 | V05 | V06 | `0501` abre / `0500` cierra V05 &nbsp;&nbsp; `0601` abre / `0600` cierra V06 |
| #04 | V07 y V08 | V07 | V08 | `0701` abre / `0700` cierra V07 &nbsp;&nbsp; `0801` abre / `0800` cierra V08 |

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

| Valvula | Registro | Canal | UC511 | Salida | Abrir | Cerrar |
|---|---|---|---|---|---|---|
| V01 | 0 | 1 | #01 | 1 | `0101` | `0100` |
| V02 | 1 | 2 | #01 | 2 | `0201` | `0200` |
| V03 | 2 | 3 | #02 | 1 | `0301` | `0300` |
| V04 | 3 | 4 | #02 | 2 | `0401` | `0400` |
| V05 | 4 | 5 | #03 | 1 | `0501` | `0500` |
| V06 | 5 | 6 | #03 | 2 | `0601` | `0600` |
| V07 | 6 | 7 | #04 | 1 | `0701` | `0700` |
| V08 | 7 | 8 | #04 | 2 | `0801` | `0800` |

## 9. Lista de la compra para ESTE piloto

| Cantidad | Modelo exacto | Nota |
|---|---|---|
| 4 | UC511, banda 915 MHz, **hardware 4.x** | No UC512. |
| 0 | UC100 | Ya lo tienes, y este piloto no necesita otro. |

## 10. Orden de puesta en marcha

1. Cambiar el UC100 y los 4 UC511 a AU915 y a la clave D2D propia.
2. Confirmar el direccionamiento base 0 con el canal 9, `Address` 24.
3. Medir la cadencia de sondeo del UC100 con la linea de estado del LCD.
4. Dar de alta los 8 canales y las 16 reglas de la seccion 5.
5. Grabar en cada UC511 sus dos codigos D2D y el `Duration` de seguridad.
6. Probar valvula por valvula: energizar la entrada del tablero, cronometrar
   hasta que abre; desenergizar, cronometrar hasta que cierra.
7. Prueba de una hora con las 8 valvulas y caudal real antes de escalar.

Cuando el piloto quede validado, escalar a las 24 valvulas es el documento
[configuracion-radio-d2d.md](configuracion-radio-d2d.md): ese si necesita los
tres UC100 en tres buses RS485 separados y el cambio de firmware
correspondiente.
