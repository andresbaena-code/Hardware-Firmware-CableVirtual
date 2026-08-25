# Contrato — Puertos entre la lógica de control y el hardware

**Feature**: `001-migracion-platformio` | **Versión del contrato**: 1.0 | **Fecha**: 2026-08-20

Estos son los únicos puntos por los que la lógica de control toca el mundo exterior
(Principio III). Viven en `lib/wiga_ports` como interfaces de solo cabecera, sin
dependencias. Los adaptadores concretos viven en `lib/wiga_hal` y los dobles de prueba en
`test/mocks`.

**Regla general PT-00**: ningún método de estos puertos puede bloquear más de **2 ms**
(Principio I). Un adaptador que no pueda cumplirlo debe reportar fallo y regresar, nunca
esperar.

---

## 1. `IClock` — fuente de tiempo

```
class IClock {
public:
  virtual ~IClock() = default;
  virtual uint32_t nowMs() const = 0;
};
```

| Regla | Descripción |
|---|---|
| CLK-01 | `nowMs()` es monótona salvo por el desbordamiento natural de 32 bits (~49,7 días) |
| CLK-02 | Los consumidores DEBEN comparar con `(uint32_t)(now - mark) >= interval`, nunca con marcas absolutas (FR-013, invariante R-04) |
| CLK-03 | `nowMs()` no bloquea ni deshabilita interrupciones más allá de lo que exige leer el contador |

**Adaptador**: `ArduinoClock` → `millis()`.
**Doble de prueba**: `FakeClock`, con avance manual, para forzar el desbordamiento en
`test_control/test_time_wraparound`.

---

## 2. `IDigitalInputPort` — entradas del tablero

```
class IDigitalInputPort {
public:
  virtual ~IDigitalInputPort() = default;
  virtual void configure() = 0;
  virtual SignalLevel read(ValveIndex index) const = 0;
};
```

| Regla | Descripción |
|---|---|
| IN-01 | `configure()` se llama una vez en el arranque; establece pull-up interno en todos los pines del mapa |
| IN-02 | `read()` devuelve el **nivel eléctrico** crudo. La conversión a estado físico y el antirebote ocurren en la capa de control, no aquí (FR-021b) |
| IN-03 | `read()` con un índice ≥ `kValveCount` es un error de programación; el adaptador puede asumir el índice válido porque el llamador lo garantiza (invariante V-03) |
| IN-04 | `read()` no bloquea: una lectura digital en AVR cuesta microsegundos |

**Adaptador**: `ArduinoDigitalInput` → `pinMode(pin, INPUT_PULLUP)` y `digitalRead(pin)`,
con el pin tomado del mapa de instalación.
**Doble de prueba**: `FakeInputs`, con niveles fijados por prueba, para simular rebote.

---

## 3. `IRadioPort` — comunicación con los nodos

```
class IRadioPort {
public:
  virtual ~IRadioPort() = default;
  virtual void begin() = 0;
  virtual bool send(const ValveCommand& command) = 0;
  virtual uint8_t poll(ValveReport* out, uint8_t maxReports) = 0;
  virtual uint16_t framingErrors() const = 0;
};
```

| Regla | Descripción |
|---|---|
| RD-01 | `send()` serializa según [application-protocol.md §2](./application-protocol.md) y transmite una única trama. Devuelve `false` si no pudo entregarla al radio |
| RD-02 | `send()` **NO DEBE** vaciar, descartar ni ignorar el búfer de recepción (FR-018, R-003) |
| RD-03 | `send()` no bloquea: la trama completa (≈ 24 bytes con escapes) cabe en el búfer de transmisión de 64 bytes del `HardwareSerial` |
| RD-04 | `poll()` drena el radio y escribe hasta `maxReports` confirmaciones válidas en `out`, devolviendo cuántas escribió. Debe invocar la lectura de trama **repetidamente** hasta agotar el búfer o alcanzar `maxReports` (R-002) |
| RD-05 | `poll()` aplica las reglas `RX-01` a `RX-04` del contrato de protocolo. Las reglas `RX-05` y `RX-06` son responsabilidad de la capa de control, que es quien conoce el mapa y el estado |
| RD-06 | `poll()` nunca devuelve una confirmación cuya trama no haya pasado la validación completa |
| RD-07 | `framingErrors()` acumula tramas descartadas por corrupción o desincronización, para diagnóstico (FR-028) |

**Adaptador**: `XBeeRadioAdapter` → librería `andrewrapp/XBee` sobre `Serial1` a 9 600
baudios, modo API escapado.
**Doble de prueba**: `FakeRadio`, que registra las órdenes transmitidas y permite encolar
confirmaciones arbitrarias, incluidas tramas corruptas y de nodos desconocidos.

---

## 4. `IDisplayPort` — panel de estado

```
class IDisplayPort {
public:
  virtual ~IDisplayPort() = default;
  virtual void begin() = 0;
  virtual void setValveSymbol(ValveIndex index, char symbol) = 0;
  virtual void setStatusLine(const char* text) = 0;
  virtual void tick() = 0;
  virtual bool healthy() const = 0;
};
```

| Regla | Descripción |
|---|---|
| DP-01 | `setValveSymbol()` y `setStatusLine()` **solo marcan** el buffer de sombra; no escriben al hardware (R-007) |
| DP-02 | `tick()` escribe como máximo `kDisplayCellsPerTick` celdas sucias por llamada, y regresa. Es el único método que toca el bus I2C en régimen |
| DP-03 | Un fallo del panel NO DEBE propagarse a la capa de control: `tick()` no lanza, no bloquea y no altera el resultado del lazo (FR-022, invariante B-02) |
| DP-04 | Tras un número configurable de fallos consecutivos, el adaptador deja de intentar escribir y `healthy()` pasa a `false`, para no consumir presupuesto de ciclo con un bus caído |
| DP-05 | El símbolo recibido representa el **estado físico** de la válvula. El puerto no convierte niveles (NV-02) |

**Adaptador**: `LcdDisplayAdapter` → `marcoschwartz/LiquidCrystal_I2C@1.1.4` en `0x27`,
20×4, bus I2C a 400 kHz.
**Doble de prueba**: `FakeDisplay`, que expone el contenido del buffer para aserciones.

---

## 5. `IWatchdogPort` — perro guardián

```
class IWatchdogPort {
public:
  virtual ~IWatchdogPort() = default;
  virtual void disable() = 0;
  virtual void enable() = 0;
  virtual void feed() = 0;
};
```

| Regla | Descripción |
|---|---|
| WD-01 | `disable()` se llama al comienzo de `setup()`, antes de cualquier inicialización lenta |
| WD-02 | `enable()` se llama al final de `setup()`, con plazo de 8 s (FR-012) |
| WD-03 | `feed()` se invoca en **un único punto** del lazo principal. Está prohibido llamarlo dentro de una espera, un bucle interno o un manejador de error (Principio I, FR-012) |
| WD-04 | Ningún adaptador ni módulo distinto del lazo principal tiene acceso a este puerto |

**Adaptador**: `AvrWatchdog` → `wdt_disable()`, `wdt_enable(WDTO_8S)`, `wdt_reset()`.
**Doble de prueba**: `FakeWatchdog`, que cuenta invocaciones para verificar WD-03.

---

## 6. `ILogPort` — consola de diagnóstico

```
enum class LogLevel : uint8_t { Error = 1, Warn = 2, Info = 3, Debug = 4 };

class ILogPort {
public:
  virtual ~ILogPort() = default;
  virtual void begin() = 0;
  virtual void event(LogLevel level, const char* tag, ValveIndex index, int32_t value) = 0;
  virtual void message(LogLevel level, const char* text) = 0;
};
```

| Regla | Descripción |
|---|---|
| LG-01 | Toda traza es no bloqueante: si el búfer de transmisión está lleno, la traza **se pierde**; nunca se espera a que se vacíe (Principio I, FR-029) |
| LG-02 | Una traza NO DEBE alterar el comportamiento del control. Compilar con `-D WIGA_LOG_LEVEL=0` elimina las llamadas sin cambiar ninguna otra ruta de ejecución (FR-029) |
| LG-03 | Las cadenas literales residen en Flash (`F()` / `PROGMEM`), no en SRAM (NFR-001) |
| LG-04 | El formato es de campos fijos y legible por máquina, para poder filtrarlo en campo |

**Eventos mínimos requeridos** (FR-026, FR-027, FR-028):

| Tag | Cuándo | `value` |
|---|---|---|
| `BOOT` | arranque | causa del último reinicio (bits de `MCUSR`) |
| `VER` | arranque | versión de firmware |
| `TX` | orden transmitida | número de intento |
| `ACK` | confirmación válida recibida | estado confirmado |
| `TMO` | vence la espera sin confirmar | intentos consumidos |
| `BACK` | entra en reposo prolongado | fallos acumulados de esa válvula |
| `DROP` | trama descartada | motivo (regla `RX-*` que falló) |
| `LCD` | panel marcado como no sano | fallos consecutivos |

**Adaptador**: `SerialLog` → `Serial` a 9 600 baudios.
**Doble de prueba**: `FakeLog`, que acumula los eventos para aserciones.

---

## 7. Regla de composición

**CP-01**: los adaptadores concretos se instancian **exclusivamente** en `src/main.cpp`,
que es la única raíz de composición del proyecto. Ningún módulo de `lib/wiga_control` o
`lib/wiga_domain` conoce un tipo concreto.

**CP-02**: `src/main.cpp` no contiene lógica de negocio. Su responsabilidad completa es:
crear adaptadores, inyectarlos en `ValveSupervisor`, y ejecutar el lazo
(`feed watchdog → tick supervisor → tick display`).

**CP-03**: `lib/wiga_hal` **no se compila** en el entorno `native`. Una inclusión de
`<Arduino.h>`, `<XBee.h>`, `<LiquidCrystal_I2C.h>` o `<avr/wdt.h>` desde el dominio o el
control rompe la compilación de pruebas de forma inmediata. Esta es la verificación
estructural del Principio III.

---

## 8. Verificación del contrato

Cada regla de este documento se verifica de una de dos formas:

- **Reglas de comportamiento** (`CLK-02`, `RD-02`, `RD-04`, `DP-01`, `DP-02`, `DP-03`,
  `WD-03`, `LG-02`): prueba automatizada en `native` contra los dobles de prueba.
- **Reglas estructurales** (`CP-01`, `CP-03`): verificadas por el propio build. Si se
  violan, `pio test -e native` no compila.
