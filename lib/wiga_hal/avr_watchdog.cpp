// Cubre: FR-012
#include "avr_watchdog.h"

#include <avr/wdt.h>

namespace wiga {
namespace hal {

void AvrWatchdog::disable() { wdt_disable(); }

void AvrWatchdog::enable() { wdt_enable(WDTO_8S); }

void AvrWatchdog::feed() { wdt_reset(); }

}  // namespace hal
}  // namespace wiga
