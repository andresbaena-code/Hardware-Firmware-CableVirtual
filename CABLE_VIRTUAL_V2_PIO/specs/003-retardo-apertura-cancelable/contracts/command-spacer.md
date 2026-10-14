# Contrato: separador de comandos por UC511

**Spec**: [../spec.md](../spec.md) · **Modelo**: [../data-model.md](../data-model.md)

Este contrato fija el comportamiento observable de `CommandSpacer` y de su integración
en `ValvePublisher`. El mapa de registros Modbus de la spec 002
([register-map.md](../../002-modbus-esclavo-rs485/contracts/register-map.md)) **no
cambia**: solo cambia **cuándo** un registro de válvula toma su nuevo valor.

## 1. Interfaz (forma, no implementación)

```text
CommandSpacer
  begin(published[kValveCount], nowMs)
      Siembra el estado publicado y marca como ocupados los equipos con
      alguna válvula en Open.
  update(index, desired, nowMs) -> Outcome { Unchanged, Queued, Cancelled }
      Informa el deseado de una válvula. Crea o cancela su pendiente.
  next(nowMs, &index, &state) -> bool
      Devuelve el siguiente cambio autorizado en esta vuelta y lo da por
      publicado. Llamar hasta que devuelva false.
  published(index) -> ValveState
  pending(index) -> bool
```

Sin punteros a puertos, sin `<Arduino.h>`, sin memoria dinámica. El tiempo entra como
argumento.

## 2. Reglas

| Id | Regla | Requisito |
|---|---|---|
| SP-01 | Un cambio cuyo equipo está libre se autoriza en la misma vuelta en que se pide | FR-205 |
| SP-02 | Un cambio cuyo equipo tuvo otro cambio hace menos de `kCommandSpacingMs` queda pendiente | FR-204 |
| SP-03 | Un pendiente se cancela si el deseado vuelve al publicado; no consume turno | FR-206 |
| SP-04 | Por equipo y vuelta se autoriza como máximo un cambio: cierre antes que apertura, luego el más antiguo, luego el menor índice | FR-207 |
| SP-05 | Equipos distintos no se esperan entre sí | FR-208 |
| SP-06 | Nunca se autoriza un estado distinto del deseado vigente | FR-209 |
| SP-07 | El intervalo se mide con `wiga::elapsed()` | FR-210 |
| SP-08 | `begin()` no espera y ocupa los equipos con alguna válvula en `Open` | FR-211 |
| SP-09 | Ningún pendiente espera `3 × kCommandSpacingMs` o más | caso borde |

## 3. Integración en `ValvePublisher`

| Id | Regla | Requisito |
|---|---|---|
| VP-31 | El deseado es `panelRequest(nivel estable)` si el equipo está instalado, `Closed` si no | FR-212 |
| VP-32 | El banco de registros solo se escribe con los cambios que devuelve `next()` | FR-204, FR-209 |
| VP-33 | Traza `TX` al publicar, `WAIT` si un cambio queda pendiente al final de la vuelta, `CANC` al cancelar, `NOINST` cuando el antirrebote confirma un pedido de riego en un equipo no instalado | FR-213, FR-216 |
| VP-34 | El panel muestra `-` para válvulas de equipos no instalados y el valor publicado para las demás | FR-215 |
| VP-35 | `begin()` siembra registros y separador antes de atender el bus | FR-211, FR-116 (002) |

## 4. Mapa de instalación

| Id | Regla | Requisito |
|---|---|---|
| MAP-V13 | `actuator` en 1..`kActuatorCount` | FR-202 |
| MAP-V14 | `output` es 1 o 2 | FR-202 |
| MAP-V15 | no hay dos válvulas con el mismo (`actuator`, `output`) | FR-202 |
| MAP-V16 | ningún equipo tiene más de 2 válvulas | FR-202 |
| MAP-V17 | todo equipo declarado tiene al menos una válvula | FR-202 |
| TC-01 | `kCommandSpacingMs >= kActuatorMinGapMs` | FR-214 |

## 5. Vectores de prueba (host)

Tiempos escritos para un intervalo de 8000 ms; las pruebas usan `kCommandSpacingMs` (hoy 15000 ms) y escalan igual. V01/V02 en el equipo 1, V03 en el equipo 2. Antirrebote ya resuelto.

| # | Secuencia | Esperado |
|---|---|---|
| 1 | t=0 sube V01 (equipo libre) | V01=`1` en t=0 |
| 2 | V01=`1` desde t=0; t=60000 baja V01 y sube V02 | V01=`0` en t=60000; V02=`1` en t=68000, `0` en t=67999 |
| 3 | V02 detectada una vuelta antes que la bajada de V01 (ambas dentro del intervalo de un cambio previo) | primero V01=`0`, 8 s después V02=`1` |
| 4 | V02 baja y sube V03 a la vez | ambos cambian en la misma vuelta |
| 5 | apertura de V02 pendiente; V02 baja antes del turno | V02 nunca vale `1`; traza `CANC` |
| 6 | V01=`1` publicada en t; V01 baja en t+3000 | V01=`0` en t+8000 |
| 7 | cierre de V01 pendiente; V01 vuelve a subir antes del turno | V01 sigue en `1` sin pasar por `0` |
| 8 | V01 y V02 suben en la misma vuelta | V01 en t, V02 en t+8000 |
| 9 | último cambio en t=0xFFFFF000; nuevo pedido en t+1000 | se publica en 0xFFFFF000 + 8000 (con desbordamiento) |
| 10 | arranque con V01 en `1`; V02 sube en t=2000 | V01=`1` antes del primer sondeo; V02=`1` en t=8000 |
| 11 | sube V09 (equipo 5 no instalado) | registro 8 sigue en `0`; una traza `NOINST`; panel `-` |
| 12 | peor caso de espera (pendientes cruzados en un equipo) | ningún pendiente llega a 3 intervalos |
