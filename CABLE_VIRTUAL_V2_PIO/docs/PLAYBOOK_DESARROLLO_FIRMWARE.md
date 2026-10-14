# Playbook de Desarrollo de Firmware

**Versión**: 1.0.0  
**Fecha**: 2026-08-26  
**Estado**: Borrador — pendiente revisión del equipo  
**Flujo**: Spec Kit completo + verificación obligatoria

---

## 1. Visión General

Todo desarrollo de firmware sigue **6 fases** con **puertas de calidad** entre cada una.
Ninguna fase avanza hasta que la anterior pase su puerta.

```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│  1. SPECIFY │───▶│  2. PLAN    │───▶│  3. TASKS   │
│  Definir QUÉ │    │  Definir CÓMO│    │  Descomponer│
└─────────────┘    └─────────────┘    └─────────────┘
     │                   │                   │
   Puerta 1           Puerta 2           Puerta 3
     │                   │                   │
     ▼                   ▼                   ▼
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│ 4. IMPLEMENT│───▶│ 5. VERIFY   │───▶│ 6. DEPLOY   │
│  Escribir   │    │  Probar     │    │  Integrar   │
└─────────────┘    └─────────────┘    └─────────────┘
     │                   │                   │
   Puerta 4           Puerta 5           Puerta 6
```

**Herramientas obligatorias**: VS Code + PlatformIO, Git + GitHub, Spec Kit, CI/CD (GitHub Actions).

---

## 2. Fase 1 — SPECIFY (Definir QUÉ)

### Qué hacer

1. Recibir o generar la especificación de la funcionalidad.
2. Definir historias de usuario con criterios de aceptación.
3. Definir requisitos funcionales (FR-xxx) y no funcionales (NFR-xxx).
4. Identificar casos borde y dependencias.

### Cómo hacerlo

Usar Spec Kit:

```
/speckit-specify
```

Spec Kit genera un `spec.md` con:
- Historias de usuario priorizadas (P1, P2, P3)
- Requisitos funcionales numerados (FR-001, FR-002, ...)
- Requisitos no funcionales (NFR-001, ...)
- Criterios de éxito medibles (SC-001, ...)
- Casos borde
- Supuestos y exclusiones

### Ejemplo real

Ver `specs/001-migracion-platformio/spec.md` — 35 FR, 3 NFR, 8 SC.

### Entregable

- `specs/NNN-nombre/spec.md`

### Puerta de calidad 1

- [ ] Todos los requisitos tienen FR-xxx o NFR-xxx
- [ ] Cada requisito es verificable (tiene un criterio de aceptación)
- [ ] Los casos borde están documentados
- [ ] Los supuestos están explícitos
- [ ] Se ejecutó `/speckit-clarify` y no hay dudas pendientes

### Comandos

```bash
/speckit-specify          # crear especificación
/speckit-clarify          # resolver dudas pendientes
```

---

## 3. Fase 2 — PLAN (Definir CÓMO)

### Qué hacer

1. Investigar decisiones técnicas (dependencias, protocolos, restricciones).
2. Definir la arquitectura del proyecto.
3. Diseñar el modelo de datos y las interfaces.
4. Crear contratos de puertos.
5. Validar la constitución del proyecto.

### Cómo hacerlo

Spec Kit genera los documentos de planificación:

```
/speckit-plan
```

Esto crea:
- `plan.md` — arquitectura, estructura del proyecto, fases de trabajo
- `research.md` — decisiones técnicas con evidencia
- `data-model.md` — entidades, estados, transiciones
- `contracts/` — contratos de interfaces y puertos

### Ejemplo real

Ver `specs/001-migracion-platformio/` — 14 decisiones técnicas, modelo de datos completo.

### Entregable

- `specs/NNN-nombre/plan.md`
- `specs/NNN-nombre/research.md`
- `specs/NNN-nombre/data-model.md`
- `specs/NNN-nombre/contracts/`

### Puerta de calidad 2

- [ ] La arquitectura respeta los 7 principios (ver `ESTANDAR_DESARROLLO_FIRMWARE.md`)
- [ ] La barrera de capas es estructural (dominio y control sin `<Arduino.h>`)
- [ ] No hay memoria dinámica en la lógica de control
- [ ] La configuración está separada de la lógica
- [ ] Las decisiones técnicas tienen evidencia documentada
- [ ] El presupuesto de memoria es factible (< 75% RAM)

### Comandos

```bash
/speckit-plan             # generar plan de implementación
```

---

## 4. Fase 3 — TASKS (Descomponer en tareas)

### Qué hacer

1. Descomponer el plan en tareas ordenadas por dependencia.
2. Identificar qué tareas son paralelizables.
3. Definir el MVP (entrega mínima con valor).
4. Asignar requisitos a cada tarea.

### Cómo hacerlo

Spec Kit genera las tareas:

```
/speckit-tasks
```

Esto crea `tasks.md` con:
- Tareas numeradas (T001, T002, ...)
- Dependencias entre tareas
- Marca `[P]` para tareas paralelizables
- Historia de usuario a la que pertenece
- Requisitos que cubre cada tarea

### Ejemplo real

Ver `specs/001-migracion-platformio/tasks.md` — 73 tareas, 9 fases.

### Entregable

- `specs/NNN-nombre/tasks.md`

### Puerta de calidad 3

- [ ] Cada tarea tiene un ID único
- [ ] Las dependencias están explícitas
- [ ] El MVP está definido (fases esenciales)
- [ ] Cada requisito aparece en al menos una tarea
- [ ] Las tareas paralelizables están marcadas con `[P]`

### Comandos

```bash
/speckit-tasks            # generar lista de tareas
```

---

## 5. Fase 4 — IMPLEMENT (Escribir código)

### Qué hacer

1. Crear la rama de desarrollo.
2. Seguir el orden de las tareas (respetando dependencias).
3. Escribir código **junto con sus pruebas** (Principio IV).
4. Respetar la estructura del proyecto y las convenciones.
5. Hacer commits pequeños y descriptivos.

### Orden de implementación

El orden correcto para un firmware nuevo es:

```
1. Configuración (tipos, constantes, mapa)
2. Interfaces (puertos abstractos)
3. Dominio (protocolo, entidades)
4. Control (lógica de estados, algoritmos)
5. Mocks (dobles de prueba)
6. Pruebas unitarias
7. HAL (adaptadores concretos)
8. main.cpp (cableado)
9. Pruebas de integración
```

**Por qué este orden**: cada capa depende de la anterior. Si escribís el HAL primero, no
tenés interfaces para implementar. Si escribís main.cpp primero, no tenés nada que
conectar.

### Convenciones de código

| Elemento | Formato | Ejemplo |
|----------|---------|---------|
| Tipos | `PascalCase` | `ValveState` |
| Funciones | `camelCase` | `onRequestChanged()` |
| Variables miembro | `camelCase_` | `lastServed_` |
| Constantes | `kPascalCase` | `kValveCount` |
| Archivos | `snake_case` | `modbus_slave.h` |
| Comentarios | Explican el porqué, en español | |

### Reglas durante la implementación

1. **No incluir `<Arduino.h>`** en `lib/dominio/`, `lib/control/`, o `lib/puertos/`.
2. **No usar `delay()`** en la lógica de control.
3. **No usar memoria dinámica** (`new`, `malloc`, `String`).
4. **Toda constante** va en archivos de configuración, no hardcodeada.
5. **Todo `FR-xxx`** tiene su comentario en el código y su prueba.

### Cómo escribir una prueba

Patrón AAA (Arrange → Act → Assert):

```cpp
static void test_ejemplo(void) {
    // Arrange — preparar el escenario
    FakeClock clock;
    FakeInputs inputs;
    RegisterFile registers;
    ValvePublisher publisher(clock, inputs, registers, display, log);
    publisher.begin();
    inputs.setLevel(0, SignalLevel::High);

    // Act — ejecutar la acción
    clock.advance(50);   // pasar la ventana de antirrebote
    publisher.tick();

    // Assert — verificar el resultado
    TEST_ASSERT_EQUAL_UINT16(1, registers.get(valveRegister(0)));
}
```

### Commits

Formato: `tipo(alcance): FR-xxx — descripción corta`

```
feat(control): FR-006/FR-007 — reposo prolongado
test(control): FR-002 — antirebote de 50ms
fix(hal): FR-018 — eliminar vaciado de búfer de radio
docs(config): FR-023 — mapa de instalación
```

### Entregable

- Código en `src/`, `lib/`, `test/`
- Commits con trazabilidad a requisitos

### Puerta de calidad 4

- [ ] `pio test -e native` pasa sin errores
- [ ] `pio run -e megaatmega2560` compila sin errores
- [ ] No hay `delay()` en código de control
- [ ] No hay `<Arduino.h>` fuera de `lib/hal/`
- [ ] No hay memoria dinámica
- [ ] Cada función de control tiene su `FR-xxx`
- [ ] Cada prueba nueva tiene nombre descriptivo en español

### Comandos

```bash
git checkout -b NNN-nombre-corto    # crear rama
pio run -e megaatmega2560           # compilar para producción
pio test -e native                  # ejecutar pruebas
```

---

## 6. Fase 5 — VERIFY (Probar y verificar)

### Qué hacer

1. Ejecutar la suite completa de pruebas.
2. Verificar el presupuesto de memoria.
3. Ejecutar los escenarios de validación (quickstart).
4. Verificar la barrera de capas.
5. Documentar resultados.

### Suite de pruebas

```bash
pio test -e native                  # todas las pruebas en host
pio test -e native -f test_control  # solo pruebas de control
pio test -e native -f test_domain   # solo pruebas de dominio
pio test -e native -f test_integration  # solo pruebas de integración
```

### Verificación de memoria

```bash
pio run -e megaatmega2560           # compilar y ver reporte
```

Resultado esperado:

```
RAM:   [=====     ]  45.2% (used 3699 bytes from 8192 bytes)
Flash: [=======   ]  68.3% (used 174892 bytes from 262144 bytes)
```

**Regla**: RAM < 75%, Flash < 90%.

### Verificación de barrera de capas

```bash
# Agregar temporalmente #include <Arduino.h> en lib/control/algo.cpp
pio test -e native                  # DEBE fallar
# Revertir el cambio
pio test -e native                  # DEBE pasar
```

### Escenarios de validación (quickstart)

Cada proyecto define sus escenarios en `specs/NNN-nombre/quickstart.md`.

| Escenario | Sin hardware | Requisito |
|-----------|--------------|-----------|
| Compilación reproducible | Sí | FR-030 |
| Pruebas en host | Sí | FR-031 |
| Barrera de capas | Sí | Principio III |
| Configuración inválida no compila | Sí | FR-025 |
| Configuración centralizada | Sí | SC-004 |
| Arranque y diagnóstico | Banco | FR-026 |
| Funcionalidad principal | Banco | SC-001 |
| Protección de batería | Banco | SC-005 |

### Entregable

- Resultados de pruebas documentados
- Uso de RAM y Flash registrado
- Escenarios de validación ejecutados

### Puerta de calidad 5

- [ ] 100% de pruebas pasan
- [ ] RAM < 75%
- [ ] Flash < 90%
- [ ] Barrera de capas verificada
- [ ] Escenarios de quickstart ejecutados
- [ ] Resultados documentados en el PR

### Comandos

```bash
pio test -e native -v              # verbose — ver cada prueba
pio test -e native -vvv            # ultra verbose — ver asserts
pio run -e megaatmega2560          # compilar y ver memoria
```

---

## 7. Fase 6 — DEPLOY (Integrar y desplegar)

### Qué hacer

1. Abrir Pull Request.
2. Verificar que CI pasa.
3. Hacer code review.
4. Merge a `main`.
5. Cargar en placa y probar en banco.
6. Desplegar en campo.

### Pull Request

1. Actualizar la rama con `main`:

```bash
git fetch origin
git rebase origin/main
```

2. Empujar la rama:

```bash
git push origin NNN-nombre-corto
```

3. Abrir PR en GitHub con:
   - Título descriptivo con `FR-xxx`
   - Resumen de cambios
   - Uso de RAM y Flash
   - Checklist de la puerta 4

### Code Review

El reviewer verifica:
- [ ] Las pruebas pasan en CI
- [ ] No hay `<Arduino.h>` fuera de `hal/`
- [ ] No hay `delay()` en control
- [ ] No hay memoria dinámica
- [ ] Los `FR-xxx` están documentados
- [ ] Las pruebas cubren los requisitos

### CI/CD (GitHub Actions)

El pipeline verifica automáticamente:
1. Pruebas en host (`pio test -e native`)
2. Compilación para producción (`pio run -e megaatmega2560`)
3. Verificación de RAM < 75%
4. Barrera de capas estructural

### Carga en placa

```bash
pio run -e megaatmega2560 -t upload    # compilar y cargar
pio device monitor -b 9600              # abrir consola
```

### Prueba en banco

Seguir los escenarios de `quickstart.md` que requieren hardware.

### Entregable

- PR aprobado y mergeado
- Firmware cargado en placa
- Pruebas de banco documentadas (acta)

### Puerta de calidad 6

- [ ] CI pasa sin errores
- [ ] Code review aprobado
- [ ] Firmware cargado y arranca
- [ ] Escenarios de banco ejecutados
- [ ] Acta de pruebas firmada

---

## 8. Comandos Rápidos

### Desarrollo diario

```bash
# Compilar para producción
pio run -e megaatmega2560

# Ejecutar todas las pruebas
pio test -e native

# Ejecutar una suite específica
pio test -e native -f test_control

# Cargar en placa
pio run -e megaatmega2560 -t upload

# Abrir consola serie
pio device monitor -b 9600

# Ver uso de memoria
pio run -e megaatmega2560  # el reporte aparece al final
```

### Git

```bash
# Crear rama
git checkout -b 001-nombre-corto

# Hacer commit
git add .
git commit -m "feat(control): FR-006 — reposo prolongado"

# Empujar
git push origin 001-nombre-corto

# Ver estado
git status
git log --oneline -10
```

### Spec Kit

```bash
/speckit-specify    # crear especificación
/speckit-clarify    # resolver dudas
/speckit-plan       # generar plan
/speckit-tasks      # generar tareas
/speckit-implement  # implementar tareas
```

---

## 9. Checklist Maestro

### Antes de empezar un proyecto nuevo

- [ ] Especificación creada con `/speckit-specify`
- [ ] Dudas resueltas con `/speckit-clarify`
- [ ] Plan generado con `/speckit-plan`
- [ ] Tareas generadas con `/speckit-tasks`
- [ ] Constitución del proyecto definida (7 principios)

### Durante la implementación

- [ ] Respetar el orden: config → interfaces → dominio → control → mocks → tests → hal → main
- [ ] Cada función tiene su `FR-xxx`
- [ ] Cada prueba tiene nombre descriptivo en español
- [ ] No hay `delay()` en control
- [ ] No hay `<Arduino.h>` fuera de `hal/`
- [ ] No hay memoria dinámica
- [ ] Commits con formato `tipo(alcance): FR-xxx — descripción`

### Antes de cada commit

- [ ] `pio test -e native` pasa
- [ ] `pio run -e megaatmega2560` compila

### Antes de cada PR

- [ ] Todas las pruebas pasan
- [ ] RAM < 75%
- [ ] Flash < 90%
- [ ] Escenarios de quickstart ejecutados
- [ ] Resultados documentados

### Antes de desplegar en campo

- [ ] PR aprobado y mergeado
- [ ] CI pasa
- [ ] Firmware cargado en placa
- [ ] Pruebas de banco ejecutadas
- [ ] Acta de pruebas firmada

---

## 10. Referencias

| Documento | Ubicación |
|-----------|-----------|
| Estándar de código | `ESTANDAR_DESARROLLO_FIRMWARE.md` |
| Especificación (Cable Virtual) | `specs/001-migracion-platformio/spec.md` |
| Plan (Cable Virtual) | `specs/001-migracion-platformio/plan.md` |
| Tareas (Cable Virtual) | `specs/001-migracion-platformio/tasks.md` |
| Quickstart (Cable Virtual) | `specs/001-migracion-platformio/quickstart.md` |
| Convenciones del proyecto | `AGENTS.md` |

---

*Playbook creado el 2026-08-26. Pendiente de revisión por el equipo de ingeniería.*
