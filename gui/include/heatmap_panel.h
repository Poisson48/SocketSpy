#pragma once
#include <QWidget>
#include <QHash>
#include <cstdint>
#include "cancore.h"

class QListWidget;
class QListWidgetItem;
class QTimer;

namespace socketspy::gui {

class HeatmapWidget;

// ── HeatmapState ─────────────────────────────────────────────────────────────
// Per-ID tracking: toggle counts, last toggle timestamp, last bit values.
struct HeatmapState {
    uint32_t toggle_count[64]{};
    qint64   last_toggle_ms[64]{};
    uint8_t  last_val[64]{};
    uint64_t frame_count{0};
};

// ── HeatmapPanel ─────────────────────────────────────────────────────────────
// Left: QListWidget listing seen CAN IDs.
// Right: HeatmapWidget showing bit activity for the selected ID.
// Updated at 30 fps via QTimer.
class HeatmapPanel : public QWidget {
    Q_OBJECT
public:
    explicit HeatmapPanel(QWidget* parent = nullptr);

public slots:
    void onFrameReceived(const socketspy::core::CanFrame& frame);

private slots:
    void onIdSelected(QListWidgetItem* item);
    void onRefreshTimer();

private:
    void setupUi();
    void updateIdList(uint32_t id);
    void pushBitsToWidget();

    QListWidget*   m_idList{nullptr};
    HeatmapWidget* m_heatmap{nullptr};
    QTimer*        m_timer{nullptr};

    QHash<uint32_t, HeatmapState> m_states;
    uint32_t m_selectedId{0xFFFFFFFF};  // sentinel: none selected
    bool     m_dirty{false};
};

} // namespace socketspy::gui
