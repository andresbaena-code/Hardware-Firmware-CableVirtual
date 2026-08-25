# Acta de pruebas de banco — Firmware Cable Virtual V2

Documento de aceptación para imprimir y firmar. Cubre los escenarios 6 a 10 de la
[guía de validación](../specs/001-migracion-platformio/quickstart.md), que son los que
requieren hardware.

> **Regla de oro:** ningún resultado se anota de memoria. Cada casilla se marca en el
> momento, con la evidencia delante. Si algo no se probó, se marca **NP** (no probado), no
> se deja en blanco ni se supone correcto.

---

## Datos de la sesión

| Campo | Valor |
|---|---|
| Fecha | ______ / ______ / ____________ |
| Responsable de la prueba | ______________________________________________ |
| Versión de firmware (línea `VER` de la consola) | ______________________________ |
| Fecha de compilación (consola) | ______________________________________ |
| Commit (`git rev-parse --short HEAD`) | ______________________________ |
| Instalación / finca de referencia | ______________________________________ |

## Equipo utilizado

| Elemento | Modelo / serie | Observaciones |
|---|---|---|
| Arduino Mega 2560 | | |
| Módulo XBee coordinador (`SL`) | | |
| Nodo remoto 1 (`SL`) | | |
| Nodo remoto 2 (`SL`) | | |
| Panel LCD I2C 20×4 (dirección) | | |
| Fuente / batería | | tensión en vacío: ________ V |
| Multímetro | | |

---

## Bloque 0 — Requisito previo bloqueante

> **No continuar si este bloque no pasa.** Todo lo demás depende de que los nodos
> instalados acepten el formato de trama que este firmware transmite.

### 0.1 — Los nodos aceptan el payload de 5 bytes con verificador (R-012, FR-015)

El firmware transmite 5 bytes: `AA 14 <id> <nivel> <verificador>`. Las versiones antiguas
del sistema transmitían 4, sin verificador. Hay que confirmar contra un nodo real que la
forma de 5 bytes se acepta.

| Paso | Resultado esperado | Obtenido | ✔ / ✘ / NP |
|---|---|---|---|
| Conmutar una entrada cualquiera | La consola muestra `TX` | | |
| Observar el nodo | La válvula acciona físicamente | | |
| Observar la consola | Aparece `ACK` de esa válvula | | |

**Si no hay `ACK`:** el nodo no acepta el payload de 5 bytes. **Detener la puesta en
producción** y escalar: hay que decidir si se ajusta el firmware del controlador o se
actualiza el de los nodos.

Firma responsable del bloque 0: ______________________________

---

## Bloque 1 — Escenario 6: arranque y diagnóstico (FR-010, FR-026)

```bash
pio run -e megaatmega2560 -t upload
pio device monitor -b 9600
```

| # | Comprobación | Esperado | Obtenido | ✔ / ✘ / NP |
|---|---|---|---|---|
| 1.1 | Identificación al arrancar | nombre, versión y fecha de compilación | | |
| 1.2 | Evento `BOOT` | aparece con la causa del reinicio | | |
| 1.3 | Causa tras conectar alimentación | `ALIMENTACION` | | |
| 1.4 | Causa tras pulsar reset | `EXTERNO` | | |
| 1.5 | Panel LCD | 3 filas de 8 símbolos + línea de identificación | | |
| 1.6 | **Ninguna orden al arrancar** | **cero** eventos `TX` antes de tocar una entrada | | |

> **1.6 es crítico (Principio II).** Si aparecen eventos `TX` al arrancar, cada reinicio
> por watchdog dispara una ráfaga de solenoides contra la batería. Es motivo de rechazo.

### 1.7 — Causa `WATCHDOG` (limitación conocida, R-008)

Provocar un reinicio por watchdog y observar la causa reportada.

- [ ] Reporta `WATCHDOG` correctamente
- [ ] Reporta otra causa → el bootloader limpia `MCUSR` antes de leerse. **No es un
      defecto del firmware.** Anotar aquí y en `docs/diagnostico.md`, tarea T062.

Causa reportada: ________________________

---

## Bloque 2 — Escenario 7: el campo sigue al tablero (US-1, SC-001)

**Criterio de aprobación: 24 de 24.** Cualquier fila con ✘ bloquea la puesta en producción
de esa válvula.

Para cada entrada: conmutar, verificar `TX` en consola, luego `ACK`, luego el símbolo en el
panel, y por último el accionamiento físico de la válvula.

| Válv. | Nodo (`SL`) | Id | Pin | `TX` | `ACK` | Panel | Válvula acciona | ✔ / ✘ / NP |
|---|---|---|---|---|---|---|---|---|
| 0 | | | A0 | | | | | |
| 1 | | | A1 | | | | | |
| 2 | | | A2 | | | | | |
| 3 | | | A3 | | | | | |
| 4 | | | A4 | | | | | |
| 5 | | | A5 | | | | | |
| 6 | | | A6 | | | | | |
| 7 | | | A7 | | | | | |
| 8 | | | A8 | | | | | |
| 9 | | | 27 | | | | | |
| 10 | | | 5 | | | | | |
| 11 | | | 8 | | | | | |
| 12 | | | 46 | | | | | |
| 13 | | | 44 | | | | | |
| 14 | | | 11 | | | | | |
| 15 | | | 12 | | | | | |
| 16 | | | 10 | | | | | |
| 17 | | | 45 | | | | | |
| 18 | | | 9 | | | | | |
| 19 | | | A9 | | | | | |
| 20 | | | A12 | | | | | |
| 21 | | | A14 | | | | | |
| 22 | | | A13 | | | | | |
| 23 | | | A15 | | | | | |

**Total correctas: ______ / 24**

> **Rellenar las columnas «Nodo» e «Id» desde el plano de la finca, no desde el código.**
> Esa comparación es la verificación del mapa (tarea T055). Prestar atención a las válvulas
> **18 a 22**, que en el mapa heredado comparten el nodo `0x41C3F595` con los
> identificadores 2 a 6 — patrón distinto al del resto de nodos y sin verificar.

Discrepancias encontradas entre el mapa y el plano:

```
_________________________________________________________________________

_________________________________________________________________________
```

---

## Bloque 3 — Escenario 8: disparo secuencial y protección de batería

### 3.1 — Exclusión mutua (FR-004)

Conmutar 5 entradas simultáneamente y anotar las marcas de tiempo de la consola.

| Orden | Válvula | Marca `TX` (ms) | Δ respecto al anterior (ms) | ≥ 1500 ms |
|---|---|---|---|---|
| 1.º | | | — | — |
| 2.º | | | | |
| 3.º | | | | |
| 4.º | | | | |
| 5.º | | | | |

**Falla si** algún Δ es menor que 1 500 ms.

### 3.2 — Reintentos acotados (FR-006, FR-007, SC-005)

Apagar un nodo y conmutar una de sus entradas. Anotar cada `TX` durante 90 minutos.

| Intento | Marca `TX` (ms) | Δ respecto al anterior | Esperado |
|---|---|---|---|
| 1 | | — | inmediato |
| 2 | | | ~30 s |
| 3 | | | ~30 s |
| 4 | | | ~30 s |
| 5 | | | ~30 s |
| `BACK` | | | tras el 5.º |
| 6 | | | **~60 min** después |

| Comprobación | Esperado | Obtenido | ✔ / ✘ / NP |
|---|---|---|---|
| `TX` en los primeros 30 min | exactamente **5** | | |
| Evento `BACK` | aparece una vez | | |
| `TX` entre el minuto 5 y el 60 | **cero** | | |
| Símbolo en el panel | `!` | | |

**Falla si** hay más de 5 `TX` en la primera media hora. Sería una regresión de consumo
frente al firmware anterior (NFR-003).

### 3.3 — Aislamiento de fallos (FR-009)

Durante 3.2, con la válvula averiada en reposo, operar válvulas de otro nodo encendido.

| Comprobación | Esperado | Obtenido | ✔ / ✘ / NP |
|---|---|---|---|
| Las otras válvulas responden | sin retraso perceptible | | |
| La averiada sigue en reposo | sin `TX` adicionales | | |

### 3.4 — Cancelación del reposo (FR-008)

Durante el reposo de 3.2, conmutar de nuevo esa entrada.

| Comprobación | Esperado | Obtenido | ✔ / ✘ / NP |
|---|---|---|---|
| `TX` inmediato | sí | | |
| Contador de intentos | vuelve a 1 | | |

---

## Bloque 4 — Escenario 9: ninguna confirmación se pierde (FR-017, FR-018)

Conmutar dos entradas de nodos **distintos** con menos de 100 ms de diferencia, de modo que
las dos confirmaciones lleguen casi juntas. **Repetir 20 veces.**

| Repetición | 2 × `ACK` | Algún `TMO` indebido | Repetición | 2 × `ACK` | Algún `TMO` indebido |
|---|---|---|---|---|---|
| 1 | | | 11 | | |
| 2 | | | 12 | | |
| 3 | | | 13 | | |
| 4 | | | 14 | | |
| 5 | | | 15 | | |
| 6 | | | 16 | | |
| 7 | | | 17 | | |
| 8 | | | 18 | | |
| 9 | | | 19 | | |
| 10 | | | 20 | | |

**Repeticiones correctas: ______ / 20.** Criterio: **20 de 20**.

Un `TMO` sobre una válvula cuyo nodo sí respondió significa que se descartó una
confirmación: es exactamente el defecto que esta versión corrige.

---

## Bloque 5 — Escenario 10: presupuesto del lazo (Principio I)

Dejar el sistema operando 10 minutos con tráfico de radio y refresco de panel.

| Comprobación | Esperado | Obtenido | ✔ / ✘ / NP |
|---|---|---|---|
| Tiempo máximo de vuelta | ≤ 2 ms | ________ ms | |
| Refresco del panel | progresivo, no "de golpe" | | |
| Reinicios por watchdog | **cero** | | |

Un refresco "de golpe" indica una escritura completa bloqueante del LCD.

### 5.1 — Tolerancia a fallo del panel (FR-022, DP-03)

Desconectar el LCD en caliente durante la operación.

| Comprobación | Esperado | Obtenido | ✔ / ✘ / NP |
|---|---|---|---|
| El control de válvulas sigue | sin interrupción | | |
| No hay reinicio por watchdog | correcto | | |
| Evento `LCD` en consola | aparece tras varios fallos | | |

---

## Registro de memoria (puerta de calidad 3 de la constitución)

Del reporte de compilación de la versión probada:

| Recurso | Usado | Total | % | Límite |
|---|---|---|---|---|
| RAM | ________ B | 8 192 B | ______ % | 75 % |
| Flash | ________ B | 253 952 B | ______ % | — |

---

## Resultado

| Bloque | Resultado |
|---|---|
| 0 — Compatibilidad de payload (**bloqueante**) | ☐ Aprobado ☐ Rechazado ☐ NP |
| 1 — Arranque y diagnóstico | ☐ Aprobado ☐ Rechazado ☐ NP |
| 2 — El campo sigue al tablero (24/24) | ☐ Aprobado ☐ Rechazado ☐ NP |
| 3 — Disparo secuencial y batería | ☐ Aprobado ☐ Rechazado ☐ NP |
| 4 — Confirmaciones no se pierden (20/20) | ☐ Aprobado ☐ Rechazado ☐ NP |
| 5 — Presupuesto del lazo | ☐ Aprobado ☐ Rechazado ☐ NP |

### Dictamen

- ☐ **Apto para sustituir firmware en producción.** Todos los bloques aprobados.
- ☐ **Apto con reservas.** Detallar debajo qué queda pendiente y bajo qué condición.
- ☐ **No apto.** Detallar el motivo.

Observaciones, incidencias y desviaciones:

```
_________________________________________________________________________

_________________________________________________________________________

_________________________________________________________________________

_________________________________________________________________________
```

| | Nombre | Firma | Fecha |
|---|---|---|---|
| Ejecutó la prueba | | | |
| Responsable técnico de firmware | | | |

---

## Después de la sesión

1. Escanear esta acta y guardarla junto al repositorio, o adjuntarla al pull request.
2. Marcar en [`tasks.md`](../specs/001-migracion-platformio/tasks.md) las tareas T053,
   T055, T062, T067 y T068 según lo verificado.
3. Registrar en [`docs/configuracion-finca.md`](configuracion-finca.md) el resultado de la
   comparación entre el mapa y el plano (bloque 2).
4. Si el bloque 1.7 no reportó `WATCHDOG`, documentar la limitación en
   [`docs/diagnostico.md`](diagnostico.md).
