#pragma once
#include <QWidget>
#include <cstdint>

namespace socketspy::gui {

// ── BitState ─────────────────────────────────────────────────────────────────
// Per-bit rendering state passed to HeatmapWidget for painting.
struct BitState {
    uint32_t toggle_count{0};
    qint64   last_toggle_ms{0};  // 0 = never toggled
    qint64   first_seen_ms{0};   // timestamp of first observation
    uint8_t  last_val{0};        // last known bit value
};

// ── HeatmapWidget ────────────────────────────────────────────────────────────
// Paints a 8×8 grid (8 bytes × 8 bits) coloured by toggle activity.
// Colour rules:
//   - Never seen (toggle_count==0, last_toggle_ms==0): grey
//   - Stable 0 (few toggles): light green
//   - Stable 1 (few toggles): light blue
//   - Moderate toggles: orange
//   - Frequent toggles (>10/s): red
//   - Fade toward grey when > 3 s since last toggle
// Grid: byte 0-7 left→right (columns), bit 7-0 top→bottom (rows).
// Labels: "B0"…"B7" above columns, "7"…"0" left of rows.
class HeatmapWidget : public QWidget {
    Q_OBJECT
public:
    explicit HeatmapWidget(QWidget* parent = nullptr);

    // Update the full 64-bit state (bits[byte*8 + bit], bit 7=MSB).
    void setBits(const BitState bits[64]);

    QSize sizeHint()    const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    BitState m_bits[64]{};

    // Returns the interpolated display colour for one bit.
    QColor colourForBit(int idx) const;

    static constexpr int kCellPx   = 34;   // cell width and height
    static constexpr int kLabelW   = 24;   // left label area (bit numbers)
    static constexpr int kLabelH   = 22;   // top label area (byte numbers)
    static constexpr int kPad      = 6;    // outer padding
};

} // namespace socketspy::gui
