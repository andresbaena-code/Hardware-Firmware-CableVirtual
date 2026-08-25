// -----------------------------------------------------------------------------
//  Aritmetica temporal resistente al desbordamiento de millis().
//  Cubre: FR-013, NFR-002  (research.md R-005, invariante R-04)
//
//  millis() desborda a los ~49,7 dias. La comparacion ingenua (now >= deadline)
//  falla exactamente una vez por desbordamiento y deja un temporizador colgado
//  hasta 49 dias. Con NFR-002 exigiendo 90 dias continuos, eso ocurre seguro.
// -----------------------------------------------------------------------------
#pragma once

#include <stdint.h>

namespace wiga {

/// true si han pasado al menos `interval` ms desde `since`, con aritmetica
/// modular sin signo. Correcto a traves del desbordamiento de 32 bits.
constexpr bool elapsed(uint32_t now, uint32_t since, uint32_t interval) {
    return static_cast<uint32_t>(now - since) >= interval;
}

/// true si `now` alcanzo o paso el plazo `deadline`.
///
/// Usa la diferencia con signo: (int32_t)(now - deadline) >= 0. Es la forma
/// estandar de comparar marcas de tiempo modulares y sigue siendo correcta
/// cuando `deadline` desbordo y `now` todavia no, o viceversa.
///
/// Valida mientras la distancia entre ambas marcas sea menor que 2^31 ms
/// (~24,8 dias). El plazo mas largo del sistema es de 1 hora (kBackoffIntervalMs),
/// asi que la condicion se cumple con tres ordenes de magnitud de margen.
constexpr bool reached(uint32_t now, uint32_t deadline) {
    return static_cast<int32_t>(now - deadline) >= 0;
}

}  // namespace wiga
