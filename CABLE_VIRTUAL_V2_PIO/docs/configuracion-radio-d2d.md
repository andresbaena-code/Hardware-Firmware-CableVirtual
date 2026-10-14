# Configuracion de radio D2D para 24 valvulas independientes

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

### UC100 del Bus 2, valvulas V09 a V16

| Channel ID | Name | Slave ID | Address | Quantity | Type | Byte Order | Sign |
|---|---|---|---|---|---|---|---|
| 1 | Valvula_09 | 1 | **8** | 1 | Holding Register(INT16) | AB | sin marcar |
| 2 | Valvula_10 | 1 | **9** | 1 | Holding Register(INT16) | AB | sin marcar |
| 3 | Valvula_11 | 1 | **10** | 1 | Holding Register(INT16) | AB | sin marcar |
| 4 | Valvula_12 | 1 | **11** | 1 | Holding Register(INT16) | AB | sin marcar |
| 5 | Valvula_13 | 1 | **12** | 1 | Holding Register(INT16) | AB | sin marcar |
| 6 | Valvula_14 | 1 | **13** | 1 | Holding Register(INT16) | AB | sin marcar |
| 7 | Valvula_15 | 1 | **14** | 1 | Holding Register(INT16) | AB | sin marcar |
| 8 | Valvula_16 | 1 | **15** | 1 | Holding Register(INT16) | AB | sin marcar |
| 9 | Version_FW | 1 | **24** | 1 | Holding Register(INT16) | AB | sin marcar |

### UC100 del Bus 3, valvulas V17 a V24

| Channel ID | Name | Slave ID | Address | Quantity | Type | Byte Order | Sign |
|---|---|---|---|---|---|---|---|
| 1 | Valvula_17 | 1 | **16** | 1 | Holding Register(INT16) | AB | sin marcar |
| 2 | Valvula_18 | 1 | **17** | 1 | Holding Register(INT16) | AB | sin marcar |
| 3 | Valvula_19 | 1 | **18** | 1 | Holding Register(INT16) | AB | sin marcar |
| 4 | Valvula_20 | 1 | **19** | 1 | Holding Register(INT16) | AB | sin marcar |
| 5 | Valvula_21 | 1 | **20** | 1 | Holding Register(INT16) | AB | sin marcar |
| 6 | Valvula_22 | 1 | **21** | 1 | Holding Register(INT16) | AB | sin marcar |
| 7 | Valvula_23 | 1 | **22** | 1 | Holding Register(INT16) | AB | sin marcar |
| 8 | Valvula_24 | 1 | **23** | 1 | Holding Register(INT16) | AB | sin marcar |
| 9 | Version_FW | 1 | **24** | 1 | Holding Register(INT16) | AB | sin marcar |

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

| Regla | IF: condicion | THEN: una sola accion | Delay Time | Lockout |
|---|---|---|---|---|
| 1 | Channel 1 **Above 0.5** | D2D `0101` = abrir V01 | 0 | sin marcar |
| 2 | Channel 1 **Below 0.5** | D2D `0100` = cerrar V01 | 0 | sin marcar |
| 3 | Channel 2 **Above 0.5** | D2D `0201` = abrir V02 | 0 | sin marcar |
| 4 | Channel 2 **Below 0.5** | D2D `0200` = cerrar V02 | 0 | sin marcar |
| 5 | Channel 3 **Above 0.5** | D2D `0301` = abrir V03 | 0 | sin marcar |
| 6 | Channel 3 **Below 0.5** | D2D `0300` = cerrar V03 | 0 | sin marcar |
| 7 | Channel 4 **Above 0.5** | D2D `0401` = abrir V04 | 0 | sin marcar |
| 8 | Channel 4 **Below 0.5** | D2D `0400` = cerrar V04 | 0 | sin marcar |
| 9 | Channel 5 **Above 0.5** | D2D `0501` = abrir V05 | 0 | sin marcar |
| 10 | Channel 5 **Below 0.5** | D2D `0500` = cerrar V05 | 0 | sin marcar |
| 11 | Channel 6 **Above 0.5** | D2D `0601` = abrir V06 | 0 | sin marcar |
| 12 | Channel 6 **Below 0.5** | D2D `0600` = cerrar V06 | 0 | sin marcar |
| 13 | Channel 7 **Above 0.5** | D2D `0701` = abrir V07 | 0 | sin marcar |
| 14 | Channel 7 **Below 0.5** | D2D `0700` = cerrar V07 | 0 | sin marcar |
| 15 | Channel 8 **Above 0.5** | D2D `0801` = abrir V08 | 0 | sin marcar |
| 16 | Channel 8 **Below 0.5** | D2D `0800` = cerrar V08 | 0 | sin marcar |

### UC100 del Bus 2

| Regla | IF: condicion | THEN: una sola accion | Delay Time | Lockout |
|---|---|---|---|---|
| 1 | Channel 1 **Above 0.5** | D2D `0901` = abrir V09 | 0 | sin marcar |
| 2 | Channel 1 **Below 0.5** | D2D `0900` = cerrar V09 | 0 | sin marcar |
| 3 | Channel 2 **Above 0.5** | D2D `0A01` = abrir V10 | 0 | sin marcar |
| 4 | Channel 2 **Below 0.5** | D2D `0A00` = cerrar V10 | 0 | sin marcar |
| 5 | Channel 3 **Above 0.5** | D2D `0B01` = abrir V11 | 0 | sin marcar |
| 6 | Channel 3 **Below 0.5** | D2D `0B00` = cerrar V11 | 0 | sin marcar |
| 7 | Channel 4 **Above 0.5** | D2D `0C01` = abrir V12 | 0 | sin marcar |
| 8 | Channel 4 **Below 0.5** | D2D `0C00` = cerrar V12 | 0 | sin marcar |
| 9 | Channel 5 **Above 0.5** | D2D `0D01` = abrir V13 | 0 | sin marcar |
| 10 | Channel 5 **Below 0.5** | D2D `0D00` = cerrar V13 | 0 | sin marcar |
| 11 | Channel 6 **Above 0.5** | D2D `0E01` = abrir V14 | 0 | sin marcar |
| 12 | Channel 6 **Below 0.5** | D2D `0E00` = cerrar V14 | 0 | sin marcar |
| 13 | Channel 7 **Above 0.5** | D2D `0F01` = abrir V15 | 0 | sin marcar |
| 14 | Channel 7 **Below 0.5** | D2D `0F00` = cerrar V15 | 0 | sin marcar |
| 15 | Channel 8 **Above 0.5** | D2D `1001` = abrir V16 | 0 | sin marcar |
| 16 | Channel 8 **Below 0.5** | D2D `1000` = cerrar V16 | 0 | sin marcar |

### UC100 del Bus 3

| Regla | IF: condicion | THEN: una sola accion | Delay Time | Lockout |
|---|---|---|---|---|
| 1 | Channel 1 **Above 0.5** | D2D `1101` = abrir V17 | 0 | sin marcar |
| 2 | Channel 1 **Below 0.5** | D2D `1100` = cerrar V17 | 0 | sin marcar |
| 3 | Channel 2 **Above 0.5** | D2D `1201` = abrir V18 | 0 | sin marcar |
| 4 | Channel 2 **Below 0.5** | D2D `1200` = cerrar V18 | 0 | sin marcar |
| 5 | Channel 3 **Above 0.5** | D2D `1301` = abrir V19 | 0 | sin marcar |
| 6 | Channel 3 **Below 0.5** | D2D `1300` = cerrar V19 | 0 | sin marcar |
| 7 | Channel 4 **Above 0.5** | D2D `1401` = abrir V20 | 0 | sin marcar |
| 8 | Channel 4 **Below 0.5** | D2D `1400` = cerrar V20 | 0 | sin marcar |
| 9 | Channel 5 **Above 0.5** | D2D `1501` = abrir V21 | 0 | sin marcar |
| 10 | Channel 5 **Below 0.5** | D2D `1500` = cerrar V21 | 0 | sin marcar |
| 11 | Channel 6 **Above 0.5** | D2D `1601` = abrir V22 | 0 | sin marcar |
| 12 | Channel 6 **Below 0.5** | D2D `1600` = cerrar V22 | 0 | sin marcar |
| 13 | Channel 7 **Above 0.5** | D2D `1701` = abrir V23 | 0 | sin marcar |
| 14 | Channel 7 **Below 0.5** | D2D `1700` = cerrar V23 | 0 | sin marcar |
| 15 | Channel 8 **Above 0.5** | D2D `1801` = abrir V24 | 0 | sin marcar |
| 16 | Channel 8 **Below 0.5** | D2D `1800` = cerrar V24 | 0 | sin marcar |

## 6. Reparto de valvulas por UC511

Cada UC511 gobierna dos valvulas y gasta cuatro de sus 16 reglas, asi que sobra
holgura para horarios o corte por volumen de agua.

| UC511 | Valvulas | Salida 1 | Salida 2 | Codigos D2D a grabar |
|---|---|---|---|---|
| #01 | V01 y V02 | V01 | V02 | `0101` abre / `0100` cierra V01 &nbsp;&nbsp; `0201` abre / `0200` cierra V02 |
| #02 | V03 y V04 | V03 | V04 | `0301` abre / `0300` cierra V03 &nbsp;&nbsp; `0401` abre / `0400` cierra V04 |
| #03 | V05 y V06 | V05 | V06 | `0501` abre / `0500` cierra V05 &nbsp;&nbsp; `0601` abre / `0600` cierra V06 |
| #04 | V07 y V08 | V07 | V08 | `0701` abre / `0700` cierra V07 &nbsp;&nbsp; `0801` abre / `0800` cierra V08 |
| #05 | V09 y V10 | V09 | V10 | `0901` abre / `0900` cierra V09 &nbsp;&nbsp; `0A01` abre / `0A00` cierra V10 |
| #06 | V11 y V12 | V11 | V12 | `0B01` abre / `0B00` cierra V11 &nbsp;&nbsp; `0C01` abre / `0C00` cierra V12 |
| #07 | V13 y V14 | V13 | V14 | `0D01` abre / `0D00` cierra V13 &nbsp;&nbsp; `0E01` abre / `0E00` cierra V14 |
| #08 | V15 y V16 | V15 | V16 | `0F01` abre / `0F00` cierra V15 &nbsp;&nbsp; `1001` abre / `1000` cierra V16 |
| #09 | V17 y V18 | V17 | V18 | `1101` abre / `1100` cierra V17 &nbsp;&nbsp; `1201` abre / `1200` cierra V18 |
| #10 | V19 y V20 | V19 | V20 | `1301` abre / `1300` cierra V19 &nbsp;&nbsp; `1401` abre / `1400` cierra V20 |
| #11 | V21 y V22 | V21 | V22 | `1501` abre / `1500` cierra V21 &nbsp;&nbsp; `1601` abre / `1600` cierra V22 |
| #12 | V23 y V24 | V23 | V24 | `1701` abre / `1700` cierra V23 &nbsp;&nbsp; `1801` abre / `1800` cierra V24 |

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

| Valvula | Registro | Bus / UC100 | Canal | UC511 | Salida | Abrir | Cerrar |
|---|---|---|---|---|---|---|---|
| V01 | 0 | Bus 1 | 1 | #01 | 1 | `0101` | `0100` |
| V02 | 1 | Bus 1 | 2 | #01 | 2 | `0201` | `0200` |
| V03 | 2 | Bus 1 | 3 | #02 | 1 | `0301` | `0300` |
| V04 | 3 | Bus 1 | 4 | #02 | 2 | `0401` | `0400` |
| V05 | 4 | Bus 1 | 5 | #03 | 1 | `0501` | `0500` |
| V06 | 5 | Bus 1 | 6 | #03 | 2 | `0601` | `0600` |
| V07 | 6 | Bus 1 | 7 | #04 | 1 | `0701` | `0700` |
| V08 | 7 | Bus 1 | 8 | #04 | 2 | `0801` | `0800` |
| V09 | 8 | Bus 2 | 1 | #05 | 1 | `0901` | `0900` |
| V10 | 9 | Bus 2 | 2 | #05 | 2 | `0A01` | `0A00` |
| V11 | 10 | Bus 2 | 3 | #06 | 1 | `0B01` | `0B00` |
| V12 | 11 | Bus 2 | 4 | #06 | 2 | `0C01` | `0C00` |
| V13 | 12 | Bus 2 | 5 | #07 | 1 | `0D01` | `0D00` |
| V14 | 13 | Bus 2 | 6 | #07 | 2 | `0E01` | `0E00` |
| V15 | 14 | Bus 2 | 7 | #08 | 1 | `0F01` | `0F00` |
| V16 | 15 | Bus 2 | 8 | #08 | 2 | `1001` | `1000` |
| V17 | 16 | Bus 3 | 1 | #09 | 1 | `1101` | `1100` |
| V18 | 17 | Bus 3 | 2 | #09 | 2 | `1201` | `1200` |
| V19 | 18 | Bus 3 | 3 | #10 | 1 | `1301` | `1300` |
| V20 | 19 | Bus 3 | 4 | #10 | 2 | `1401` | `1400` |
| V21 | 20 | Bus 3 | 5 | #11 | 1 | `1501` | `1500` |
| V22 | 21 | Bus 3 | 6 | #11 | 2 | `1601` | `1600` |
| V23 | 22 | Bus 3 | 7 | #12 | 1 | `1701` | `1700` |
| V24 | 23 | Bus 3 | 8 | #12 | 2 | `1801` | `1800` |

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
