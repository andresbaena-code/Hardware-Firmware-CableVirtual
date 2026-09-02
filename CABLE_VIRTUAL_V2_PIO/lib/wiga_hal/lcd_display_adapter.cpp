// Cubre: FR-019, FR-020, FR-022  (research.md R-007)
#include "lcd_display_adapter.h"

#include <Arduino.h>
#include <Wire.h>

#include "installation_map.h"
#include "time_math.h"

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

// -----------------------------------------------------------------------------
//  El divisor del TWI tiene que quedar en especificacion del AVR.
//
//      SCL = F_CPU / (16 + 2 * TWBR)   =>   TWBR = (F_CPU / SCL - 16) / 2
//
//  El datasheet exige TWBR >= 10 en modo maestro. Esta comprobacion habria
//  atrapado en compilacion la combinacion que este proyecto tuvo durante un
//  tiempo -400 kHz pedidos sobre un cristal de 8 MHz, que da TWBR = 2- y que
//  solo funcionaba porque F_CPU estaba mal declarado a 16 MHz.
//
//  Vive aqui y no en timing_config.h porque F_CPU solo existe al compilar para
//  la placa: la capa pura tambien se compila en las pruebas de escritorio.
// -----------------------------------------------------------------------------
static constexpr uint32_t kTwbr = (F_CPU / config::kI2cClockHz - 16) / 2;
static_assert(kTwbr >= 10,
              "C-14: TWBR por debajo del minimo del AVR en modo maestro. "
              "Bajar kI2cClockHz o revisar F_CPU.");
static_assert(kTwbr <= 255,
              "C-14: TWBR no cabe en el registro de 8 bits del TWI.");

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

    // Borrado explicito de la DDRAM. init() de la libreria ya deberia hacerlo,
    // pero es la unica orden que blanquea el panel de una sola vez y no cuesta
    // nada asegurarla: si el repintado de mas abajo fallara por cualquier
    // motivo, al menos no queda texto de la carga anterior en pantalla.
    lcd_.clear();
    Wire.setClock(config::kI2cClockHz);  // 100 kHz: maximo del PCF8574T
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

    // REPINTADO COMPLETO. Sin esto queda basura del firmware anterior en
    // pantalla: el buffer de sombra arranca creyendo que las 80 celdas contienen
    // espacios, de modo que las que el panel FISICO tiene escritas con otro
    // caracter nunca se reescriben. Coinciden con lo que la sombra espera, no se
    // marcan sucias, y el resto se queda ahi para siempre.
    //
    // Verificado en banco el 2026-08-27: tras cargar el firmware nuevo, la linea
    // de estado mostraba caracteres heredados de una carga anterior detras de
    // "PEDIDO - SIN BUS".
    //
    // Cuesta 80 vueltas de lazo, una celda por vuelta. Solo al arrancar.
    for (uint8_t r = 0; r < kRows; ++r) {
        for (uint8_t c = 0; c < kCols; ++c) {
            markDirty(r, c);
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
    const uint32_t now = millis();

    // DP-04: con el bus caido, reintentar de forma periodica. Se mide en TIEMPO
    // REAL y no en vueltas de lazo.
    //
    // DEFECTO CORREGIDO: la version anterior contaba iteraciones de loop() y
    // afirmaba en un comentario que eran ~1 s. Pero display.tick() se llama en
    // CADA vuelta del lazo, no una vez cada kLoopPeriodMs, de modo que la
    // cadencia real era unas quince veces mas rapida de la documentada.
    if (!healthy_) {
        if (!elapsed(now, lastRecoveryAtMs_, config::kLcdRecoveryIntervalMs)) {
            return;
        }
        lastRecoveryAtMs_    = now;
        consecutiveFailures_ = 0;
        healthy_             = true;
        // No retorna: permite escribir una celda como prueba.
    }

    // Sondeo de presencia. Un LCD desconectado en caliente responde NACK y no
    // expiracion, asi que el flag de timeout de Wire nunca se activaria y el
    // panel se daria por sano indefinidamente.
    if (elapsed(now, lastProbeAtMs_, config::kLcdProbeIntervalMs)) {
        lastProbeAtMs_ = now;
        if (!probe()) {
            noteFailure();
        } else {
            consecutiveFailures_ = 0;
        }
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

    // DP-03: un fallo del panel no se propaga al control.
    if (Wire.getWireTimeoutFlag()) {
        Wire.clearWireTimeoutFlag();
        noteFailure();
    }
}

/// Pregunta al panel si sigue ahi. endTransmission() devuelve 0 si el esclavo
/// I2C reconoce su direccion, y 2 si responde NACK, que es lo que ocurre cuando
/// el cable se suelta en caliente: el caso mas comun en un tablero de finca.
bool LcdDisplayAdapter::probe() {
    Wire.beginTransmission(config::kLcdI2cAddress);
    const uint8_t result = Wire.endTransmission();
    if (Wire.getWireTimeoutFlag()) {
        Wire.clearWireTimeoutFlag();
        return false;
    }
    return result == 0;
}

void LcdDisplayAdapter::noteFailure() {
    if (consecutiveFailures_ < 0xFF) ++consecutiveFailures_;
    if (consecutiveFailures_ >= config::kLcdMaxConsecutiveFailures) {
        healthy_ = false;
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
