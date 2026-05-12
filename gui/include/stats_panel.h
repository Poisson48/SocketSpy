#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QHash>
#include <QElapsedTimer>
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
};

class StatsPanel : public QWidget {
    Q_OBJECT

public:
    explicit StatsPanel(QWidget* parent = nullptr);

public slots:
    void onFrameReceived(socketspy::core::CanFrame frame);
    void onDbcLoaded(socketspy::dbc::DbcDatabase db);

private slots:
    void onTick();
    void onReset();

private:
    static constexpr uint64_t kBitRate = 500'000;

    void setupUi();
    static uint64_t bitsPerFrame(const socketspy::core::CanFrame& f);

    QLabel*        m_totalLabel{nullptr};
    QLabel*        m_loadLabel{nullptr};
    QLabel*        m_errorLabel{nullptr};
    QLabel*        m_uptimeLabel{nullptr};
    QPushButton*   m_resetBtn{nullptr};
    QTableWidget*  m_table{nullptr};
    QTimer*        m_timer{nullptr};

    QHash<uint32_t, PerIdStats> m_stats;
    uint64_t m_totalFrames  = 0;
    uint64_t m_totalErrors  = 0;
    uint64_t m_bitsInWindow = 0;
    double   m_busLoad      = 0.0;

    socketspy::dbc::DbcDatabase m_dbc;
    bool m_dbcLoaded = false;

    QElapsedTimer m_uptime;
};

} // namespace socketspy::gui
