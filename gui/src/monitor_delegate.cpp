#include "monitor_delegate.h"
#include <QPainter>
#include <QStyleOptionViewItem>
#include <QModelIndex>
#include <QColor>
#include <QRect>
#include <QVariant>
#include <QApplication>
#include <algorithm>
#include <cmath>
#include <set>

namespace socketspy::gui {

// -----------------------------------------------------------------------
// Pattern-coloring helpers

// Classify a single byte from its ByteHistory record.
// Returns a QColor (invalid = use default bg).
static QColor byteColor(const ByteHistory& bh, int b) {
    if (!bh.first_set[b])
        return {};

    // "never changed": only one distinct value ever seen
    // We check count[b] frames stored; if all equal first_val -> grey
    const int n = bh.count[b];
    if (n == 0)
        return QColor(180, 180, 180); // grey — never received a frame yet

    bool neverChanged = true;
    for (int i = 0; i < n; ++i) {
        if (bh.last_vals[b][i] != bh.first_val[b]) { neverChanged = false; break; }
    }
    if (neverChanged)
        return QColor(200, 200, 200); // grey bg

    // "changes frequently": >3 changes in the last 2 seconds
    if (bh.change_cnt[b] > 3)
        return QColor(255, 100, 100); // red bg

    // "oscillates between exactly 2 values"
    // Look at the last 10 values and count distinct values
    std::set<uint8_t> distinct;
    for (int i = 0; i < n; ++i)
        distinct.insert(bh.last_vals[b][i]);
    if (distinct.size() == 2)
        return QColor(255, 165, 60); // orange bg

    // "stable": same value for last 5 frames
    if (n >= 5) {
        uint8_t ref = bh.last_vals[b][(bh.head[b] + 10 - 1) % 10];
        bool stable = true;
        for (int k = 1; k < 5; ++k) {
            int idx = (bh.head[b] + 10 - 1 - k) % 10;
            if (bh.last_vals[b][idx] != ref) { stable = false; break; }
        }
        if (stable)
            return QColor(100, 220, 100); // green bg
    }

    return {}; // default
}

// Parse hex string "AA BB CC …" into byte values.
// Returns empty vector on failure.
static std::vector<uint8_t> parseHex(const QString& txt) {
    std::vector<uint8_t> out;
    const QStringList tokens = txt.split(' ', Qt::SkipEmptyParts);
    out.reserve(tokens.size());
    for (const auto& t : tokens) {
        bool ok = false;
        uint v = t.toUInt(&ok, 16);
        if (!ok) return {};
        out.push_back(static_cast<uint8_t>(v));
    }
    return out;
}

// -----------------------------------------------------------------------
MonitorDelegate::MonitorDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

void MonitorDelegate::paint(QPainter* painter,
                            const QStyleOptionViewItem& option,
                            const QModelIndex& index) const
{
    // Let the base class handle selection highlight, focus rect, etc.
    // We override only the background fill and text rendering.
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    painter->save();

    // -----------------------------------------------------------------------
    // Try to retrieve ByteHistory for pattern coloring (Data column).
    // The item stores a pointer-as-quintptr in kByteHistoryRole.
    QVariant bhVar = index.data(kByteHistoryRole);
    if (!bhVar.isNull()) {
        // Data column with byte-history attached
        const ByteHistory* bh = reinterpret_cast<const ByteHistory*>(
            bhVar.value<quintptr>());

        // Parse the hex text so we know how many bytes to draw
        const QString txt = opt.text;
        const std::vector<uint8_t> bytes = parseHex(txt);
        const int n = static_cast<int>(bytes.size());

        if (bh && n > 0) {
            // Fill row background first (selection / alt row)
            const QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
            opt.text.clear(); // suppress default text draw
            style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

            // Draw each byte as a colored sub-rectangle with its hex text
            const QRect r = option.rect;
            const int cellW = r.width() / n;

            for (int b = 0; b < n; ++b) {
                QRect bRect(r.left() + b * cellW, r.top(),
                            (b == n - 1) ? r.right() - r.left() - b * cellW : cellW,
                            r.height());

                QColor bg = byteColor(*bh, b);
                if (bg.isValid())
                    painter->fillRect(bRect, bg);

                // Draw hex text
                QString byteStr = QString("%1").arg(bytes[b], 2, 16, QChar('0')).toUpper();
                painter->setPen(option.palette.text().color());
                painter->drawText(bRect, Qt::AlignCenter, byteStr);
            }

            painter->restore();
            return;
        }
    }

    // -----------------------------------------------------------------------
    // Try to retrieve SparkData for sparkline overlay (Decoded/signal column).
    QVariant sparkVar = index.data(kSparkDataRole);
    if (!sparkVar.isNull()) {
        const SparkData* sd = reinterpret_cast<const SparkData*>(
            sparkVar.value<quintptr>());

        if (sd && sd->vals.size() >= 2) {
            // Draw normal item first (text + selection bg)
            QStyledItemDelegate::paint(painter, option, index);

            // Overlay sparkline in bottom-right corner of the cell
            const QRect r = option.rect;
            const int sparkW = std::min(60, r.width() - 4);
            const int sparkH = std::min(18, r.height() - 4);
            if (sparkW > 4 && sparkH > 4) {
                QRect sparkRect(r.right() - sparkW - 2,
                                r.top() + (r.height() - sparkH) / 2,
                                sparkW, sparkH);

                // Semi-transparent white background
                painter->fillRect(sparkRect, QColor(255, 255, 255, 180));

                // Normalize and draw polyline
                const auto& vals = sd->vals;
                double vMin = *std::min_element(vals.begin(), vals.end());
                double vMax = *std::max_element(vals.begin(), vals.end());
                double vRange = (vMax > vMin) ? (vMax - vMin) : 1.0;

                const int sz = static_cast<int>(vals.size());
                QPolygonF poly;
                poly.reserve(sz);
                for (int i = 0; i < sz; ++i) {
                    double xf = sparkRect.left() + (i * (sparkW - 1.0)) / (sz - 1);
                    double yf = sparkRect.bottom() -
                                ((vals[i] - vMin) / vRange) * (sparkH - 1);
                    poly << QPointF(xf, yf);
                }

                painter->setPen(QPen(QColor(30, 120, 220), 1.2));
                painter->setRenderHint(QPainter::Antialiasing, true);
                painter->drawPolyline(poly);
            }

            painter->restore();
            return;
        }
    }

    // -----------------------------------------------------------------------
    // Default rendering
    QStyledItemDelegate::paint(painter, option, index);
    painter->restore();
}

QSize MonitorDelegate::sizeHint(const QStyleOptionViewItem& option,
                                const QModelIndex& index) const
{
    QSize s = QStyledItemDelegate::sizeHint(option, index);
    // Keep at least 26px height for the sparkline overlay
    if (s.height() < 26) s.setHeight(26);
    return s;
}

} // namespace socketspy::gui
