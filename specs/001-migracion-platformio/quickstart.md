# Fase 1 — Guía de validación: Cable Virtual sobre PlatformIO

**Feature**: `001-migracion-platformio` | **Fecha**: 2026-08-20

Esta guía describe **cómo comprobar que la funcionalidad quedó bien hecha**, no cómo
implementarla. Los escenarios 1 a 5 se ejecutan sin hardware; los escenarios 6 a 9
requieren banco. Ninguno debe darse por aprobado de memoria: cada uno deja evidencia.

---

## Prerrequisitos

| Requisito | Verificación |
|---|---|
| Visual Studio Code | instalado |
| Extensión PlatformIO IDE | instalada (PlatformIO Core 6.1.19 verificado en el equipo) |
| Repositorio clonado | `git clone https://github.com/andresbaena-code/Hardware-Firmware-CableVirtual` |
| Para escenarios 6-9 | Arduino Mega 2560, XBee coordinador, ≥ 1 nodo remoto, LCD I2C 20×4, fuente 12 V |

En Windows, si `pio` no está en el `PATH`, el ejecutable está en
`%USERPROFILE%\.platformio\penv\Scripts\pio.exe`.

---

## Escenario 1 — Compilación reproducible desde cero (FR-030, SC-002, US-2)

**Objetivo**: un equipo limpio compila el firmware sin pasos manuales.

```bash
pio run -e megaatmega2560
```

**Resultado esperado**:
- Las dependencias `andrewrapp/XBee` y `marcoschwartz/LiquidCrystal_I2C` se descargan solas
  en las versiones fijadas.
- La compilación termina en `SUCCESS`, sin errores ni advertencias nuevas.
- El reporte de memoria muestra **RAM < 75 %** (NFR-001).

**Evidencia a registrar**: las dos líneas `RAM:` y `Flash:` del reporte, en el commit o el
acta de aceptación (puerta de calidad 3 de la constitución).

**Falla si**: pide instalar una librería a mano, o el uso de RAM supera el 75 %.

---

## Escenario 2 — La lógica se verifica sin hardware (FR-031, SC-003, US-3)

**Objetivo**: la suite completa corre en el equipo del desarrollador, sin placa conectada.

```bash
pio test -e native
```

**Resultado esperado**:
- Todas las pruebas de `test_domain`, `test_control` y `test_integration` se ejecutan.
- Resultado `PASSED` en todas.
- Tiempo total **< 30 segundos** (SC-003).

**Falla si**: alguna prueba requiere un puerto serie, o la suite tarda más de 30 s.

---

## Escenario 3 — La barrera de capas es real (Principio III, CP-03)

**Objetivo**: comprobar que la separación no depende de la disciplina del desarrollador.

1. Añadir temporalmente `#include <Arduino.h>` en
   `lib/wiga_control/valve_supervisor.h`.
2. Ejecutar `pio test -e native`.

**Resultado esperado**: la compilación **falla** con un error de archivo no encontrado.

3. Revertir el cambio y confirmar que la suite vuelve a pasar.

**Falla si**: la compilación tiene éxito. Significa que el entorno `native` está arrastrando
el framework Arduino y el Principio III no está garantizado por el build.

---

## Escenario 4 — El mapa mal configurado no compila (FR-025, MAP-V3, MAP-V4)

**Objetivo**: un error de mapeo se detecta en el escritorio, no en campo.

1. En `lib/wiga_config/installation_map.h`, duplicar el pin de la válvula 5 en la válvula 6.
2. Ejecutar `pio run -e megaatmega2560`.

**Resultado esperado**: falla con `static_assert` y el mensaje
`pin de entrada duplicado en el mapa`.

3. Revertir. Repetir duplicando un par (dirección, `valveId`): debe fallar con
   `par direccion/id duplicado en el mapa`.
4. Revertir y confirmar que compila.

**Falla si**: cualquiera de los dos casos compila sin error.

---

## Escenario 5 — Configurar una finca nueva toca un solo archivo (SC-004, MAP-04, US-4)

**Objetivo**: la configuración está realmente centralizada.

1. Reemplazar `kInstallation` por un mapa de prueba de **6 válvulas** sobre 2 nodos.
2. Ejecutar `pio run -e megaatmega2560` y `pio test -e native`.
3. Ejecutar `git diff --name-only`.

**Resultado esperado**:
- Ambos comandos terminan en éxito con 6 válvulas, sin tocar nada más.
- El diff lista **exactamente un archivo**: `lib/wiga_config/installation_map.h`.

**Falla si**: aparece cualquier archivo de `lib/wiga_domain`, `lib/wiga_control`,
`lib/wiga_ports`, `lib/wiga_hal` o `src/` en el diff.

---

## Escenario 6 — Arranque y diagnóstico (FR-026, US-5) · *requiere banco*

```bash
pio run -e megaatmega2560 -t upload
pio device monitor -b 9600
```

**Resultado esperado en la consola, al arrancar**:
- Evento `VER` con la versión de firmware y fecha de compilación.
- Evento `BOOT` con la causa del último reinicio.
- El panel LCD muestra las tres filas de ocho válvulas y la línea de identificación.
- **Ninguna orden transmitida** durante el arranque (FR-010): la consola no debe mostrar
  eventos `TX` hasta que una entrada cambie.

**Falla si**: aparecen eventos `TX` al arrancar. Sería una ráfaga de disparos sobre la
batería tras cada reinicio.

---

## Escenario 7 — El campo sigue al tablero (US-1, SC-001) · *requiere banco*

**Objetivo**: las 24 entradas operan su válvula y el panel refleja la confirmación.

Para cada entrada `i` de 0 a 23:

1. Conmutar la entrada `i`.
2. Observar en consola: `TX i` con intento 1, y luego `ACK i`.
3. Observar en el panel: la posición `i` pasa a `?` y luego al símbolo del estado físico.
4. Confirmar físicamente el accionamiento de la válvula en el nodo.

**Resultado esperado**: **24 de 24** entradas correctas (SC-001).

**Evidencia**: tabla de 24 filas firmada, en el acta de aceptación.

---

## Escenario 8 — Disparo secuencial y protección de batería (FR-004, SC-005, NFR-003) · *requiere banco*

**Objetivo**: verificar el Principio II en el hardware real.

**8a — Exclusión mutua**: conmutar 5 entradas simultáneamente.
*Esperado*: cinco eventos `TX`, separados **≥ 1 500 ms** entre sí. Nunca dos a la vez.
Medir con las marcas de tiempo de la consola.

**8b — Reintentos acotados**: apagar un nodo remoto y conmutar una de sus entradas.
*Esperado*: exactamente **5** eventos `TX` en los primeros 30 minutos (uno cada ~30 s), un
evento `BACK` al agotarlos, y **ningún** `TX` adicional hasta pasada 1 hora (SC-005).

**8c — Aislamiento de fallos**: durante 8b, conmutar entradas de otro nodo encendido.
*Esperado*: esas válvulas operan con normalidad y sin retraso perceptible (FR-009).

**8d — Cancelación del reposo**: durante el reposo de 8b, conmutar de nuevo esa entrada.
*Esperado*: `TX` inmediato, con el contador de intentos reiniciado (FR-008).

**Falla si**: dos `TX` separados por menos de 1 500 ms, o más de 5 `TX` en la primera media
hora sobre una válvula que no cambió de entrada.

---

## Escenario 9 — Ninguna confirmación se pierde (FR-017, FR-018, R-003) · *requiere banco*

**Objetivo**: verificar la corrección de FR-018 en el hardware real.

1. Conmutar dos entradas de nodos **distintos** con menos de 100 ms de diferencia, de modo
   que las dos confirmaciones lleguen casi juntas.
2. Repetir 20 veces.

**Resultado esperado**: en las 20 repeticiones aparecen **dos** eventos `ACK`, y ningún
`TMO` ni reintento sobre válvulas que sí respondieron.

**Falla si**: aparece algún `TMO` sobre una válvula cuyo nodo respondió. Indicaría que
una confirmación se descartó.

---

## Escenario 10 — Presupuesto del lazo (Principio I) · *requiere banco*

**Objetivo**: comprobar que ninguna vuelta del lazo bloquea.

1. Compilar con la instrumentación de tiempo de ciclo habilitada.
2. Dejar el sistema operando 10 minutos con tráfico de radio y refrescos de panel.

**Resultado esperado**: tiempo máximo de vuelta observado **≤ 2 ms**, con el panel
refrescándose de forma visible pero progresiva.

**Falla si**: alguna vuelta supera 2 ms, o el panel se refresca "de golpe" (indicio de una
escritura completa bloqueante).

---

## Escenario 11 — Operación prolongada (NFR-002, SC-006) · *diferido*

**Objetivo**: verificar el comportamiento a través del desbordamiento de `millis()`.

Se ejecuta como prueba de campo durante la puesta en marcha, no en el ciclo de desarrollo.
El comportamiento ante el desbordamiento **ya está verificado en `native`**
(`test_control/test_time_wraparound`, escenario 2), que fuerza el desbordamiento en
milisegundos en lugar de esperar 49 días.

**Evidencia**: registro de 90 días sin reinicio no explicado ni desincronía entre tablero y
campo.

---

## Resumen de cobertura

| Escenario | Sin hardware | Requisitos cubiertos |
|---|---|---|
| 1 — Compilación reproducible | ✅ | FR-030, NFR-001, SC-002 |
| 2 — Pruebas en host | ✅ | FR-031, FR-032, SC-003 |
| 3 — Barrera de capas | ✅ | Principio III, CP-03 |
| 4 — Mapa inválido no compila | ✅ | FR-025, MAP-V3, MAP-V4 |
| 5 — Configuración centralizada | ✅ | FR-023, SC-004, MAP-04 |
| 6 — Arranque y diagnóstico | banco | FR-010, FR-026 |
| 7 — Campo sigue al tablero | banco | US-1, SC-001 |
| 8 — Disparo secuencial y batería | banco | FR-004, FR-006/7/8/9, SC-005, NFR-003 |
| 9 — Confirmaciones no se pierden | banco | FR-017, FR-018 |
| 10 — Presupuesto del lazo | banco | Principio I, FR-011 |
| 11 — Operación prolongada | campo | NFR-002, SC-006 |

**Criterio de aceptación de la funcionalidad**: escenarios 1 a 5 en verde antes de
integrar a `main`; escenarios 6 a 10 en verde antes de sustituir firmware en una
instalación productiva; escenario 11 como seguimiento posterior.
