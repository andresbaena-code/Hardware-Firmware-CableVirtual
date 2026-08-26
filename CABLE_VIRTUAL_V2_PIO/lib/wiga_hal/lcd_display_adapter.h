// -----------------------------------------------------------------------------
//  Adaptador de IDisplayPort sobre LiquidCrystal_I2C.
//  Contrato: ports.md seccion 4 (DP-01..DP-05). Cubre: FR-019, FR-022
//
//  La libreria es sincrona: un refresco completo de 80 celdas superaria los
//  100 ms y bloquearia el lazo. Por eso el adaptador mantiene un buffer de
//  sombra con marcas de suciedad y escribe UNA celda por vuelta, con el bus a
//  400 kHz (research.md R-007).
// -----------------------------------------------------------------------------
#pragma once

#include <LiquidCrystal_I2C.h>

#include "i_display_port.h"
#include "timing_config.h"

namespace wiga {
namespace hal {

class LcdDisplayAdapter : public IDisplayPort {
public:
    LcdDisplayAdapter();

    void begin() override;
    void setValveSymbol(ValveIndex index, char symbol) override;
    void setStatusLine(const char* text) override;
    void tick() override;
    bool healthy() const override { return healthy_; }

private:
    void markDirty(uint8_t row, uint8_t col);
    bool isDirty(uint8_t row, uint8_t col) const;
    void clearDirty(uint8_t row, uint8_t col);
    void writeCell(uint8_t row, uint8_t col);

    static constexpr uint8_t kRows  = config::kLcdRows;
    static constexpr uint8_t kCols  = config::kLcdColumns;
    static constexpr uint8_t kFlags = (kRows * kCols + 7) / 8;

    LiquidCrystal_I2C lcd_;
    char    cells_[kRows][kCols];
    uint8_t dirty_[kFlags];
    uint8_t cursor_              = 0;
    uint8_t consecutiveFailures_ = 0;
    uint16_t recoveryCounter_    = 0;   ///< RA-08: intentos de recuperacion del panel
    bool    healthy_             = true;
};

}  // namespace hal
}  // namespace wiga
