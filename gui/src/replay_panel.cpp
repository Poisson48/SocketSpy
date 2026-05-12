#include "replay_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QElapsedTimer>
#include <QFont>

namespace socketspy::gui {

// ---------------------------------------------------------------------------
// ReplayWorker
// ---------------------------------------------------------------------------

ReplayWorker::ReplayWorker(QObject* parent) : QThread(parent) {}

ReplayWorker::~ReplayWorker() {
    stop();
    wait();
}

void ReplayWorker::load(const QVector<QPair<double, socketspy::core::CanFrame>>& frames,
                        const QVector<QString>& ifaces) {
    m_frames = frames;
    m_ifaces = ifaces;
}

void ReplayWorker::setSpeed(double multiplier) {
    m_speed = multiplier;
}

void ReplayWorker::play() {
    m_state.storeRelaxed(1);
    m_cond.wakeAll();
}

void ReplayWorker::pause() {
    m_state.storeRelaxed(2);
}

void ReplayWorker::stop() {
    m_state.storeRelaxed(0);
    m_cond.wakeAll();
}

void ReplayWorker::run() {
    if (m_frames.isEmpty()) {
        emit finished();
        return;
    }

    double startTs = m_frames.first().first;
    double endTs   = m_frames.last().first;
    double duration = endTs - startTs;

    QElapsedTimer wallTimer;
    wallTimer.start();

    for (int i = 0; i < m_frames.size(); ++i) {
        int state = m_state.loadRelaxed();
        if (state == 0) break;

        if (state == 2) {
            QMutexLocker lk(&m_mutex);
            while (m_state.loadRelaxed() == 2)
                m_cond.wait(&m_mutex);
            if (m_state.loadRelaxed() == 0) break;
        }

        double relTs = m_frames[i].first - startTs;
        double wallExpected = relTs / m_speed;
        double wallElapsed  = wallTimer.elapsed() / 1000.0;
        double sleepSec     = wallExpected - wallElapsed;

        if (sleepSec > 0.0) {
            auto sleepUs = static_cast<unsigned long>(sleepSec * 1'000'000.0);
            while (sleepUs > 50'000u) {
                QThread::usleep(50'000u);
                sleepUs -= 50'000u;
                if (m_state.loadRelaxed() != 1) goto done;
            }
            if (sleepUs > 0) QThread::usleep(sleepUs);
        }

        if (m_state.loadRelaxed() != 1) break;

        emit frameReady(m_frames[i].second, m_ifaces.value(i));

        int permille = (duration > 0.0)
            ? static_cast<int>(relTs / duration * 1000.0)
            : 0;
        emit progressChanged(qBound(0, permille, 1000), relTs);
    }

done:
    emit finished();
}

// ---------------------------------------------------------------------------
// ReplayPanel
// ---------------------------------------------------------------------------

ReplayPanel::ReplayPanel(QWidget* parent) : QWidget(parent) {
    setupUi();
}

ReplayPanel::~ReplayPanel() {
    if (m_worker) {
        m_worker->stop();
        m_worker->wait();
    }
}

void ReplayPanel::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(8);
    root->setContentsMargins(12, 12, 12, 12);

    // Top bar
    auto* topBar = new QHBoxLayout;
    m_openBtn   = new QPushButton("Open Log", this);
    m_fileLabel = new QLabel("No file loaded", this);
    m_fileLabel->setStyleSheet("color:#8892a4;");
    m_playBtn   = new QPushButton("Play", this);
    m_pauseBtn  = new QPushButton("Pause", this);
    m_stopBtn   = new QPushButton("Stop", this);

    m_speedCombo = new QComboBox(this);
    m_speedCombo->addItem("0.25x", 0.25);
    m_speedCombo->addItem("0.5x",  0.5);
    m_speedCombo->addItem("1x",    1.0);
    m_speedCombo->addItem("2x",    2.0);
    m_speedCombo->addItem("5x",    5.0);
    m_speedCombo->addItem("10x",   10.0);
    m_speedCombo->setCurrentIndex(2);

    topBar->addWidget(m_openBtn);
    topBar->addWidget(m_fileLabel, 1);
    topBar->addWidget(m_playBtn);
    topBar->addWidget(m_pauseBtn);
    topBar->addWidget(m_stopBtn);
    topBar->addWidget(new QLabel("Speed:", this));
    topBar->addWidget(m_speedCombo);

    // Progress row
    auto* progBar = new QHBoxLayout;
    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setRange(0, 1000);
    m_slider->setValue(0);
    m_timeLabel = new QLabel("00:00.000 / 00:00.000", this);
    m_timeLabel->setStyleSheet("color:#8892a4; font-family: monospace;");
    progBar->addWidget(m_slider, 1);
    progBar->addWidget(m_timeLabel);

    // Log view
    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(200);
    QFont mono("Monospace");
    mono.setStyleHint(QFont::TypeWriter);
    mono.setPointSize(9);
    m_log->setFont(mono);
    m_log->setStyleSheet(
        "QPlainTextEdit {"
        "  background:#0f1623;"
        "  color:#c8d3e6;"
        "  border:1px solid #1e2d45;"
        "}");

    root->addLayout(topBar);
    root->addLayout(progBar);
    root->addWidget(m_log, 1);

    m_playBtn->setEnabled(false);
    m_pauseBtn->setEnabled(false);
    m_stopBtn->setEnabled(false);

    connect(m_openBtn,    &QPushButton::clicked,              this, &ReplayPanel::onOpenLog);
    connect(m_playBtn,    &QPushButton::clicked,              this, &ReplayPanel::onPlay);
    connect(m_pauseBtn,   &QPushButton::clicked,              this, &ReplayPanel::onPause);
    connect(m_stopBtn,    &QPushButton::clicked,              this, &ReplayPanel::onStop);
    connect(m_speedCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &ReplayPanel::onSpeedChanged);
}

QString ReplayPanel::formatTime(double secs) const {
    int ms    = static_cast<int>(secs * 1000.0) % 1000;
    int s     = static_cast<int>(secs) % 60;
    int m     = static_cast<int>(secs) / 60;
    return QString("%1:%2.%3")
        .arg(m,  2, 10, QChar('0'))
        .arg(s,  2, 10, QChar('0'))
        .arg(ms, 3, 10, QChar('0'));
}

void ReplayPanel::loadLogFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    m_frames.clear();
    m_ifaces.clear();

    QTextStream in(&f);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;

        // Format: (timestamp) iface ID#DATA
        if (!line.startsWith('(')) continue;
        int rp = line.indexOf(')');
        if (rp < 0) continue;
        double ts = line.mid(1, rp - 1).toDouble();

        QString rest = line.mid(rp + 2).trimmed();
        int sp = rest.indexOf(' ');
        if (sp < 0) continue;
        QString iface = rest.left(sp);
        QString idData = rest.mid(sp + 1).trimmed();

        int hash = idData.indexOf('#');
        if (hash < 0) continue;
        QString idStr   = idData.left(hash);
        QString dataStr = idData.mid(hash + 1);

        bool ok;
        uint32_t id = idStr.toUInt(&ok, 16);
        if (!ok) continue;

        socketspy::core::CanFrame fr{};
        fr.id = id;
        fr.timestamp_us = static_cast<uint64_t>(ts * 1'000'000.0);

        int nbytes = qMin(dataStr.length() / 2, 64);
        for (int i = 0; i < nbytes; ++i) {
            fr.data[i] = static_cast<uint8_t>(dataStr.mid(i * 2, 2).toUInt(nullptr, 16));
        }
        fr.dlc = static_cast<uint8_t>(nbytes);

        m_frames.append({ts, fr});
        m_ifaces.append(iface);
    }

    if (m_frames.isEmpty()) return;

    m_duration = m_frames.last().first - m_frames.first().first;
    m_slider->setValue(0);
    m_timeLabel->setText("00:00.000 / " + formatTime(m_duration));
    m_playBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_pauseBtn->setEnabled(false);
}

void ReplayPanel::onOpenLog() {
    QString path = QFileDialog::getOpenFileName(
        this, "Open CAN Log", {},
        "CAN Log Files (*.log *.asc);;All Files (*)");
    if (path.isEmpty()) return;

    onStop();
    m_fileLabel->setText(path.section('/', -1));
    loadLogFile(path);
    m_log->clear();
}

void ReplayPanel::onPlay() {
    if (m_frames.isEmpty()) return;

    if (m_worker && m_worker->isRunning()) {
        m_worker->play();
        m_playBtn->setEnabled(false);
        m_pauseBtn->setEnabled(true);
        return;
    }

    if (m_worker) {
        m_worker->stop();
        m_worker->wait();
        delete m_worker;
    }

    m_worker = new ReplayWorker(this);
    m_worker->load(m_frames, m_ifaces);
    m_worker->setSpeed(m_speedCombo->currentData().toDouble());

    connect(m_worker, &ReplayWorker::frameReady,
            this,     &ReplayPanel::onFrameReplayed,
            Qt::QueuedConnection);
    connect(m_worker, &ReplayWorker::progressChanged,
            this,     &ReplayPanel::onWorkerProgress,
            Qt::QueuedConnection);
    connect(m_worker, &ReplayWorker::finished,
            this,     &ReplayPanel::onWorkerFinished,
            Qt::QueuedConnection);

    m_worker->play();
    m_worker->start();

    m_playBtn->setEnabled(false);
    m_pauseBtn->setEnabled(true);
    m_stopBtn->setEnabled(true);
}

void ReplayPanel::onPause() {
    if (m_worker) m_worker->pause();
    m_playBtn->setEnabled(true);
    m_pauseBtn->setEnabled(false);
}

void ReplayPanel::onStop() {
    if (m_worker) {
        m_worker->stop();
        m_worker->wait();
        delete m_worker;
        m_worker = nullptr;
    }
    m_slider->setValue(0);
    m_timeLabel->setText("00:00.000 / " + formatTime(m_duration));
    m_playBtn->setEnabled(!m_frames.isEmpty());
    m_pauseBtn->setEnabled(false);
    m_stopBtn->setEnabled(false);
}

void ReplayPanel::onSpeedChanged(int /*index*/) {
    if (m_worker) m_worker->setSpeed(m_speedCombo->currentData().toDouble());
}

void ReplayPanel::onWorkerProgress(int permille, double currentSec) {
    m_slider->setValue(permille);
    m_timeLabel->setText(formatTime(currentSec) + " / " + formatTime(m_duration));
}

void ReplayPanel::onWorkerFinished() {
    m_playBtn->setEnabled(!m_frames.isEmpty());
    m_pauseBtn->setEnabled(false);
    m_stopBtn->setEnabled(false);
    m_slider->setValue(0);
    m_timeLabel->setText("00:00.000 / " + formatTime(m_duration));
}

void ReplayPanel::onFrameReplayed(socketspy::core::CanFrame frame, QString iface) {
    bool extended = (frame.id > 0x7FFu);
    QString idStr = extended
        ? QString("%1").arg(frame.id, 8, 16, QChar('0')).toUpper()
        : QString("%1").arg(frame.id, 3, 16, QChar('0')).toUpper();

    QString dataStr;
    for (int i = 0; i < frame.dlc; ++i)
        dataStr += QString("%1").arg(frame.data[i], 2, 16, QChar('0')).toUpper();

    double ts = static_cast<double>(frame.timestamp_us) / 1'000'000.0;
    m_log->appendPlainText(
        QString("(%1) %2 %3#%4")
            .arg(ts, 0, 'f', 6)
            .arg(iface)
            .arg(idStr)
            .arg(dataStr));

    emit replayFrame(frame);
}

} // namespace socketspy::gui
