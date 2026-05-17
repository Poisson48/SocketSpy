#pragma once
#include <QStyledItemDelegate>
#include <QHash>
#include <QElapsedTimer>
#include <cstdint>
#include <deque>

namespace socketspy::gui {

// -----------------------------------------------------------------------
// Per-byte history for pattern coloring in Track mode.
// Up to 8 bytes, 10 last values each.
struct ByteHistory {
    uint8_t last_vals[8][10]{};   // circular buffer of last values
    int     count[8]{};           // how many values stored (0-10)
    int     head[8]{};            // index of next write
    qint64  change_ts[8]{};       // epoch-ms of last change (for 2-s window)
    int     change_cnt[8]{};      // changes in the last 2 seconds
    // sentinel: first value seen (for "never changed" detection)
    uint8_t first_val[8]{};
    bool    first_set[8]{};
};

// -----------------------------------------------------------------------
// Sparkline history: 30 last signal values keyed by "msgId:sigName".
static constexpr int kSparkLen = 30;
struct SparkData {
    std::deque<double> vals; // up to kSparkLen values
};

// -----------------------------------------------------------------------
// Custom role exposed to the delegate via item user data.
static constexpr int kByteHistoryRole  = Qt::UserRole + 20;
static constexpr int kSparkDataRole    = Qt::UserRole + 21;

// -----------------------------------------------------------------------
class MonitorDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit MonitorDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;
};

} // namespace socketspy::gui
