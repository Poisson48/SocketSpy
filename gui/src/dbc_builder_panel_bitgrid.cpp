// dbc_builder_panel_bitgrid.cpp — widget BitGridWidget (grille de bits 8×N
// éditable) du DbcBuilderPanel. Extrait de dbc_builder_panel.cpp.
#include "dbc_builder_panel.h"

#include <QPainter>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QSplitter>
#include <QGroupBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QStackedWidget>
#include <QMessageBox>
#include <QTimer>
#include <cstring>
#include <span>

// Permanently undef Qt's `signals` macro so we can access dbc::Message::signals.
// Safe in .cpp files — no `signals:` access specifier is used here.
#include "dbc_compat.h"
#include "gui_palette.h"

namespace socketspy::gui {

// ─── BitGridWidget ────────────────────────────────────────────────────────────

BitGridWidget::BitGridWidget(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(200, 180);
}

int BitGridWidget::cellPx() const {
    int availW = width()  - kLeftPad;
    int availH = height() - kTopPad;
    int szW = (availW - 7 * kGap) / 8;
    int szH = (availH - 7 * kGap) / 8;
    return std::max(18, std::min(szW, szH));
}

QSize BitGridWidget::sizeHint() const {
    return { kLeftPad + 8 * (22 + kGap), kTopPad + 8 * (22 + kGap) };
}

QSize BitGridWidget::minimumSizeHint() const { return { 200, 180 }; }

void BitGridWidget::setData(const uint8_t* data, int dlc) {
    m_dlc = dlc;
    std::memset(m_data, 0, sizeof(m_data));
    if (data && dlc > 0)
        std::memcpy(m_data, data, static_cast<size_t>(std::min(dlc, 8)));
    update();
}

void BitGridWidget::setSignals(const std::vector<socketspy::dbc::Signal>& sigs) {
    m_signals = sigs;
    update();
}

void BitGridWidget::clearSelection() {
    m_dragStart = m_dragEnd = -1;
    m_dragging = false;
    update();
}

void BitGridWidget::setSelection(int startBit, int length) {
    if (length <= 0) {
        m_dragStart = m_dragEnd = -1;
    } else {
        m_dragStart = startBit;
        m_dragEnd   = startBit + length - 1;
    }
    update();
}

// Convert row/col to Intel flat bit index: row*8 + (7-col)
// row = byte index (0..7), col = bit within byte (0=LSB right, 7=MSB left)
static int toFlatBit(int row, int col) { return row * 8 + (7 - col); }

QRect BitGridWidget::cellRect(int bitIdx) const {
    const int cp = cellPx();
    int row = bitIdx / 8;
    int col = 7 - (bitIdx % 8);
    int x = kLeftPad + col * (cp + kGap);
    int y = kTopPad  + row * (cp + kGap);
    return { x, y, cp, cp };
}

int BitGridWidget::bitAt(QPoint pos) const {
    const int cp = cellPx();
    int col = (pos.x() - kLeftPad) / (cp + kGap);
    int row = (pos.y() - kTopPad)  / (cp + kGap);
    if (col < 0 || col > 7 || row < 0 || row > 7) return -1;
    // Verify click is inside the cell (not in the gap)
    int cx = kLeftPad + col * (cp + kGap);
    int cy = kTopPad  + row * (cp + kGap);
    if (pos.x() < cx || pos.x() >= cx + cp) return -1;
    if (pos.y() < cy || pos.y() >= cy + cp) return -1;
    return toFlatBit(row, col);
}

// Build the set of flat bit indices belonging to a Motorola BE signal
static std::vector<int> motorolaBits(const socketspy::dbc::Signal& sig) {
    std::vector<int> bits;
    bits.reserve(sig.bit_length);
    uint32_t bit_pos = sig.start_bit;
    for (uint32_t i = 0; i < sig.bit_length; ++i) {
        bits.push_back(static_cast<int>(bit_pos));
        uint32_t byte_idx = bit_pos / 8u;
        uint32_t bit_idx  = bit_pos % 8u;
        if (bit_idx == 0) bit_pos = (byte_idx + 1u) * 8u + 7u;
        else --bit_pos;
    }
    return bits;
}

void BitGridWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    const int cp = cellPx();

    // Background
    p.fillRect(rect(), QColor("#1e1e2e"));

    // Build signal coverage map: flat bit index → signal color index
    std::unordered_map<int, int> bitToSig;
    for (int si = 0; si < static_cast<int>(m_signals.size()); ++si) {
        const auto& sig = m_signals[si];
        int colorIdx = si % Palette::kNumSigColors;
        if (sig.byte_order == socketspy::dbc::ByteOrder::LittleEndian) {
            for (uint32_t b = 0; b < sig.bit_length; ++b)
                bitToSig[static_cast<int>(sig.start_bit + b)] = colorIdx;
        } else {
            for (int b : motorolaBits(sig))
                bitToSig[b] = colorIdx;
        }
    }

    // Determine selection range
    int selMin = -1, selMax = -1;
    if (m_dragStart >= 0 && m_dragEnd >= 0) {
        selMin = std::min(m_dragStart, m_dragEnd);
        selMax = std::max(m_dragStart, m_dragEnd);
    }

    QFont font = p.font();
    font.setPixelSize(10);
    p.setFont(font);

    // Column headers (7..0)
    p.setPen(QColor("#9ca3af"));
    for (int col = 0; col < 8; ++col) {
        int x = kLeftPad + col * (cp + kGap);
        p.drawText(QRect(x, 0, cp, kTopPad - 2),
                   Qt::AlignHCenter | Qt::AlignVCenter,
                   QString::number(7 - col));
    }

    // Row labels (B0..B7) and cells
    for (int row = 0; row < 8; ++row) {
        int y = kTopPad + row * (cp + kGap);
        p.setPen(QColor("#9ca3af"));
        p.drawText(QRect(0, y, kLeftPad - 4, cp),
                   Qt::AlignRight | Qt::AlignVCenter,
                   QString("B%1").arg(row));

        for (int col = 0; col < 8; ++col) {
            int flatBit = toFlatBit(row, col);
            QRect r = cellRect(flatBit);

            // Determine cell fill color
            QColor fill;
            bool inSel = (selMin >= 0 && flatBit >= selMin && flatBit <= selMax);
            if (inSel) {
                fill = QColor(100, 120, 220, 180);
            } else if (bitToSig.count(flatBit)) {
                fill = Palette::kSigColors[bitToSig[flatBit]];
                fill.setAlpha(200);
            } else if (m_dlc > 0 && row < m_dlc) {
                int byteVal = m_data[row];
                int bitVal  = (byteVal >> (7 - col)) & 1;
                fill = bitVal ? QColor("#374151") : QColor("#111827");
            } else {
                fill = QColor("#1e2030");
            }

            p.fillRect(r, fill);

            // Bit value text
            if (m_dlc > 0 && row < m_dlc) {
                int bitVal = (m_data[row] >> (7 - col)) & 1;
                p.setPen(inSel ? Qt::white : (bitToSig.count(flatBit) ? Qt::white : QColor("#9ca3af")));
                p.drawText(r, Qt::AlignCenter, QString::number(bitVal));
            }
        }
    }
}

void BitGridWidget::mousePressEvent(QMouseEvent* ev) {
    if (ev->button() != Qt::LeftButton) return;
    int b = bitAt(ev->pos());
    if (b < 0) return;
    m_dragStart = m_dragEnd = b;
    m_dragging = true;
    update();
}

void BitGridWidget::mouseMoveEvent(QMouseEvent* ev) {
    if (!m_dragging) return;
    int b = bitAt(ev->pos());
    if (b < 0) return;
    m_dragEnd = b;
    update();
}

void BitGridWidget::mouseReleaseEvent(QMouseEvent* ev) {
    if (ev->button() != Qt::LeftButton || !m_dragging) return;
    m_dragging = false;
    int b = bitAt(ev->pos());
    if (b >= 0) m_dragEnd = b;
    emitSelection();
    update();
}

void BitGridWidget::resizeEvent(QResizeEvent* ev) {
    QWidget::resizeEvent(ev);
    update();
}

void BitGridWidget::emitSelection() {
    if (m_dragStart < 0 || m_dragEnd < 0) return;
    int startBit = std::min(m_dragStart, m_dragEnd);
    int length   = std::abs(m_dragEnd - m_dragStart) + 1;
    emit selectionChanged(startBit, length);
}

// ─── DbcBuilderPanel ─────────────────────────────────────────────────────────

} // namespace socketspy::gui
