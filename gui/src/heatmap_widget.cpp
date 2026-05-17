#include "heatmap_widget.h"
#include <QPainter>
#include <QPaintEvent>
#include <QDateTime>
#include <cmath>

namespace socketspy::gui {

HeatmapWidget::HeatmapWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(sizeHint());
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

void HeatmapWidget::setBits(const BitState bits[64]) {
    std::copy(bits, bits + 64, m_bits);
    update();
}

QSize HeatmapWidget::sizeHint() const {
    return { kPad + kLabelW + 8 * kCellPx + kPad,
             kPad + kLabelH + 8 * kCellPx + kPad };
}

QSize HeatmapWidget::minimumSizeHint() const { return sizeHint(); }

// ---------------------------------------------------------------------------
// Colour logic
// ---------------------------------------------------------------------------

QColor HeatmapWidget::colourForBit(int idx) const {
    const BitState& b = m_bits[idx];

    // Never seen
    if (b.last_toggle_ms == 0 && b.toggle_count == 0)
        return QColor(160, 160, 160);  // grey

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const qint64 agems = nowMs - b.last_toggle_ms;

    // Compute toggle rate: approximate from last 1-second window.
    // toggle_count is cumulative so we use a simple proxy: if the last
    // toggle was < 100 ms ago the bit is considered "active right now".
    // A rough rate estimate: we don't store a rate, so we use a sliding
    // heuristic: if agems < 1000 ms, rate ≈ toggle_count ÷ (elapsed_s+ε).
    // We cap elapsed at 10 s so an old, high-count bit fades gracefully.
    const double elapsed_s = std::max(static_cast<double>(agems) / 1000.0, 0.1);
    const double rate      = static_cast<double>(b.toggle_count) / elapsed_s;

    QColor vivid;
    if (rate > 10.0) {
        vivid = QColor(220, 50, 50);   // red  — frequent toggle
    } else if (rate > 2.0) {
        vivid = QColor(230, 130, 30);  // orange — moderate toggle
    } else {
        // stable: colour by last value
        vivid = (b.last_val == 0)
            ? QColor(80, 200, 100)     // green  — stable 0
            : QColor(60, 130, 220);    // blue   — stable 1
    }

    // Fade toward grey after 3 s of inactivity
    constexpr qint64 kFadeStart = 3000;   // ms
    constexpr qint64 kFadeEnd   = 8000;   // ms — fully grey
    if (agems <= kFadeStart)
        return vivid;

    const double t = std::clamp(
        static_cast<double>(agems - kFadeStart) / static_cast<double>(kFadeEnd - kFadeStart),
        0.0, 1.0);
    const QColor grey(160, 160, 160);
    return QColor(
        static_cast<int>(vivid.red()   * (1.0 - t) + grey.red()   * t),
        static_cast<int>(vivid.green() * (1.0 - t) + grey.green() * t),
        static_cast<int>(vivid.blue()  * (1.0 - t) + grey.blue()  * t));
}

// ---------------------------------------------------------------------------
// Paint
// ---------------------------------------------------------------------------

void HeatmapWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    const int originX = kPad + kLabelW;
    const int originY = kPad + kLabelH;

    // Background
    p.fillRect(rect(), palette().window());

    const QFont labelFont = p.font();
    p.setFont(labelFont);

    // --- Byte labels (B0…B7) across the top ---
    p.setPen(palette().text().color());
    for (int byte = 0; byte < 8; ++byte) {
        const int x = originX + byte * kCellPx;
        p.drawText(QRect(x, kPad, kCellPx, kLabelH - 2),
                   Qt::AlignCenter,
                   QString("B%1").arg(byte));
    }

    // --- Bit labels (7…0) down the left ---
    for (int bit = 7; bit >= 0; --bit) {
        const int row = 7 - bit;   // row 0 = bit 7 (MSB)
        const int y   = originY + row * kCellPx;
        p.drawText(QRect(kPad, y, kLabelW - 2, kCellPx),
                   Qt::AlignVCenter | Qt::AlignRight,
                   QString::number(bit));
    }

    // --- Cells ---
    for (int byte = 0; byte < 8; ++byte) {
        for (int bitRow = 0; bitRow < 8; ++bitRow) {
            // bitRow 0 → bit 7 (MSB), bitRow 7 → bit 0 (LSB)
            const int bitNum = 7 - bitRow;
            const int idx    = byte * 8 + bitNum;

            const QColor fill = colourForBit(idx);
            const int x = originX + byte   * kCellPx;
            const int y = originY + bitRow * kCellPx;
            const QRect cell(x + 1, y + 1, kCellPx - 2, kCellPx - 2);

            p.fillRect(cell, fill);

            // Draw toggle count as tiny text inside the cell if > 0
            if (m_bits[idx].toggle_count > 0) {
                p.setPen(Qt::white);
                QFont tiny = labelFont;
                tiny.setPixelSize(9);
                p.setFont(tiny);
                p.drawText(cell, Qt::AlignCenter,
                           QString::number(m_bits[idx].toggle_count));
                p.setFont(labelFont);
            }

            // Grid border
            p.setPen(QColor(80, 80, 80, 60));
            p.drawRect(QRect(x, y, kCellPx, kCellPx));
        }
    }

    // Restore default pen
    p.setPen(palette().text().color());
}

} // namespace socketspy::gui
