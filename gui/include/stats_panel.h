#pragma once
#include <QWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QHash>
#include <QElapsedTimer>
#include <limits>
#include "cancore.h"

#pragma push_macro("signals")
#undef signals
#include "dbc_types.h"
#pragma pop_macro("signals")

namespace socketspy::gui {

struct PerIdStats {
    uint64_t count          = 0;
    uint64_t count_last_sec = 0;
    uint64_t bits_in_window = 0;
    uint8_t  last_dlc       = 0;
    uint8_t  last_data[8]   = {};
    // Gap B — per-ID byte-0 min/max/sum for a quick first-glance range
    uint8_t  b0_min = 0xFF;
    uint8_t  b0_max = 0x00;
    uint64_t b0_sum = 0;          // for mean
};

// Gap B — per-signal decoded stats (populated when DBC is loaded)
struct PerSignalStats {
    QString  msgName;
    QString  sigName;
    uint32_t msgId    = 0;
    uint64_t count    = 0;
    double   valMin   = std::numeric_limits<double>::max();
    double   valMax   = std::numeric_limits<double>::lowest();
    double   valSum   = 0.0;
    double   valLast  = 0.0;
    QString  unit;
};

class StatsPanel : public QWidget {
    Q_OBJECT

public:
    explicit StatsPanel(QWidget* parent = nullptr);

public slots:
    void onFrameReceived(socketspy::core::CanFrame frame);
    void onDbcLoaded(socketspy::dbc::DbcDatabase db);
    void setBitrate(int bitrate);

private slots:
    void onTick();
    void onReset();

private:
    uint64_t m_bitrate = 500'000;  // dynamic, updated via setBitrate()

    void setupUi();
    void refreshSignalTable();
    static uint64_t bitsPerFrame(const socketspy::core::CanFrame& f);

    QTabWidget*    m_tabs{nullptr};

    // Tab 0 — per-ID bus stats (existing)
    QLabel*        m_totalLabel{nullptr};
    QLabel*        m_loadLabel{nullptr};
    QLabel*        m_errorLabel{nullptr};
    QLabel*        m_uptimeLabel{nullptr};
    QPushButton*   m_resetBtn{nullptr};
    QTableWidget*  m_table{nullptr};

    // Tab 1 — per-signal decoded stats (Gap B)
    QTableWidget*  m_signalTable{nullptr};

    QTimer*        m_timer{nullptr};

    QHash<uint32_t, PerIdStats> m_stats;
    uint64_t m_totalFrames  = 0;
    uint64_t m_totalErrors  = 0;
    uint64_t m_bitsInWindow = 0;
    double   m_busLoad      = 0.0;

    socketspy::dbc::DbcDatabase m_dbc;
    bool m_dbcLoaded = false;

    // Signal name → stats (key = "MsgName::SigName")
    QHash<QString, PerSignalStats> m_sigStats;

    QElapsedTimer m_uptime;
};

} // namespace socketspy::gui
