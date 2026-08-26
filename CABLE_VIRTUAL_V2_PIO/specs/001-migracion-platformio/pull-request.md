# Descripcion del pull request

Titulo: **Migracion del firmware Cable Virtual a PlatformIO (V2)**

Rama: `001-migracion-platformio` -> `main`

---

## Que hace este PR

Migra el firmware de control de 24 valvulas latch del sketch monolitico
`CABLE_VIRTUAL___LATCH_V1.ino` a un proyecto PlatformIO con arquitectura en capas, en la
carpeta `CABLE_VIRTUAL_V2_PIO/`. **Los tres sketches originales quedan intactos en la raiz**
y no se modifican.

El protocolo con los nodos ya instalados en campo se conserva byte a byte.

## Verificado

| Puerta | Resultado |
|---|---|
| `pio run -e megaatmega2560` | RAM 22,2 % (1822/8192 B), Flash 4,8 % |
| `pio test -e native` | 71 pruebas, todas en verde, 18 s |
| Barrera de capas | incluir `<Arduino.h>` en el dominio rompe el build |
| Trazabilidad | 38/38 requisitos citados en codigo o infraestructura |

Las cuatro puertas corren automaticamente en cada push (`.github/workflows/ci.yml`).

## Tres hallazgos que no estaban documentados en ninguna parte

1. **Los radios operan en modo API escapado (AP=2).** Verificado leyendo el codigo de la
   libreria XBee, que escapa siempre en transmision (`XBee.h:35` fija `ATAP 2`). Transmitir
   en AP=1 haria que los nodos descartaran cualquier trama con un byte reservado, con
   fallos intermitentes imposibles de diagnosticar.

2. **`readPacket()` devuelve una sola trama por llamada.** El sketch base la llamaba una vez
   por vuelta *y ademas* vaciaba el buffer de recepcion antes de transmitir. Entre las dos
   cosas, explican reintentos -y disparos de solenoide sobre la bateria- en valvulas que si
   habian respondido. Corregido (FR-018).

3. **La inversion del panel LCD no es un defecto.** La etapa de potencia de los nodos usa
   logica inversa por transistor. Ahora se modela con dos tipos que el compilador no deja
   mezclar (`SignalLevel` y `ValveState`), para que nadie la "corrija" y rompa las
   instalaciones.

## Otras correcciones respecto al firmware base

- La temporizacion pasa de contar vueltas de lazo a tiempo real, con aritmetica que
  sobrevive al desbordamiento de `millis()` a los ~49 dias (NFR-002 exige 90).
- Antirebote de 50 ms en las entradas: el sketch base no tenia, y cualquier microcorte del
  contacto disparaba una orden latch.
- El reparto de turnos es circular: antes una valvula de indice bajo que fallaba de forma
  persistente monopolizaba los turnos.
- El mapa de finca se valida al compilar: un pin duplicado o un par (direccion, id)
  repetido **no compila**.
- El panel escribe una celda por vuelta con timeout de bus: un LCD averiado ya no puede
  colgar el lazo de control.

## Documentacion

Ciclo completo de GitHub Spec Kit en `CABLE_VIRTUAL_V2_PIO/specs/001-migracion-platformio/`:
constitucion, especificacion (35 FR + 3 NFR + 8 criterios), investigacion, modelo de datos,
contratos, guia de validacion y 75 tareas.

Guias de operacion en `CABLE_VIRTUAL_V2_PIO/docs/`.

## ANTES DE FUSIONAR Y DESPLEGAR

Este PR **no debe llevar a sustituir firmware en produccion** sin antes ejecutar los
escenarios de banco de `docs/acta-pruebas-banco.md`. En particular el **bloque 0**, que es
bloqueante: confirmar contra un nodo real que acepta el payload de 5 bytes con verificador.

Quedan 6 tareas abiertas, todas dependientes de hardware o de terceros:

- **T055** contrastar el mapa heredado contra el plano de la finca. Ojo con las valvulas
  18-22, que comparten el nodo `0x41C3F595` con ids 2 a 6, patron distinto al del resto.
- **T053, T067, T068** verificacion en banco.
- **T062** comprobar si `optiboot` limpia `MCUSR` antes de leerse.
- **T071** revision de cumplimiento constitucional por otra persona.
