// Cubre: FR-019, FR-020, FR-022  (research.md R-007)
#include "lcd_display_adapter.h"

#include <Arduino.h>
#include <Wire.h>

#include "installation_map.h"

namespace wiga {
namespace hal {
namespace {

/// Etiqueta de cada fila de valvulas: "E1: ", "E2: ", "E3: ".
void rowLabel(uint8_t row, char* out) {
    out[0] = 'E';
    out[1] = static_cast<char>('1' + row);
    out[2] = ':';
    out[3] = ' ';
}

}  // namespace

// RA-05: verificar que las dimensiones del LCD caben en uint16_t.
static constexpr uint16_t kLcdTotalCells =
    static_cast<uint16_t>(config::kLcdRows) * config::kLcdColumns;
static_assert(kLcdTotalCells <= UINT16_MAX,
              "LCD dimensions exceed uint16_t capacity");

LcdDisplayAdapter::LcdDisplayAdapter()
    : lcd_(config::kLcdI2cAddress, config::kLcdColumns, config::kLcdRows) {
    for (uint8_t r = 0; r < kRows; ++r) {
        for (uint8_t c = 0; c < kCols; ++c) cells_[r][c] = ' ';
    }
    for (uint8_t i = 0; i < kFlags; ++i) dirty_[i] = 0;
}

void LcdDisplayAdapter::begin() {
    // La inicializacion del HD44780 lleva decenas de milisegundos, pero ocurre
    // en setup(), antes de habilitar el watchdog. No afecta al lazo.
    lcd_.init();
    lcd_.backlight();
    Wire.setClock(config::kI2cClockHz);  // 400 kHz: ~0,4 ms por celda
    // DP-03: sin este limite, un bus I2C caido cuelga el lazo de control
    // indefinidamente. Con el, la transferencia aborta y el firmware sigue.
    Wire.setWireTimeout(3000 /* us */, true /* reset_with_timeout */);

    // Etiquetas fijas de las tres filas de valvulas.
    for (uint8_t r = 0; r + 1 < kRows; ++r) {
        char label[config::kLcdLabelWidth];
        rowLabel(r, label);
        for (uint8_t c = 0; c < config::kLcdLabelWidth; ++c) {
            cells_[r][c] = label[c];
            markDirty(r, c);
        }
        // Separadores entre simbolos.
        for (uint8_t v = 0; v < config::kValvesPerRow; ++v) {
            const uint8_t col = config::kLcdLabelWidth + v * 2;
            if (col + 1 < kCols) {
                cells_[r][col + 1] = ' ';
                markDirty(r, col + 1);
            }
        }
    }
}

void LcdDisplayAdapter::setValveSymbol(ValveIndex index, char symbol) {
    const uint8_t row = index / config::kValvesPerRow;
    const uint8_t col = config::kLcdLabelWidth +
                        (index % config::kValvesPerRow) * 2;
    if (row >= kRows || col >= kCols) return;

    // DP-01: solo se marca el buffer de sombra; no se toca el bus.
    if (cells_[row][col] == symbol) return;
    cells_[row][col] = symbol;
    markDirty(row, col);
}

void LcdDisplayAdapter::setStatusLine(const char* text) {
    const uint8_t row = kRows - 1;
    for (uint8_t c = 0; c < kCols; ++c) {
        const char ch = (text != nullptr && text[c] != '\0') ? text[c] : ' ';
        if (cells_[row][c] == ch) continue;
        cells_[row][c] = ch;
        markDirty(row, c);
        if (text == nullptr || text[c] == '\0') {
            // Rellena el resto con espacios sin volver a leer la cadena.
            for (uint8_t rest = c + 1; rest < kCols; ++rest) {
                if (cells_[row][rest] != ' ') {
                    cells_[row][rest] = ' ';
                    markDirty(row, rest);
                }
            }
            return;
        }
    }
}

void LcdDisplayAdapter::tick() {
    // DP-04: con el bus caido, intentar recuperacion periodica (~1 s). Si el bus
    // se restablece, la siguiente escritura exitosa restaura healthy_.
    if (!healthy_) {
        ++recoveryCounter_;
        if (recoveryCounter_ < 66) return;  // ~66 * 15 ms = ~1 s
        recoveryCounter_ = 0;
        consecutiveFailures_ = 0;
        healthy_ = true;
        // No retorna: permite escribir una celda como prueba.
    }

    // DP-02: como maximo kDisplayCellsPerTick celdas por vuelta.
    uint8_t written = 0;
    const uint16_t total = static_cast<uint16_t>(kRows) * kCols;

    for (uint16_t scanned = 0; scanned < total && written < config::kDisplayCellsPerTick;
         ++scanned) {
        const uint16_t pos = (cursor_ + scanned) % total;
        const uint8_t  row = static_cast<uint8_t>(pos / kCols);
        const uint8_t  col = static_cast<uint8_t>(pos % kCols);
        if (!isDirty(row, col)) continue;

        writeCell(row, col);
        clearDirty(row, col);
        ++written;
        cursor_ = static_cast<uint8_t>((pos + 1) % total);
    }
}

void LcdDisplayAdapter::writeCell(uint8_t row, uint8_t col) {
    lcd_.setCursor(col, row);
    lcd_.print(cells_[row][col]);

    // DP-03: un fallo del panel no se propaga al control. Wire deja el codigo
    // del ultimo error de transmision; se usa solo para decidir si vale la pena
    // seguir intentando.
    if (Wire.getWireTimeoutFlag()) {
        Wire.clearWireTimeoutFlag();
        if (consecutiveFailures_ < 0xFF) ++consecutiveFailures_;
        if (consecutiveFailures_ >= config::kLcdMaxConsecutiveFailures) {
            healthy_ = false;
        }
    } else {
        consecutiveFailures_ = 0;
    }
}

void LcdDisplayAdapter::markDirty(uint8_t row, uint8_t col) {
    const uint16_t bit = static_cast<uint16_t>(row) * kCols + col;
    dirty_[bit / 8] |= static_cast<uint8_t>(1u << (bit % 8));
}

bool LcdDisplayAdapter::isDirty(uint8_t row, uint8_t col) const {
    const uint16_t bit = static_cast<uint16_t>(row) * kCols + col;
    return (dirty_[bit / 8] & static_cast<uint8_t>(1u << (bit % 8))) != 0;
}

void LcdDisplayAdapter::clearDirty(uint8_t row, uint8_t col) {
    const uint16_t bit = static_cast<uint16_t>(row) * kCols + col;
    dirty_[bit / 8] &= static_cast<uint8_t>(~(1u << (bit % 8)));
}

}  // namespace hal
}  // namespace wiga
