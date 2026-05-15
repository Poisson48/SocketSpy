#pragma once
#include <QWidget>
#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QHash>
#include <memory>
#include <unordered_map>
#include <set>
#include <vector>
#include "cancore.h"
#include "filter_model.h"

// Temporarily suppress the Qt `signals` keyword macro so that
// socketspy::dbc::Message::signals (a data member) parses correctly.
#pragma push_macro("signals")
#undef signals
#include "dbc_types.h"
#pragma pop_macro("signals")

namespace socketspy::gui {

// ---------------------------------------------------------------------------
// All monitor-specific filter state (distinct from FrameFilter which is the
// global hardware/protocol filter applied upstream).
struct MonitorFilter {
    bool     changedOnly   = false;   // Track: hide IDs that never change;
                                      // Log:   skip duplicate data per ID
    int      dlc           = 0;       // 0 = any, 1-64 = exact match
    bool     useTimestamp  = false;   // enable min/max timestamp gate
    double   tsMin         = 0.0;     // seconds (µs / 1e6)
    double   tsMax         = 0.0;
};

// ---------------------------------------------------------------------------
// Non-modal dialog that exposes MonitorFilter controls.
// The dialog is created once and shown/hidden by the "Filters…" button.
class MonitorFilterDialog : public QDialog {
    Q_OBJECT
public:
    explicit MonitorFilterDialog(QWidget* parent = nullptr);

    // Read the current filter state.
    MonitorFilter filter() const;

    // Push a filter into the UI (for initialisation).
    void setFilter(const MonitorFilter& f);

signals:
    void filterChanged(const socketspy::gui::MonitorFilter& f);

private slots:
    void onAnyChanged();

private:
    QCheckBox*     m_changedOnly{nullptr};
    QSpinBox*      m_dlc{nullptr};
    QCheckBox*     m_useTs{nullptr};
    QDoubleSpinBox* m_tsMin{nullptr};
    QDoubleSpinBox* m_tsMax{nullptr};
};

// ---------------------------------------------------------------------------

class MonitorPanel : public QWidget {
    Q_OBJECT

public:
    explicit MonitorPanel(QWidget* parent = nullptr);
    ~MonitorPanel() override;

    static constexpr int kMaxRows = 2000;

    // Returns the current monitor-local filter state.
    MonitorFilter currentMonitorFilter() const { return m_monFilter; }

    // Pushes a filter into both the dialog UI and the active filter state.
    void applyMonitorFilter(const MonitorFilter& f);

public slots:
    void onFrameReceived(socketspy::core::CanFrame frame);
    void onDbcLoaded(socketspy::dbc::DbcDatabase db);
    void onFilterChanged(const socketspy::gui::FrameFilter& filter);
    void setAliases(const QHash<QString,QString>& aliases);
    void onExportCsv();   // also wired from File menu (Ctrl+Shift+E)

signals:
    void signalDoubleClicked(QString signalName, uint32_t msgId);
    void frameGraphRequested(uint32_t id);

private slots:
    void onClear();
    void onCellDoubleClicked(int row, int col);
    void onContextMenu(const QPoint& pos);
    void onSearchChanged(const QString& text);
    void onTrackModeToggled(bool checked);
    void onMonitorFilterChanged(const socketspy::gui::MonitorFilter& f);
    void onFiltersButtonClicked();

private:
    void setupUi();
    QString decodeFrame(const socketspy::core::CanFrame& f) const;
    void appendLogRow(const socketspy::core::CanFrame& frame);
    void updateTrackingRow(const socketspy::core::CanFrame& frame);
    void applyRowVisibility(int row, uint32_t id);
    bool passesMonitorFilter(const socketspy::core::CanFrame& frame) const;

    QTableWidget*        m_table{nullptr};
    QPushButton*         m_clear{nullptr};
    QCheckBox*           m_pause{nullptr};
    QCheckBox*           m_trackMode{nullptr};
    QPushButton*         m_filterBtn{nullptr};
    QPushButton*         m_exportCsvBtn{nullptr};
    QCheckBox*           m_autoScrollChk{nullptr};
    QLineEdit*           m_search{nullptr};
    MonitorFilterDialog* m_filterDlg{nullptr};

    std::unique_ptr<socketspy::dbc::DbcDatabase> m_dbc;
    bool m_dbcLoaded{false};
    QHash<QString, QString> m_aliases;

    socketspy::gui::FrameFilter  m_filter;
    MonitorFilter                m_monFilter;   // persisted filter state

    // Tracking mode: one row per CAN ID
    struct TrackEntry {
        int row{-1};
        std::vector<uint8_t> lastData;
        int lastDlc{0};
        bool everChanged{false};
        // Delta / rate tracking
        uint64_t firstTs{0};      // µs of first frame for this ID
        uint64_t prevTs{0};       // µs of the frame before the latest
        uint64_t lastTs{0};       // µs of last frame
        uint64_t frameCount{0};   // total frames seen for this ID
        // Per-byte min/max (up to 8 bytes for display)
        uint8_t  byteMin[8]{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
        uint8_t  byteMax[8]{};
    };
    std::unordered_map<uint32_t, TrackEntry> m_tracked;
    std::set<uint32_t> m_pinned;

    // Log mode: last seen data per ID (for changed-only deduplication)
    std::unordered_map<uint32_t, std::pair<std::vector<uint8_t>, int>> m_logLastSeen;
};

} // namespace socketspy::gui
