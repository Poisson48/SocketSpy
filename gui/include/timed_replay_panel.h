#pragma once
#include <QWidget>
#include <QVector>
#include "cancore.h"

class QPushButton;
class QCheckBox;
class QDoubleSpinBox;
class QProgressBar;
class QLabel;
class QTimer;
class QElapsedTimer;

namespace socketspy::gui {

// TimedReplayPanel — replays a previously captured CAN trace while preserving
// the original inter-frame timing (instead of injecting every frame at full
// speed).  A capture (.log / .csv) is parsed into an in-memory vector of
// CanFrame, then a single-shot QTimer chain drives playback so that the
// wall-clock gap between consecutive emits matches the recorded
// timestamp_us deltas, scaled by a user-selectable speed multiplier.
//
// Each emitted frame is published via frameReady(); MainWindow wires this into
// the existing wireFrames() lambda so replayed frames reach the monitor /
// graph / stats panels.  This panel never touches hardware — it only re-emits
// into the application's in-process frame bus.
class TimedReplayPanel : public QWidget {
    Q_OBJECT

public:
    explicit TimedReplayPanel(QWidget* parent = nullptr);
    ~TimedReplayPanel() override;

signals:
    // Re-emitted replayed frame for MainWindow::wireFrames().
    void frameReady(const socketspy::core::CanFrame& frame);

private slots:
    void onOpen();
    void onPlay();
    void onPause();
    void onStop();
    void onSpeedChanged(double speed);
    void onTick();

private:
    void setupUi();
    void loadCapture(const QString& path);
    bool parseLine(const QString& line, socketspy::core::CanFrame& out) const;
    void scheduleNext();
    void resetPlayback();
    void updateButtons();
    void updateProgress();
    QString formatTime(double secs) const;

    enum class State { Stopped, Playing, Paused };

    // UI
    QPushButton*    m_openBtn{nullptr};
    QLabel*         m_fileLabel{nullptr};
    QPushButton*    m_playBtn{nullptr};
    QPushButton*    m_pauseBtn{nullptr};
    QPushButton*    m_stopBtn{nullptr};
    QDoubleSpinBox* m_speedSpin{nullptr};
    QCheckBox*      m_loopCheck{nullptr};
    QProgressBar*   m_progress{nullptr};
    QLabel*         m_statusLabel{nullptr};

    // Timing engine
    QTimer*         m_timer{nullptr};
    QElapsedTimer*  m_wallClock{nullptr};

    // Trace
    QVector<socketspy::core::CanFrame> m_frames;  // sorted by timestamp_us
    int     m_index{0};        // next frame to emit
    double  m_speed{1.0};      // playback multiplier (0.1x .. 10x)
    State   m_state{State::Stopped};
};

} // namespace socketspy::gui
