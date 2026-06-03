#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QTimer>
#include <QLabel>
#include <QPushButton>
#include <QVector>
#include <QString>
#include <array>
#include <cstdint>
#include "cancore.h"

namespace socketspy::gui {

// ---------------------------------------------------------------------------
// ErrorDiagPanel — dedicated error-frame & bus-off watchdog.
//
// Runs socketspy::core::classify_error() on every received frame.  Any frame
// whose ErrorType != None is appended (timestamped) to a scrolling 1000-row
// table and bumps a per-type counter shown in the summary strip.  Seeing a
// BusOff/BusError flashes a red banner with a suggested-recovery hint.  A
// 500 ms timer drives the table refresh and banner decay.  Pause/Clear
// buttons let the user freeze and reset the view.
// ---------------------------------------------------------------------------

class ErrorDiagPanel : public QWidget {
    Q_OBJECT

public:
    explicit ErrorDiagPanel(QWidget* parent = nullptr);

public slots:
    void onFrameReceived(const socketspy::core::CanFrame& frame);

private slots:
    void refresh();
    void onPauseToggled(bool paused);
    void onClear();

private:
    void setupUi();
    void updateSummary();

    // Number of distinct ErrorType enum values (incl. None) — used to size
    // the per-type counter and label arrays.
    static constexpr int kErrorTypeCount = 8;
    static constexpr int kMaxRows        = 1000;

    // One captured error-frame event, buffered until the next refresh tick.
    struct ErrorEvent {
        qint64                   timestampUs;
        socketspy::core::ErrorType type;
        uint32_t                 id;
        uint8_t                  dlc;
        std::array<uint8_t, 64>  data;
    };

    // --- Widgets ---
    QTableWidget* m_table{nullptr};
    QLabel*       m_banner{nullptr};
    QLabel*       m_summary{nullptr};
    QPushButton*  m_pauseBtn{nullptr};
    QPushButton*  m_clearBtn{nullptr};
    QTimer*       m_timer{nullptr};

    // --- State ---
    QVector<ErrorEvent> m_pending;                          // since last refresh
    std::array<quint64, kErrorTypeCount> m_counts{};        // per-type totals
    quint64       m_totalErrors{0};
    bool          m_paused{false};

    // Banner decay: counts down refresh ticks while a critical fault is shown.
    int           m_bannerTicks{0};
};

} // namespace socketspy::gui
