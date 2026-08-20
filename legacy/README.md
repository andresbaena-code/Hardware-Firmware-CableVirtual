# Firmware anterior — material archivado

Estos sketches de Arduino son las versiones previas del sistema, conservadas **sin
modificar**. No se compilan ni se mantienen: están aquí para poder entender por qué una
instalación antigua se comporta distinto.

El firmware vigente está en la raíz del repositorio y es un proyecto PlatformIO.

---

## Qué hay aquí

### `CABLE_VIRTUAL___LATCH_V1/`

**Es la línea base de la migración.** Válvulas latch, disparo secuencial para no agotar la
batería, reintentos rápidos de 30 s y persistencia de una hora, checksum en el payload.

Todo el comportamiento de esta versión se conserva en el firmware vigente. El protocolo
con los nodos es idéntico byte a byte.

Diferencias respecto al firmware actual:

| Aspecto | Este sketch | Firmware vigente |
|---|---|---|
| Temporización | contando vueltas de lazo de 15 ms | tiempo real con `millis()` |
| Desbordamiento de `millis()` | no aplicaba (contaba vueltas) | manejado explícitamente |
| Buffer de recepción | se vaciaba antes de cada transmisión | se conserva y se procesa |
| Lectura de radio | una trama por vuelta | se drena hasta agotar el buffer |
| Antirebote de entradas | ninguno | 50 ms configurables |
| Panel LCD | refresco directo, bloqueante | buffer de sombra, 1 celda por vuelta |
| Reparto de turnos | siempre desde el índice 0 | circular desde el último atendido |
| Configuración | tres arreglos paralelos en el código | un mapa con validación al compilar |
| Pruebas | ninguna | 70 en host |

### `WIGA_24IN_LCD/`

Versión V4, para válvulas **no latch**. Escaneo secuencial de las 24 entradas con
reintentos y contador de errores. Sin checksum en el payload: transmite 4 bytes.

Si alguna instalación sigue con esta versión, sus nodos podrían no aceptar el payload de
5 bytes del firmware vigente. **Verificar antes de sustituir.**

### `WIGA_24IN_LCDB1_5/`

La versión más antigua. Antirebote por contador de lecturas (`maxM = 5`) y un pulso de
validación periódico que reenviaba el estado de cierre.

---

## Defectos conocidos de las versiones antiguas

Documentados para que **no se reintroduzcan**.

### En `WIGA_24IN_LCDB1_5`

**1. El LCD se declara con la geometría equivocada.**

```cpp
LiquidCrystal_I2C lcd(0x27, 16, 3);   // 16 columnas, 3 filas
...
lcd.setCursor(0, 3);                  // pero se escribe en la fila 4
lcd.print("SOLUCIONES WIGA!!!");      // y con 18 caracteres
```

El panel real es de 20×4. Escribir fuera de la geometría declarada produce
comportamiento indefinido en la librería.

**2. `mapear()` recorre `sizeof()` en lugar del número de elementos.**

```cpp
for (int i = 0; i < sizeof(direcciones); i++) {
```

`direcciones` es un arreglo de 24 `uint32_t`, así que `sizeof` vale **96**, no 24. El
bucle lee 72 posiciones fuera del arreglo en cada búsqueda. Que funcionara en campo fue
suerte: la memoria contigua no coincidía con ninguna dirección buscada.

**3. `pinMode()` se aplica al arreglo equivocado.**

```cpp
for (int j = 0; j < Entradas; j++) {
    pinMode(entradas[j], INPUT);      // 'entradas' son los ids 0..7, no los pines
}
```

Configura los pines 0 a 7 en vez de los pines reales del mapa, que están en `pines[]`. Los
pines de entrada nunca se configuran explícitamente y quedan en el estado por defecto.

### En `WIGA_24IN_LCD`

**4. El contador de errores se reinicia solo, sin límite real.**

```cpp
if (E[i] >= maxE) {
    actualizarPuntoLCD(i, 2, false);
    if (T[i] == 0) E[i] = 0;   // vuelve a intentar de inmediato
}
```

No hay reposo prolongado: una válvula que no responde reintenta indefinidamente, con un
disparo de solenoide cada vez. Es precisamente lo que agota la batería de 12 V, y la razón
por la que el firmware vigente hace del reposo un principio no negociable.

### En `CABLE_VIRTUAL___LATCH_V1`

**5. El buffer de recepción se vacía antes de transmitir.**

```cpp
while (Serial1.available()) Serial1.read();
```

Descarta confirmaciones ya recibidas, provocando reintentos —y disparos de solenoide—
sobre válvulas que sí habían obedecido. Corregido en el firmware vigente (requisito
FR-018).

**6. `readPacket()` se llama una sola vez por vuelta.**

La librería devuelve una única trama por llamada y deja el resto en el buffer. Con dos
confirmaciones seguidas, la segunda llegaba tarde o se perdía.

**7. El reparto de turnos favorece siempre al índice más bajo.**

El bucle recorre desde 0 y hace `break` en la primera candidata. Una válvula de índice
bajo que falla de forma persistente monopoliza los turnos y las de índice alto pueden no
atenderse nunca.
