<!--
SYNC IMPACT REPORT
==================
Version change: (plantilla sin versionar) -> 1.0.0
Bump rationale: MAJOR inicial. Primera ratificación del documento; se sustituyen
todos los placeholders de la plantilla por principios concretos del proyecto.

Principios definidos (nuevos, ninguno renombrado):
  - I.   Determinismo del Lazo de Control
  - II.  Seguridad de Válvula y Protección Eléctrica (NO NEGOCIABLE)
  - III. Arquitectura en Capas con Hardware Abstraído
  - IV.  Pruebas en Host Antes que Hardware (NO NEGOCIABLE)
  - V.   Configuración Centralizada, Cero Números Mágicos
  - VI.  Observabilidad y Diagnóstico por Puerto Serie
  - VII. Trazabilidad Requisito -> Código -> Prueba

Secciones agregadas:
  - Restricciones Técnicas y de Plataforma   (reemplaza [SECTION_2_NAME])
  - Flujo de Desarrollo y Puertas de Calidad (reemplaza [SECTION_3_NAME])
  - Governance

Secciones removidas: ninguna.

Follow-up TODOs: ninguno. Todos los placeholders fueron resueltos.
-->

# Constitución del Firmware WIGA Cable Virtual

Este documento gobierna el desarrollo del firmware de control de válvulas de riego
"Cable Virtual" de WIGA. Aplica a todo el código, documentación y proceso del
repositorio. Sus reglas prevalecen sobre preferencias individuales, costumbres
heredadas del código Arduino original y sobre la conveniencia de corto plazo.

## Core Principles

### I. Determinismo del Lazo de Control

El lazo principal DEBE ejecutarse en un tiempo acotado y predecible. Queda
PROHIBIDO el uso de `delay()`, esperas activas sobre periféricos y cualquier
bloqueo mayor a 2 ms dentro del ciclo de control. Toda temporización se expresa
como máquina de estados no bloqueante basada en `millis()`, con aritmética de
desbordamiento segura (`(uint32_t)(ahora - marca) >= intervalo`).

El watchdog DEBE permanecer habilitado en operación normal y su refresco DEBE
ocurrir en un único punto del lazo principal; jamás dentro de una función de
espera ni de un manejador de error.

*Racional:* el sistema controla riego en campo sin operario presente. Un lazo que
se bloquea deja válvulas en estado indefinido, y el reinicio por watchdog pierde
el estado de las 24 salidas. El código Arduino original usaba `delay(500)` dentro
del disparo de válvula, lo que retrasaba la lectura de confirmaciones de radio.

### II. Seguridad de Válvula y Protección Eléctrica (NO NEGOCIABLE)

Ante cualquier fallo o incertidumbre, el firmware DEBE converger al estado seguro
de la instalación (válvula cerrada) y NUNCA repetir órdenes de forma ilimitada.

Reglas obligatorias:

- **Disparo secuencial:** solo UNA orden de válvula latch puede transmitirse a la
  vez, con una ventana de exclusión configurable entre disparos. Prohibido el
  disparo simultáneo de múltiples solenoides.
- **Reintentos acotados:** un cambio de estado no confirmado se reintenta un
  número finito de veces con periodo definido; agotados los intentos, el canal
  entra en reposo prolongado antes de volver a intentar.
- **Confirmación obligatoria:** un estado se considera real solo cuando el nodo
  remoto lo confirma. La orden enviada NO actualiza el estado real por sí sola.
- **Sin ráfagas al reiniciar:** tras un reset por watchdog el firmware DEBE
  reconstruir su estado desde las entradas físicas sin emitir una ráfaga de
  órdenes.

*Racional:* la instalación opera con batería de 12 V / 9 Ah. Cada disparo latch es
un pico de corriente; una ráfaga de reintentos agota la batería y estresa los
solenoides. Un fallo de radio no puede convertirse en un fallo de campo.

### III. Arquitectura en Capas con Hardware Abstraído

El código DEBE separarse en capas con dependencias en una sola dirección:

```
aplicación (lógica de control)  ->  dominio (estado de válvulas, protocolo)
        |                                        |
        v                                        v
             puertos (interfaces abstractas)
                          |
                          v
     adaptadores (Arduino, XBee, LiquidCrystal_I2C, watchdog)
```

La lógica de control y el dominio NO PUEDEN incluir `<Arduino.h>`, `<XBee.h>`,
`<LiquidCrystal_I2C.h>` ni `<avr/wdt.h>`. Todo acceso a hardware pasa por una
interfaz declarada en la capa de puertos. Prohibido el estado global mutable
compartido entre módulos; el estado vive en objetos con propietario único.

*Racional:* sin esta separación la lógica de riego no es verificable fuera de la
placa, y cada prueba exige subir firmware a un Mega conectado a radios reales.

### IV. Pruebas en Host Antes que Hardware (NO NEGOCIABLE)

Toda regla de negocio (máquina de estados de válvula, política de reintentos,
codificación y validación del protocolo XBee, mapeo de índices) DEBE tener prueba
unitaria ejecutable en el entorno `native` de PlatformIO, sin hardware.

- Las pruebas se escriben ANTES o JUNTO al código que verifican, nunca después de
  darlo por terminado.
- Ningún cambio en la lógica de control se da por completo si `pio test -e native`
  no pasa.
- Los defectos corregidos DEBEN incorporar una prueba que reproduzca el fallo.
- La cobertura de hardware (LCD, radio física, entradas) se valida con una lista
  de verificación documentada en banco, no con pruebas automatizadas.

*Racional:* el ciclo compilar-cargar-observar en campo cuesta horas. Una prueba en
host cuesta segundos y detecta la mayoría de los errores de lógica.

### V. Configuración Centralizada, Cero Números Mágicos

Todo parámetro de instalación (direcciones de radio de 64 bits, identificadores de
válvula, mapeo de pines, tiempos, umbrales, dirección I2C, geometría del LCD) DEBE
declararse en un único módulo de configuración, con tipo explícito, unidad en el
nombre o el comentario, y `constexpr` cuando sea posible.

Queda PROHIBIDO en el código de lógica: literales numéricos sin nombre, tamaños de
arreglo repetidos y constantes duplicadas entre módulos. El número de válvulas
DEBE derivarse del tamaño del arreglo de configuración, nunca escribirse dos veces.

*Racional:* el firmware original tenía 24 direcciones y 24 pines en tres arreglos
paralelos editados a mano, con comentarios contradictorios sobre qué dirección
correspondía a qué bloque. Ese es exactamente el error que esta regla previene.

### VI. Observabilidad y Diagnóstico por Puerto Serie

El firmware DEBE exponer por el puerto serie de depuración, de forma no bloqueante:

- Identificación de versión y fecha de compilación al arrancar.
- Causa del último reinicio, incluyendo reset por watchdog.
- Cada transición de estado de válvula: índice, estado solicitado, número de
  intento, y confirmación o expiración.
- Contadores acumulados de errores de radio por canal.

Las trazas DEBEN tener nivel de severidad y poder desactivarse en compilación sin
alterar la lógica. Prohibido que una traza cambie el comportamiento del control.

*Racional:* el diagnóstico en campo se hace con un portátil y un cable USB. Si el
firmware no cuenta lo que hace, el técnico depura a ciegas.

### VII. Trazabilidad Requisito -> Código -> Prueba

Toda funcionalidad entra por el ciclo Spec Kit: `/speckit-specify` ->
`/speckit-clarify` -> `/speckit-plan` -> `/speckit-tasks` -> `/speckit-implement`.
Queda PROHIBIDO modificar la lógica de control sin una especificación asociada.

Cada requisito funcional lleva identificador estable (`FR-xxx`), y ese
identificador DEBE aparecer en el comentario del módulo que lo implementa y en el
nombre o comentario de la prueba que lo verifica. Los mensajes de commit
referencian la carpeta de la especificación correspondiente.

*Racional:* el conocimiento del sistema vive hoy en la memoria de una persona y en
comentarios sueltos. La trazabilidad convierte ese conocimiento en un activo del
proyecto.

## Restricciones Técnicas y de Plataforma

- **Placa objetivo:** Arduino Mega 2560 (ATmega2560, 8 KB SRAM, 256 KB Flash). El
  consumo de SRAM reportado en compilación DEBE mantenerse por debajo del 75 %
  para dejar margen de pila; las cadenas literales van en Flash (`F()` /
  `PROGMEM`).
- **Entorno de construcción:** PlatformIO sobre Visual Studio Code. La compilación
  DEBE ser reproducible desde `platformio.ini` sin pasos manuales. Prohibido
  depender del IDE de Arduino o de librerías instaladas globalmente: toda
  dependencia se declara con versión fijada.
- **Entornos mínimos:** `megaatmega2560` (producción) y `native` (pruebas en host).
- **Comunicación:** XBee ZigBee en modo API sobre `Serial1` a 9600 baudios; consola
  de depuración sobre `Serial`. El protocolo de aplicación se documenta en la
  especificación, no se infiere del código.
- **Lenguaje:** C++ con las capacidades soportadas por el avr-gcc del toolchain de
  PlatformIO. Prohibida la asignación dinámica de memoria (`new`, `malloc`,
  `String`, contenedores STL con heap) en el camino de ejecución.
- **Idioma:** documentación, especificaciones y comentarios en español; los
  identificadores del código en inglés y consistentes en todo el repositorio.

## Flujo de Desarrollo y Puertas de Calidad

1. **Especificación primero.** Ninguna rama de funcionalidad se abre sin su carpeta
   en `specs/`.
2. **Ramas por funcionalidad.** Una rama `NNN-nombre-corto` por especificación.
   Prohibido comprometer lógica de control directamente en `main`.
3. **Puertas obligatorias antes de integrar a `main`:**
   - `pio run -e megaatmega2560` compila sin errores ni advertencias nuevas.
   - `pio test -e native` pasa en su totalidad.
   - El uso de SRAM y Flash reportado se registra en el commit o el PR.
   - La revisión verifica cumplimiento explícito de los Principios I, II y IV.
4. **Complejidad justificada.** Toda desviación de un principio DEBE documentarse
   en el plan de la funcionalidad, con la alternativa más simple descartada y el
   motivo del descarte. Una desviación sin justificar bloquea la integración.
5. **Cambios en campo.** Ninguna corrección aplicada en sitio se queda fuera del
   repositorio; se incorpora con su especificación o como corrección trazada.

## Governance

Esta constitución prevalece sobre cualquier otra práctica del proyecto. En caso de
conflicto entre esta constitución y una especificación, un plan o una revisión,
prevalece la constitución hasta que sea formalmente enmendada.

**Procedimiento de enmienda:** toda modificación se propone por escrito indicando
el principio afectado, la motivación y el impacto sobre el código existente. La
enmienda se aplica ejecutando `/speckit-constitution`, que regenera este documento
con su Sync Impact Report. Requiere aprobación del responsable técnico del firmware.

**Política de versionado (semántico):**
- **MAJOR:** eliminación o redefinición incompatible de un principio o de una regla
  de gobernanza.
- **MINOR:** incorporación de un principio o sección nueva, o ampliación material
  de una guía existente.
- **PATCH:** aclaraciones, correcciones de redacción y ajustes sin efecto semántico.

**Revisión de cumplimiento:** cada revisión de código verifica explícitamente los
principios aplicables antes de aprobar. Las revisiones DEBEN rechazar cambios que
violen un principio NO NEGOCIABLE (II y IV) aunque el código funcione. La guía
operativa de desarrollo en tiempo de ejecución vive en `CLAUDE.md` en la raíz del
repositorio y no puede contradecir este documento.

**Version**: 1.0.0 | **Ratified**: 2026-08-20 | **Last Amended**: 2026-08-20
