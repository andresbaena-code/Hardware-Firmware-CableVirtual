# wiga_modbus — protocolo Modbus RTU, capa pura

**Esta biblioteca NUNCA incluye `<Arduino.h>`.**

Es la regla que la justifica. `platformio.ini` excluye `wiga_hal` de las pruebas con
`lib_ignore`, de modo que todo lo que viva alli queda fuera del alcance de la suite. La
version anterior del transporte Modbus se implemento dentro de `wiga_hal` y acumulo 25
defectos con las 73 pruebas del proyecto en verde, porque ninguna lo tocaba.

Aqui viven el tramado, el verificador de redundancia, el direccionamiento y el analisis
de peticiones: todo lo que decide algo. En `wiga_hal` solo queda un adaptador que mueve
bytes entre `Serial1` y esta capa, sin tomar ninguna decision.

Cubre el Principio III (barrera de capas) y el Principio IV (pruebas en host) de la
constitucion v2.0.0.
