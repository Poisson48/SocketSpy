#pragma once
#include <QWidget>
#include <QLabel>
#include <QTableWidget>
#include <QPushButton>
#include <QTimer>
#include <QSet>
#include <QVector>
#include <cstdint>
#include "cancore.h"

namespace socketspy::gui {

// Tiny self-contained sparkline widget — draws a rolling history of values as a
// filled polyline. No external header / dependency, only Qt Widgets.
class BusLoadSparkline : public QWidget {
    Q_OBJECT
public:
    explicit BusLoadSparkline(QWidget* parent = nullptr);

    void push(double value);   // append a sample (auto-scales to max seen)
    void clear();              // reset history

protected:
    void paintEvent(QPaintEvent* ev) override;

private:
    QVector<double> m_history;
    int             m_maxPoints = 120;
};

// Real-time live-metrics dashboard. Consumes every CanFrame via
// onFrameReceived() and, on a 250 ms timer, recomputes the headline metrics:
//   - frames-per-second (and peak fps)
//   - bus-utilisation percentage (estimated from frame bit count / bitrate)
//   - unique-ID count
//   - error-frame count bucketed per ErrorType via classify_error()
class BusLoadDashboardPanel : public QWidget {
    Q_OBJECT

public:
    explicit BusLoadDashboardPanel(QWidget* parent = nullptr);

public slots:
    void onFrameReceived(const socketspy::core::CanFrame& frame);
    void setBitrate(int bitrate);   // call sites optional; defaults to 500000

private slots:
    void onTick();
    void onClear();

private:
    void setupUi();

    // Estimate the on-wire bit count of a frame for the current bitrate:
    //   standard:  ~ 47 + dlc*8 bits   (incl. SOF/arbitration/CRC/EOF/IFS)
    //   extended:  ~ 67 + dlc*8 bits
    static uint64_t estimateBits(const socketspy::core::CanFrame& frame);

    // --- Widgets ------------------------------------------------------------
    QLabel*           m_fpsValue{nullptr};
    QLabel*           m_peakFpsValue{nullptr};
    QLabel*           m_loadValue{nullptr};
    QLabel*           m_uniqueValue{nullptr};
    QLabel*           m_errorValue{nullptr};
    QLabel*           m_bitrateValue{nullptr};
    BusLoadSparkline* m_sparkline{nullptr};
    QTableWidget*     m_errorTable{nullptr};
    QPushButton*      m_clearBtn{nullptr};
    QTimer*           m_timer{nullptr};

    // --- State (mutated on the frame-receive path) --------------------------
    uint64_t           m_bitrate          = 500000;
    uint64_t           m_framesInWindow   = 0;   // frames since last tick
    uint64_t           m_bitsInWindow     = 0;   // estimated bits since last tick
    uint64_t           m_totalFrames      = 0;
    uint64_t           m_totalErrors      = 0;
    double             m_peakFps          = 0.0;
    QSet<uint32_t>     m_uniqueIds;

    // One bucket per ErrorType (index = static_cast<int>(ErrorType)).
    static constexpr int kNumErrorTypes = 8;
    uint64_t           m_errorCounts[kNumErrorTypes] = {};
};

} // namespace socketspy::gui
