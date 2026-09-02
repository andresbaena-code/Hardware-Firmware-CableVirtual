# Plantilla para Nuevas Especificaciones

Guía paso a paso para especificar una feature nueva, siguiendo el patrón
establecido en `specs/001-migracion-platformio/` y `specs/002-modbus-esclavo-rs485/`.

---

## Estructura de carpetas

```
specs/
├── 001-migracion-platformio/    ← spec completada
├── 002-modbus-esclavo-rs485/    ← spec completada
├── 003-nombre-de-feature/       ← TU NUEVA SPEC
│   ├── spec.md                  ← QUÉ se va a hacer (obligatorio)
│   ├── tasks.md                 ← CÓMO se divide en tareas (obligatorio)
│   ├── plan.md                  ← Estrategia de implementación
│   ├── data-model.md            ← Tipos, estructuras, diagramas
│   ├── research.md              ← Investigación técnica, opciones evaluadas
│   ├── quickstart.md            ← Cómo probar que funciona
│   ├── contracts/               ← Contratos entre capas
│   │   ├── ports.md             ← Interfaces abstractas
│   │   └── ...
│   └── checklists/              ← Checklists de verificación
│       └── requirements.md
```

**Regla:** el número del directorio es secuencial (003, 004, 005...).

---

## Paso 1: spec.md (la especificación)

Es el documento más importante. Define QUÉ se va a hacer y POR QUÉ.

### Estructura mínima

```markdown
# Feature Specification: [Nombre descriptivo]

**Feature Branch**: `003-nombre-de-feature`
**Created**: YYYY-MM-DD
**Status**: Draft | In Progress | Done

---

## Contexto: por qué existe esta especificación

[2-3 párrafos: qué problema resuelve, qué está roto hoy, por qué es necesario]

## Topología / Diagrama

[Diagrama ASCII del sistema antes/después]

## User Stories

### User Story 1 - [Título] (Priority: P1)

[Descripción de la historia desde el punto de vista del usuario]

**Why this priority**: [Por qué esta es la más importante]

**Independent Test**: [Cómo se verifica sola, sin hardware]

**Acceptance Scenarios**:

1. **Given** [estado inicial], **When** [acción], **Then** [resultado esperado]
2. **Given** ..., **When** ..., **Then** ...

### User Story 2 - [Título] (Priority: P2)
[...]

### Edge Cases

- [Caso límite 1]
- [Caso límite 2]

## Requirements

> Numeración desde FR-201 (o el siguiente número disponible)

### [Categoría]

- **FR-201**: [Requisito funcional]
- **FR-202**: [Requisito funcional]

## Success Criteria

- **SC-201**: [Criterio medible de éxito]
- **SC-202**: [Criterio medible de éxito]

## Assumptions

- [Suposición 1]
- [Suposición 2]

## Out of Scope

- [Lo que NO se va a hacer]
```

---

## Paso 2: tasks.md (las tareas)

Divide la implementación en fases verificables. Cada tarea tiene:
- ID único (T001, T002...)
- Historia de usuario a la que pertenece (US1, US2...)
- Referencia a los FR que cubre
- Ruta del archivo que se modifica

### Estructura

```markdown
# Tasks: [Nombre de la feature]

**Input**: Documentos de diseño en `specs/003-nombre-de-feature/`
**Tests**: OBLIGATORIAS, no opcionales

---

## Phase 1: Setup (infraestructura)

- [ ] T001 [P] Crear directorio `lib/wiga_xxx/` con README
- [ ] T002 [P] Crear suite de pruebas en `test/test_xxx/`

**Checkpoint**: `pio test -e native` sigue en verde.

---

## Phase 2: Foundational (prerrequisitos)

- [ ] T003 Definir tipos en `lib/wiga_xxx/types.h` — cubre FR-201
- [ ] T004 [P] Pruebas de CRC en `test/test_xxx/test_crc.cpp` — cubre FR-202

**Checkpoint**: la capa pura tiene tipos y pruebas.

---

## Phase 3: User Story 1 (MVP)

### Pruebas (se escriben ANTES)

- [ ] T005 [P] [US1] Prueba de X en `test/test_xxx/test_x.cpp` — cubre FR-201

### Implementación

- [ ] T006 [US1] Implementar X en `lib/wiga_xxx/x.cpp` — cubre FR-201

**Checkpoint**: el sistema funciona para la historia 1.

---

## Phase 4+: Historias siguientes

[...]

---

## Dependencias

```
Phase 1 → Phase 2 → Phase 3 (US1) → Phase 4 (US2)
```

## Trazabilidad de requisitos

| Requisito | Tareas |
|---|---|
| FR-201 | T003, T005, T006 |
```

---

## Paso 3: Otros documentos (opcionales pero recomendados)

| Documento | Cuándo crearlo | Contenido |
|---|---|---|
| `plan.md` | Antes de codificar | Estrategia de implementación, orden, decisiones |
| `data-model.md` | Si hay tipos nuevos | Diagramas de estructuras, enums, relaciones |
| `research.md` | Si hay opciones técnicas | Pros/contras de cada alternativa |
| `quickstart.md` | Después de implementar | Cómo probar en 5 minutos |
| `contracts/*.md` | Si hay interfaces nuevas | Contratos entre capas, firmas de funciones |
| `checklists/*.md` | Siempre | Checklist de requisitos verificados |

---

## Convenciones de numeración

| Tipo | Prefijo | Ejemplo |
|---|---|---|
| Feature Requirement | FR- | FR-201, FR-202... |
| Non-Functional Requirement | NFR- | NFR-001 (se comparte entre specs) |
| Success Criteria | SC- | SC-201, SC-202... |
| Task | T | T001, T002... |
| User Story | US | US1, US2... |

**Importante:** la numeración de FR es **global**. Si la spec 002 terminó en FR-125,
la spec 003 empieza en FR-201 (dejar margen).

---

## Ejemplo rápido: "Agregar soporte para 48 válvulas"

### spec.md (resumen)

```
Contexto: el sistema actual soporta 24 válvulas. La finca X necesita 48.

User Story 1: El mapa acepta 48 válvulas sin romper compilación.
  Given el mapa con 48 filas, When compilo, Then éxito.
  Given el mapa con 49 filas, When compilo, Then error MAP-V2.

User Story 2: El LCD muestra las 48 válvulas en 6 filas.
  Given 48 válvulas, When giro el panel, Then veo todas.

Requirements:
  FR-301: kValveCount debe ser configurable sin tocar código de control.
  FR-302: El LCD debe mostrar todas las válvulas en filas de 8.

Success Criteria:
  SC-301: 48 válvulas funcionan end-to-end.
  SC-302: RAM < 75%.
```

### tasks.md (resumen)

```
Phase 1: Setup
  T001 [P] Crear spec 003-soporte-48-valvulas/

Phase 2: Foundational
  T002 Cambiar kMap[] a 48 filas — cubre FR-301
  T003 [P] Pruebas de límite del mapa — cubre FR-301

Phase 3: US1
  T004 [P] [US1] Prueba de 48 válvulas — cubre FR-301
  T005 [US1] Actualizar static_assert MAP-V2 — cubre FR-301

Phase 4: US2
  T006 [P] [US2] Prueba de LCD con 6 filas — cubre FR-302
  T007 [US2] Adaptar refreshDisplay() — cubre FR-302
```

---

## Checklist antes de crear una spec

- [ ] ¿Es suficientemente grande para justificar una spec? (Si es 1 archivo, quizás no)
- [ ] ¿Tengo claro el contexto y el problema?
- [ ] ¿Puedo escribir al menos 1 User Story?
- [ ] ¿Puedo medir el éxito (Success Criteria)?
- [ ] ¿Sé qué está OUT of Scope?
- [ ] ¿Conozco la numeración FR siguiente disponible?
