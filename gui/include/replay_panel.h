#pragma once
#include <QWidget>
#include <QThread>
#include <QPlainTextEdit>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QVector>
#include <QPair>
#include <QAtomicInt>
#include <QMutex>
#include <QWaitCondition>
#include "cancore.h"

namespace socketspy::gui {

class ReplayWorker : public QThread {
    Q_OBJECT
public:
    explicit ReplayWorker(QObject* parent = nullptr);
    ~ReplayWorker() override;

    void load(const QVector<QPair<double, socketspy::core::CanFrame>>& frames,
              const QVector<QString>& ifaces);
    void setSpeed(double multiplier);
    void play();
    void pause();
    void stop();

signals:
    void frameReady(socketspy::core::CanFrame frame, QString iface);
    void progressChanged(int permille, double currentSec);
    void finished();

protected:
    void run() override;

private:
    QVector<QPair<double, socketspy::core::CanFrame>> m_frames;
    QVector<QString>  m_ifaces;
    double            m_speed{1.0};
    QAtomicInt        m_state{0}; // 0=stop, 1=play, 2=pause
    QMutex            m_mutex;
    QWaitCondition    m_cond;
};

class ReplayPanel : public QWidget {
    Q_OBJECT
public:
    explicit ReplayPanel(QWidget* parent = nullptr);
    ~ReplayPanel() override;

public slots:
    void onFrameReplayed(socketspy::core::CanFrame frame, QString iface);

signals:
    void replayFrame(socketspy::core::CanFrame frame);

private slots:
    void onOpenLog();
    void onPlay();
    void onPause();
    void onStop();
    void onSpeedChanged(int index);
    void onWorkerProgress(int permille, double currentSec);
    void onWorkerFinished();

private:
    void setupUi();
    void loadLogFile(const QString& path);
    QString formatTime(double secs) const;

    QPushButton*   m_openBtn{nullptr};
    QLabel*        m_fileLabel{nullptr};
    QPushButton*   m_playBtn{nullptr};
    QPushButton*   m_pauseBtn{nullptr};
    QPushButton*   m_stopBtn{nullptr};
    QComboBox*     m_speedCombo{nullptr};
    QSlider*       m_slider{nullptr};
    QLabel*        m_timeLabel{nullptr};
    QPlainTextEdit* m_log{nullptr};

    ReplayWorker*  m_worker{nullptr};

    QVector<QPair<double, socketspy::core::CanFrame>> m_frames;
    QVector<QString> m_ifaces;
    double m_duration{0.0};
};

} // namespace socketspy::gui
