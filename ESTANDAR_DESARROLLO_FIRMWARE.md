# Estándar de Desarrollo de Firmware Embebido

**Versión**: 1.0.0  
**Fecha**: 2026-08-26  
**Estado**: Borrador — pendiente revisión del equipo  
**Alcance**: Todos los proyectos de firmware de la empresa

---

## 1. Propósito

Este documento define las reglas arquitectónicas y de código que todo firmware de la empresa
DEBE cumplir. No es una guía de estilo superficial: son patrones probados en la industria
de firmware embebido que evitan los modos de fallo más costosos en campo.

**Para quién es**: cualquier ingeniero electrónico o desarrollador de firmware que
contribuya a un proyecto de la empresa.

**Cómo usarlo**: leer los principios una vez, consultar las reglas al desarrollar, y usar
el checklist antes de cada PR.

---

## 2. Principios Fundamentales

Cada principio resuelve un problema real. Entender el "por qué" es más importante que
memorizar la regla.

### 2.1 Separación en Capas

**Problema**: si la lógica de control vive junto al código de hardware, no se puede
probar sin la placa conectada. Cada verificación en campo cuesta un desplazamiento.

**Solución**: separar el código en capas con responsabilidades claras.

```
┌─────────────────────────────────────────────────┐
│  main.cpp                                       │
│  Cableado: crea adaptadores, inyecta puertos,   │
│  ejecuta el lazo. ÚNICO archivo con hardware.   │
├─────────────────────────────────────────────────┤
│  lib/wiga_control/                              │
│  Lógica de estados, reintentos, planificación.  │
│  Sin hardware. Se prueba con mocks en la PC.    │
├─────────────────────────────────────────────────┤
│  lib/wiga_domain/                               │
│  Tipos, protocolo, mapa de configuración.       │
│  Sin hardware. constexpr y static_assert.       │
├─────────────────────────────────────────────────┤
│  lib/wiga_hal/                                  │
│  Adaptadores concretos: radio, LCD, entradas,   │
│  watchdog. ÚNICA capa que toca hardware.        │
└─────────────────────────────────────────────────┘
```

**Referencia**: Modelo de capas de David Parnas (1972). En firmware embebido, la capa
`hal` es la única que incluye `<Arduino.h>`, `<XBee.h>`, o librerías de hardware.

### 2.2 Inyección de Dependencias (Puertos e Interfaces)

**Problema**: si el supervisor de válvulas crea internamente un objeto `XBeeRadioAdapter`,
no se puede reemplazar por un doble de prueba. La prueba exige hardware real.

**Solución**: el supervisor recibe una **interfaz** abstracta, no un tipo concreto. El
archivo `main.cpp` crea el adaptador concreto y lo "enchufa" al supervisor.

```cpp
// interfaz (lib/wiga_ports/i_radio_port.h)
class IRadioPort {
public:
    virtual bool send(const ValveCommand& cmd) = 0;
    virtual uint8_t poll(ValveReport* out, uint8_t max) = 0;
};

// adaptador concreto (lib/wiga_hal/xbee_radio_adapter.h)
class XBeeRadioAdapter : public IRadioPort {
    bool send(const ValveCommand& cmd) override;  // habla con el XBee real
    uint8_t poll(ValveReport* out, uint8_t max) override;
};

// doble de prueba (test/mocks/fake_radio.h)
class FakeRadio : public IRadioPort {
    bool send(const ValveCommand& cmd) override;  // graba comandos para asserts
    uint8_t poll(ValveReport* out, uint8_t max) override;
};

// main.cpp — el único lugar donde se elige cuál usar
XBeeRadioAdapter g_radio;
ValveSupervisor g_supervisor(g_radio);  // recibe interfaz, no tipo concreto
```

**Referencia**: Dependency Inversion Principle (SOLID), Robert C. Martin (1996).
"Clean Architecture", Cap. 22. En firmware se llama "Ports and Adapters".

### 2.3 Temporización No Bloqueante (Super Loop)

**Problema**: `delay(15)` bloquea el lazo completo durante 15 ms. Si durante ese tiempo
llega una confirmación por radio, se pierde. Si el LCD tarda 200 ms en escribirse, el
sistema queda mudo 200 ms.

**Solución**: el lazo gira libremente. Un temporizador decide cuándo ejecutar la lógica.

```cpp
// MAL — bloquea el lazo
void loop() {
    g_supervisor.tick();
    delay(15);
}

// BIEN — no bloquea, permite atender radio y LCD entre ticks
uint32_t g_lastTickMs = 0;

void loop() {
    g_watchdog.feed();
    const uint32_t now = millis();
    if (elapsed(now, g_lastTickMs, kLoopPeriodMs)) {
        g_lastTickMs = now;
        g_supervisor.tick();
    }
    g_display.tick();  // escribe una celda del LCD por vuelta
}
```

**Comparación de tiempo**: `elapsed()` usa aritmética modular `(uint32_t)(now - since) >= interval`,
que es correcta incluso cuando `millis()` desborda (~49,7 días). La forma `now >= deadline`
falla una vez por desbordamiento.

**Referencia**: "Making Embedded Systems" de Elecia White (O'Reilly), Cap. 5.
Estándar en firmware embebido desde los años 90.

### 2.4 Pruebas sin Hardware (Mocks)

**Problema**: verificar que la lógica de reintentos funciona correctamente exige 35
minutos de espera real (5 reintentos × 30 s + margen). En campo, eso es un desplazamiento.

**Solución**: dobles de prueba (mocks) que simulan el hardware. La lógica se ejecuta en
la PC del desarrollador en segundos.

```cpp
// FakeRadio simula el XBee
FakeRadio radio;
radio.enqueue(nodeAddress, valveId, SignalLevel::Low);  // simula confirmación

// FakeClock avanza el tiempo sin esperar real
FakeClock clock;
clock.advance(30000);  // saltar 30 segundos en 0 ms

// FakeInputs simula el tablero
FakeInputs inputs;
inputs.requestOpen(0);  // simular que el operador abrió la válvula 0
```

**Framework**: Unity (incluido en PlatformIO, sin dependencia externa).

**Cómo ejecutar**: `pio test -e native` — corre todas las pruebas en la PC sin hardware.

**Referencia**: "Test Doubles" de Gerard Meszaros (xUnit Test Patterns, 2007).

### 2.5 Trazabilidad de Requisitos

**Problema**: si una funcionalidad falla en campo, no se puede determinar rápidamente qué
requisito se violó ni qué código lo implementa.

**Solución**: cada función, cada prueba y cada módulo lleva el código del requisito que
cubre.

```cpp
// en el código
void onTimeout(ValveRecord& record, uint32_t nowMs) {
    // FR-006, FR-007: reintentos acotados con reposo prolongado
    ...
}

// en la prueba
static void test_maximo_cinco_disparos_en_la_primera_media_hora(void) {
    // SC-005 / NFR-003: protección de batería
    ...
}
```

**Formato**: `FR-XXX` para requisitos funcionales, `NFR-XXX` para no funcionales,
`SC-XXX` para criterios de éxito.

**Referencia**: IEC 62304 (software médico), DO-178C (aviónica). Estándar en industria
regulada.

---

## 3. Reglas Obligatorias

Estas reglas DEBEN cumplirse en todo proyecto. La violación de una regla DEBE corregirse
antes de merge.

### 3.1 Estructura del Proyecto

```
proyecto/
├── src/                    # solo main.cpp — cableado y lazo
├── lib/
│   ├── dominio/            # tipos, protocolo, entidades (sin hardware)
│   ├── control/            # lógica de estados, algoritmos (sin hardware)
│   ├── puertos/            # interfaces abstractas (solo cabeceras .h)
│   ├── configuracion/      # parámetros ajustables por instalación
│   └── hal/                # adaptadores concretos (la ÚNICA capa con hardware)
├── test/
│   ├── mocks/              # dobles de prueba
│   ├── test_dominio/       # pruebas de tipos y protocolo
│   ├── test_control/       # pruebas de lógica
│   └── test_integracion/   # pruebas del supervisor completo
└── docs/                   # documentación del proyecto
```

**REGLA**: `main.cpp` es el ÚNICO archivo que incluye `<Arduino.h>` y crea objetos de
hardware. Las librerías en `lib/` NO pueden incluir librerías de hardware.

### 3.2 Nomenclatura y Convenciones

| Elemento | Formato | Ejemplo |
|----------|---------|---------|
| Tipos (clases, enums, structs) | `PascalCase` | `ValveState`, `RetryPolicy` |
| Funciones | `camelCase` | `onRequestChanged()`, `readPacket()` |
| Variables miembro | `camelCase_` con guion bajo al final | `lastServed_`, `stable_` |
| Constantes | `kPascalCase` | `kValveCount`, `kLoopPeriodMs` |
| Macros | `PROYECTO_MAYUSCULAS` | `WIGA_LOG_LEVEL` |
| Archivos | `snake_case` | `retry_policy.h`, `valve_supervisor.cpp` |
| Indentación | 4 espacios | |
| Llaves | K&R (llave de apertura en la misma línea) | |
| Comentarios | Explican el porqué, no el qué. En español | |

### 3.3 Límites de Capa

**REGLA**: la barrera de capas es **estructural**, no una convención.

| Capa | Puede incluir | No puede incluir |
|------|---------------|-------------------|
| `dominio/` | `<stdint.h>`, otros módulos de dominio | `<Arduino.h>`, librerías de hardware |
| `control/` | dominio, puertos, configuración | `<Arduino.h>`, librerías de hardware |
| `puertos/` | `<stdint.h>`, tipos de dominio | Cualquier librería externa |
| `configuracion/` | dominio | `<Arduino.h>`, control |
| `hal/` | Todo (es la capa más alta) | Nada que restrinja |
| `main.cpp` | Todo | Nada que restrinja |

**Cómo se verifica**: compilar con `pio test -e native`. Si alguna librería de `dominio/`
o `control/` incluye `<Arduino.h>`, la compilación nativa falla automáticamente.

### 3.4 Presupuesto de Memoria

**REGLA**: reportar uso de RAM y Flash en cada PR que toque estructuras de datos.

| Recurso | Límite | Cómo verificar |
|---------|--------|----------------|
| RAM (SRAM) | < 75% de la disponible | PlatformIO reporta al compilar |
| Flash | < 90% de la disponible | PlatformIO reporta al compilar |
| Sin `new`/`malloc`/`String` | 0 allocaciones dinámicas | Revisión de código |
| Cadenas literales | Siempre con `F()` en AVR | Búsqueda de strings sin `F()` |

**Por qué**: un firmware sin memoria dinámica no tiene fragmentation ni leaks. En un
sistema que opera 90 días sin reinicio, eso es crítico.

### 3.5 Configuración vs Lógica

**REGLA**: los parámetros que cambian entre instalaciones van en un archivo separado de
la lógica de control.

```
configuracion/
├── mapa_instalacion.h    # direcciones de radio, IDs de válvula, pines
└── tiempos.h             # intervalos de reintento, ventanas,umbrales
```

**REGLA**: la configuración DEBE validarse en tiempo de compilación con `static_assert`.

```cpp
//地图 instalación: detectar errores ANTES de llegar a campo
constexpr ValveMapping kMap[] = { ... };
constexpr uint8_t kCount = sizeof(kMap) / sizeof(kMap[0]);

static_assert(allPinsUnique(kMap, kCount), "pines duplicados en el mapa");
static_assert(allAddressesAssigned(kMap, kCount), "dirección sin asignar");
```

### 3.6 Tolerancia a Fallos

**REGLA**: ningún componente externo (LCD, radio, sensores) puede detener el control
principal.

```cpp
// El LCD puede fallar → el control sigue operando
void ValveSupervisor::refreshDisplay() {
    display_.setValveSymbol(i, s);  // si falla, no pasa nada
}

// La radio puede rechazar la trama → se reintenta sin gastar intento
if (!radio_.send(command)) {
    log_.event(LogLevel::Error, logtag::kDrop, index, -1);
    return;  // se reintentará en la próxima vuelta
}
```

**REGLA**: si un hardware bloquea indefinidamente (bus I2C sin timeout), el watchdog
debe reiniciar el sistema. Pero el diseño correcto es que el hardware nunca bloquee.

---

## 4. Cómo Aplicar Cada Patrón

### 4.1 Crear un Adaptador de Sensor

**Caso**: necesitás leer un sensor de temperatura por I2C y que la lógica de control
actúe según el valor.

**Paso 1 — Crear la interfaz** (en `lib/puertos/`):

```cpp
// i_temperature_port.h
#pragma once
#include <stdint.h>

class ITemperaturePort {
public:
    virtual ~ITemperaturePort() {}
    virtual void begin() = 0;
    virtual int16_t readTenths() const = 0;  // temperatura en décimas de °C
    virtual bool healthy() const = 0;
};
```

**Paso 2 — Crear el adaptador concreto** (en `lib/hal/`):

```cpp
// i2c_temperature_adapter.h
#pragma once
#include "i_temperature_port.h"
#include <Wire.h>

class I2cTemperatureAdapter : public ITemperaturePort {
public:
    void begin() override { Wire.begin(); }
    int16_t readTenths() const override {
        Wire.requestFrom(0x48, 2);
        if (Wire.available() < 2) return -9999;
        int16_t raw = (Wire.read() << 8) | Wire.read();
        return raw >> 4;  // ejemplo para TMP102
    }
    bool healthy() const override { return healthy_; }
private:
    bool healthy_ = true;
};
```

**Paso 3 — Crear el doble de prueba** (en `test/mocks/`):

```cpp
// fake_temperature.h
#pragma once
#include "i_temperature_port.h"

class FakeTemperature : public ITemperaturePort {
public:
    void begin() override {}
    int16_t readTenths() const override { return value_; }
    bool healthy() const override { return healthy_; }
    void setValue(int16_t v) { value_ = v; }
    void setHealthy(bool h) { healthy_ = h; }
private:
    int16_t value_ = 250;  // 25.0°C por defecto
    bool healthy_ = true;
};
```

**Paso 4 — Inyectar en main.cpp**:

```cpp
I2cTemperatureAdapter g_temp;
MiSupervisor g_supervisor(g_temp, ...);
```

**Paso 5 — Escribir la prueba**:

```cpp
static void test_temperatura_alta_activa_alarma(void) {
    FakeTemperature temp;
    MiSupervisor supervisor(temp, ...);
    supervisor.begin();

    temp.setValue(500);  // 50.0°C
    supervisor.tick();

    TEST_ASSERT_TRUE(supervisor.alarmaActiva());
}
```

### 4.2 Agregar un Nuevo Puerto

Si ninguna interfaz existente se ajusta, crear una nueva en `lib/puertos/`:

```cpp
// i_actuator_port.h
#pragma once
#include <stdint.h>

enum class ActuatorCommand : uint8_t { Off = 0, On = 1 };

class IActuatorPort {
public:
    virtual ~IActuatorPort() {}
    virtual void begin() = 0;
    virtual bool send(uint8_t channel, ActuatorCommand cmd) = 0;
    virtual bool healthy() const = 0;
};
```

**Regla**: la interfaz solo declara el contrato. No implementa nada.

### 4.3 Escribir una Prueba con Mock

**Patrón**: Arrange → Act → Assert (AAA).

```cpp
static void test_ejemplo(void) {
    // Arrange — preparar el escenario
    FakeRadio radio;
    FakeClock clock;
    FakeInputs inputs;
    ValveSupervisor supervisor(clock, inputs, radio, ...);
    supervisor.begin();
    radio.clearSent();

    // Act — ejecutar la acción
    inputs.requestOpen(0);
    inputs.settle();  // simular antirebote
    supervisor.tick();

    // Assert — verificar el resultado
    TEST_ASSERT_EQUAL_UINT16(1, radio.sentCount());
    TEST_ASSERT_EQUAL_UINT8(0, radio.lastSent().valveId);
}
```

**Convención de nombres**: `test_descripcion_en_espanol(void)` — descriptivo, sin
abreviaturas.

### 4.4 Agregar un Requisito con Trazabilidad

1. Agregar `FR-XXX` en el comentario de la función que lo implementa.
2. Nombrar la prueba con el mismo `FR-XXX`.
3. Documentar en `specs/` o `docs/` qué hace el requisito.

```cpp
// FR-035: el sistema DEBE apagar la alarma tras 10 segundos sin temperatura alta
void AlarmManager::tick(uint32_t nowMs) {
    if (active_ && elapsed(nowMs, activatedAtMs_, 10000)) {
        active_ = false;
    }
}
```

---

## 5. Errores Comunes (qué NO hacer)

### 5.1 `delay()` en el lazo de control

```cpp
// MAL — bloquea todo durante N milisegundos
void loop() {
    g_supervisor.tick();
    delay(15);
}

// BIEN — el lazo gira libremente
void loop() {
    const uint32_t now = millis();
    if (elapsed(now, g_lastTickMs, 15)) {
        g_lastTickMs = now;
        g_supervisor.tick();
    }
}
```

### 5.2 Incluir `<Arduino.h>` fuera de `hal/`

```cpp
// MAL — en lib/control/algo.cpp
#include <Arduino.h>    // ERROR: rompe las pruebas en host

// BIEN — en lib/hal/xbee_adapter.cpp
#include <Arduino.h>    // OK: solo la capa de hardware lo incluye
```

### 5.3 Hardcodear configuración en la lógica

```cpp
// MAL — dirección hardcodeada en la lógica de control
void sendCommand() {
    XBeeAddress64 addr(0x0013A200, 0x41C3F54F);  // ERROR
}

// BIEN — la configuración viene del mapa
void sendCommand() {
    const auto& map = config::valveMapping(index);
    XBeeAddress64 addr(config::kXBeeAddressMsb, map.nodeAddressLsb);
}
```

### 5.4 Memoria dinámica

```cpp
// MAL — asignación dinámica
String mensaje = "Valvula " + String(index);  // ERROR: usa heap
uint8_t* buffer = new uint8_t[100];           // ERROR: memory leak

// BIEN — buffers estáticos dimensionados en compilación
char mensaje[32];
snprintf(mensaje, sizeof(mensaje), "Valvula %d", index);
uint8_t buffer[100];
```

### 5.5 Comparar tiempos con `>=` ingenuo

```cpp
// MAL — falla cuando millis() desborda (~49,7 días)
if (now >= deadline) { ... }

// BIEN — aritmética modular, correcto siempre
if ((uint32_t)(now - start) >= interval) { ... }
```

---

## 6. Checklist de Revisión

Antes de cada PR, verificar:

- [ ] `pio test -e native` pasa sin errores
- [ ] `pio run -e megaatmega2560` compila sin errores
- [ ] Uso de RAM < 75% reportado en el PR
- [ ] No hay `delay()` en código de control
- [ ] No hay `#include <Arduino.h>` en `lib/dominio/`, `lib/control/`, o `lib/puertos/`
- [ ] No hay `new`, `malloc`, `String` (Arduino), ni STL con heap
- [ ] Toda constante de configuración está en archivos de configuración, no hardcodeada
- [ ] Cada función de control tiene su `FR-XXX` en el comentario
- [ ] Cada prueba nueva tiene nombre descriptivo en español
- [ ] Cadenas literales usan `F()` en código AVR
- [ ] Los adaptadores nuevos tienen su doble de prueba en `test/mocks/`
- [ ] La configuración tiene `static_assert` que valida integridad

---

## 7. Referencias

### Libros

| Libro | Autor | Capítulos relevantes |
|-------|-------|---------------------|
| "Clean Architecture" | Robert C. Martin | Cap. 22: Dependency Inversion |
| "Making Embedded Systems" | Elecia White | Cap. 5: Super Loop, Cap. 9: Testing |
| "Test Doubles" (xUnit Test Patterns) | Gerard Meszaros | Cap. 14-18: Mocks y Stubs |
| "Programming Embedded Systems" | Michael Barr, Anthony Massa | Cap. 4: Headers, Cap. 7: Software Architecture |

### Estándares

| Estándar | Qué cubre |
|----------|-----------|
| IEC 62304 | Ciclo de vida del software médico — trazabilidad |
| DO-178C | Software de aviación — verificación y validación |
| MISRA C:2012 | Subconjunto seguro de C para sistemas embebidos |
| CERT C | Prácticas seguras de programación en C |

### En el Repositorio

| Documento | Qué explica |
|-----------|-------------|
| `AGENTS.md` | Convenciones, comandos, qué NO hacer |
| `specs/001-migracion-platformio/spec.md` | 35 requisitos funcionales de Cable Virtual |
| `specs/001-migracion-platformio/research.md` | 14 decisiones técnicas con evidencia |
| `specs/001-migracion-platformio/data-model.md` | Modelo de datos y transiciones |
| `specs/001-migracion-platformio/contracts/ports.md` | Contratos de cada puerto |
| `docs/puesta-en-marcha.md` | Guía de compilación y carga |

---

*Documento creado el 2026-08-26. Pendiente de revisión por el equipo de ingeniería.*
