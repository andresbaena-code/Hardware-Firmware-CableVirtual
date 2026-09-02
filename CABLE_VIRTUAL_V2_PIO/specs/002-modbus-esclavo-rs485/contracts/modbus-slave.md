# Contrato — Comportamiento del esclavo Modbus en el bus

**Feature**: 002-modbus-esclavo-rs485 | **Fecha**: 2026-08-27

Complementa a [register-map.md](register-map.md), que define *qué* se publica. Este
documento define *cómo* se comporta el controlador en el cable.

---

## 1. Interfaces internas

### 1.1 `IByteStream` — puerto nuevo (`lib/wiga_ports/i_byte_stream.h`)

Aísla la capa pura del hardware serie. Es lo único que el protocolo necesita del mundo.

| Operación | Contrato |
|---|---|
| `begin()` | Abre el puerto y deja el transceptor en **recepción** |
| `available()` | Bytes disponibles para leer. **No bloquea** |
| `read()` | Un byte. Solo válido si `available() > 0` |
| `beginTransmission(len)` | Toma la línea: afirma DE y prepara el envío de `len` bytes |
| `write(data, len)` | Entrega los bytes. **No espera** a que salgan del cable |
| `transmitting()` | `true` mientras la trama siga saliendo |
| `endTransmission()` | Libera la línea. Solo tiene efecto si `transmitting()` es `false` |

**Regla de oro del puerto**: ninguna operación puede bloquear más de lo que cuesta mover
un byte a un búfer. La espera de que la trama salga del cable se resuelve **entre vueltas
del lazo**, consultando `transmitting()`, nunca dentro de una llamada.

### 1.2 `ModbusSlave` — capa pura (`lib/wiga_modbus/modbus_slave.h`)

Sin entrada/salida propia. Bytes entran, bytes salen. Es lo que la hace comprobable en
`env:native`.

```
   adaptador                       ModbusSlave (puro)
   ─────────                       ──────────────────
   available()/read()  ──bytes──▶  onByte(b, nowMs)
                                   poll(nowMs)
   write()             ◀──bytes──  hasResponse() / response()
                                   responseSent()
```

---

## 2. Máquina de recepción

```
                 ┌─────────┐
                 │  Idle   │◀──────────────────────────────┐
                 └────┬────┘                               │
                      │ llega un byte                      │
                      ▼                                    │
                 ┌─────────────┐                           │
        ┌───────▶│  Receiving  │                           │
        │        └──────┬──────┘                           │
        │ más bytes     │                                  │
        └───────────────┤                                  │
                        │ se alcanzó la longitud esperada  │
                        ▼                                  │
                 ┌──────────────┐   descarte silencioso    │
                 │  Validating  │──────────────────────────┤
                 └──────┬───────┘   (otro ID, CRC malo)    │
                        │ válida y para mí                 │
                        ▼                                  │
                 ┌──────────────┐                          │
                 │  Responding  │──────────────────────────┘
                 └──────────────┘   respuesta transmitida

   Desde Receiving, un silencio de 4 ms con trama parcial descarta y vuelve a Idle.
```

**Longitud esperada.** Las cuatro funciones soportadas usan peticiones de **8 bytes
fijos**, de modo que la longitud se conoce en cuanto se lee el código de función. El
silencio queda como resincronizador ante ruido o trama truncada, no como delimitador
primario. Es más robusto que depender de `millis()`, cuya resolución de 1 ms frente a un
silencio de 4 ms da un margen de error del 25 %.

---

## 3. Reglas de comportamiento

| # | Regla | Comportamiento observable |
|---|---|---|
| MB-01 | Petición dirigida a otro identificador | **Silencio absoluto.** No se responde ni se altera el estado |
| MB-02 | Verificador de redundancia incorrecto | **Silencio.** Suma al contador de CRC |
| MB-03 | Longitud declarada incoherente | **Silencio.** Suma al contador de formato. Nunca se indexa fuera del búfer |
| MB-04 | Función no soportada | Excepción `0x01` |
| MB-05 | Dirección o rango fuera del mapa | Excepción `0x02` |
| MB-06 | Cantidad cero o por encima del límite Modbus | Excepción `0x03` |
| MB-07 | Cualquier función de escritura | Excepción `0x01`. El controlador es la única fuente de verdad |
| MB-08 | Sin petición pendiente | **Jamás** se transmite nada |
| MB-09 | Trama fragmentada en varias entregas | Se reensambla; el silencio solo descarta lo parcial |
| MB-10 | Búfer de recepción excedido | Se marca desbordamiento y se descarta hasta el próximo silencio |

**Por qué el silencio y no una excepción** en MB-01, MB-02 y MB-03: lo manda el estándar
Modbus RTU. Una trama corrupta puede haber ido dirigida a otro esclavo, y responder a
algo que no se entendió ensucia el bus. Ante la duda, callar.

---

## 4. Control de dirección del transceptor

Es la parte que la implementación anterior resolvía bloqueando el lazo 55 ms.

```
  Recibiendo          Transmitiendo                    Recibiendo
  DE = LOW            DE = HIGH                        DE = LOW
  ─────────────┬──────────────────────────────┬──────────────────
               │                              │
      hay respuesta lista            transmitting() == false
      beginTransmission()            endTransmission()
      write(...)                     (en una vuelta POSTERIOR)
```

| Regla | Enunciado |
|---|---|
| DE-01 | DE se afirma **antes** del primer byte |
| DE-02 | DE se libera **solo** cuando el último bit ha salido del registro de desplazamiento. Liberarlo antes trunca la propia respuesta |
| DE-03 | La espera de DE-02 ocurre **entre vueltas del lazo**, nunca dentro de una llamada |
| DE-04 | Mientras se transmite se ignoran los bytes entrantes: en half-duplex es el eco de uno mismo |
| DE-05 | El estado de reposo del pin es recepción, incluido el arranque y tras un reinicio |

**Cálculo de la duración.** El instante de fin se estima a partir de los bytes y del
baudaje, y se confirma con el vaciado del búfer:

```
duración_ms = ceil(bytes × 10 × 1000 / baudios) + margen
```

A 9600 baudios 8N1 son 10 bits por byte en el cable. Para la respuesta mayor soportada,
53 bytes, salen 55,2 ms. El búfer de transmisión del Mega es de 64 bytes, suficiente,
de modo que `write()` retorna de inmediato.

---

## 5. Presupuesto temporal

| Concepto | Valor | Origen |
|---|---:|---|
| Tiempo por byte a 9600 8N1 | 1,04 ms | `10 / 9600` |
| Silencio entre tramas (t3.5) | 4,01 ms | `3,5 × 11 / 9600` |
| Petición (8 B) | 8,3 ms | |
| Respuesta de 1 registro (7 B) | 7,3 ms | Lo que el UC100 pide por canal |
| Respuesta de 24 bobinas (8 B) | 8,3 ms | |
| Respuesta de 24 registros (53 B) | 55,2 ms | Peor caso, solo con maestro de escritorio |
| **Ocupación del lazo por vuelta** | **< 2 ms** | Principio I. Solo se mueven bytes entre búferes |

La respuesta tarda 55 ms en salir por el cable, pero **el lazo no la espera**: sigue
girando, leyendo interruptores y refrescando el panel mientras el hardware transmite.

Esa distinción entre *tiempo en el cable* y *tiempo de lazo bloqueado* es exactamente la
que la implementación anterior no hacía, y de ahí su presupuesto de 480 a 805 ms por
vuelta frente a los 15 ms nominales.

---

## 6. Casos límite y respuesta esperada

| Caso | Comportamiento exigido |
|---|---|
| Ruido antes de una petición legítima | El ruido se descarta con el silencio; la petición siguiente se atiende con normalidad |
| Dos peticiones sin silencio suficiente entre ellas | Se tratan como trama inválida; ninguna se atiende a medias |
| Campo de longitud que declara más datos de los recibidos | Se rechaza sin leer fuera de memoria. **Defecto verificado**: hoy `byteCount = 251` hace que `(uint8_t)(3+251+2)` valga `0`, la comprobación pase, y se lean 254 bytes de un búfer de 32 |
| Petición durante una transmisión propia | Se ignora: en half-duplex es el eco propio |
| Maestro ausente mucho tiempo | El controlador sigue funcionando, publica la antigüedad en el registro 25 y lo señala en el panel |
| Desbordamiento de `millis()` a los 49,7 días | Ni el ensamblado, ni el silencio, ni la señal de bus mudo se cuelgan: toda comparación usa aritmética modular |
| Reinicio por watchdog a mitad de una respuesta | Al volver, el pin queda en recepción y el banco se siembra de los pines antes de atender el bus |

---

## 7. Vectores de prueba

Verificadores calculados y contrastados contra el vector de referencia del estándar
(`01 04 00 01 00 01` → `60 0A`).

| Caso | Petición | Respuesta esperada |
|---|---|---|
| Válvula 3 pide riego | `01 03 00 03 00 01 74 0A` | `01 03 02 00 01 79 84` |
| 24 bobinas, válvulas 0 y 3 activas | `01 01 00 00 00 18 3C 00` | `01 01 03 09 00 00 EC 4C` |
| Dirección fuera del mapa | `01 03 00 20 00 01 85 C0` | `01 83 02 C0 F1` |
| Dirigida a otro esclavo | `02 03 00 00 00 01 ...` | *silencio* |
| CRC incorrecto | `01 03 00 03 00 01 00 00` | *silencio* |
| Función de escritura | `01 06 00 00 00 01 ...` | `01 86 01 ...` (excepción `0x01`) |
