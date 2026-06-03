#include "timed_replay_panel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QProgressBar>
#include <QLabel>
#include <QTimer>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QStringList>

#include <algorithm>

namespace socketspy::gui {

namespace {

// Convert a single hex character to its nibble value, or -1 if not hex.
inline int hexNibble(QChar c) {
    const ushort u = c.unicode();
    if (u >= '0' && u <= '9') return u - '0';
    if (u >= 'a' && u <= 'f') return u - 'a' + 10;
    if (u >= 'A' && u <= 'F') return u - 'A' + 10;
    return -1;
}

// Parse a run of hex digit pairs (any non-hex characters such as spaces, dots
// or colons act purely as separators) into a CanFrame data buffer.  Returns the
// number of bytes written (clamped to 64).
int parseHexBytes(QStringView hex, uint8_t* dst) {
    int n = 0;
    int pending = -1;  // high nibble awaiting its low nibble
    for (QChar c : hex) {
        const int v = hexNibble(c);
        if (v < 0) continue;  // separator — skip
        if (pending < 0) {
            pending = v;
        } else {
            if (n >= 64) break;
            dst[n++] = static_cast<uint8_t>((pending << 4) | v);
            pending = -1;
        }
    }
    return n;
}

} // namespace

TimedReplayPanel::TimedReplayPanel(QWidget* parent) : QWidget(parent) {
    m_wallClock = new QElapsedTimer;
    setupUi();
}

TimedReplayPanel::~TimedReplayPanel() {
    delete m_wallClock;
}

void TimedReplayPanel::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    // --- Toolbar row -------------------------------------------------------
    auto* bar = new QHBoxLayout;
    bar->setSpacing(6);

    m_openBtn  = new QPushButton(tr("Open Capture"), this);
    m_fileLabel = new QLabel(tr("No capture loaded"), this);
    m_fileLabel->setStyleSheet("color:#7c8fa6;");

    m_playBtn  = new QPushButton(tr("Play"), this);
    m_pauseBtn = new QPushButton(tr("Pause"), this);
    m_stopBtn  = new QPushButton(tr("Stop"), this);

    m_speedSpin = new QDoubleSpinBox(this);
    m_speedSpin->setRange(0.1, 10.0);
    m_speedSpin->setSingleStep(0.1);
    m_speedSpin->setDecimals(1);
    m_speedSpin->setValue(1.0);
    m_speedSpin->setSuffix(tr("x"));
    m_speedSpin->setToolTip(tr("Playback speed multiplier (0.1x - 10x)"));

    m_loopCheck = new QCheckBox(tr("Loop"), this);

    bar->addWidget(m_openBtn);
    bar->addWidget(m_fileLabel, 1);
    bar->addWidget(m_playBtn);
    bar->addWidget(m_pauseBtn);
    bar->addWidget(m_stopBtn);
    bar->addWidget(new QLabel(tr("Speed:"), this));
    bar->addWidget(m_speedSpin);
    bar->addWidget(m_loopCheck);

    // --- Progress row ------------------------------------------------------
    auto* progRow = new QHBoxLayout;
    progRow->setSpacing(6);
    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 1000);
    m_progress->setValue(0);
    m_progress->setTextVisible(false);
    m_statusLabel = new QLabel("00:00.000 / 00:00.000", this);
    m_statusLabel->setStyleSheet("color:#7c8fa6; font-family: monospace;");
    progRow->addWidget(m_progress, 1);
    progRow->addWidget(m_statusLabel);

    root->addLayout(bar);
    root->addLayout(progRow);
    root->addStretch(1);

    // --- Timing engine -----------------------------------------------------
    // Single-shot chain: each tick emits one (or several due) frame(s) and then
    // re-arms itself for the next recorded inter-frame delay.
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setTimerType(Qt::PreciseTimer);

    connect(m_openBtn,  &QPushButton::clicked, this, &TimedReplayPanel::onOpen);
    connect(m_playBtn,  &QPushButton::clicked, this, &TimedReplayPanel::onPlay);
    connect(m_pauseBtn, &QPushButton::clicked, this, &TimedReplayPanel::onPause);
    connect(m_stopBtn,  &QPushButton::clicked, this, &TimedReplayPanel::onStop);
    connect(m_speedSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &TimedReplayPanel::onSpeedChanged);
    connect(m_timer, &QTimer::timeout, this, &TimedReplayPanel::onTick);

    updateButtons();
}

// ---------------------------------------------------------------------------
// Parsing
// ---------------------------------------------------------------------------

// Accepts two common capture line shapes:
//
//   candump .log : (1623847.123456) can0 1A0#11223344
//   CSV          : timestamp_us,id,dlc,data    e.g. 1623847123456,1A0,4,11223344
//                  (id and data are hex; data may be space/dot separated)
//
// Returns true and fills `out.timestamp_us` (microseconds), id, dlc, data.
bool TimedReplayPanel::parseLine(const QString& raw, socketspy::core::CanFrame& out) const {
    QString line = raw.trimmed();
    if (line.isEmpty() || line.startsWith('#') || line.startsWith("//"))
        return false;

    out = socketspy::core::CanFrame{};

    // ---- candump .log style: starts with '(' --------------------------------
    if (line.startsWith('(')) {
        const int rp = line.indexOf(')');
        if (rp < 0) return false;
        const double tsSec = line.mid(1, rp - 1).toDouble();

        QString rest = line.mid(rp + 1).trimmed();
        // rest = "can0 1A0#11223344"  (iface optional)
        QString idData = rest;
        const int sp = rest.indexOf(' ');
        if (sp >= 0) idData = rest.mid(sp + 1).trimmed();  // drop iface token

        const int hash = idData.indexOf('#');
        if (hash < 0) return false;
        bool ok = false;
        const uint32_t id = idData.left(hash).toUInt(&ok, 16);
        if (!ok) return false;

        const QString dataStr = idData.mid(hash + 1);
        const int nbytes = parseHexBytes(QStringView(dataStr), out.data);

        out.timestamp_us = static_cast<uint64_t>(tsSec * 1'000'000.0);
        out.id  = id;
        out.dlc = static_cast<uint8_t>(nbytes);
        return true;
    }

    // ---- CSV style: timestamp_us,id,dlc,data --------------------------------
    QString sep = line.contains(';') ? ";" : ",";
    const QStringList cols = line.split(sep, Qt::SkipEmptyParts);
    if (cols.size() < 2) return false;

    // Skip a header row (first column not numeric).
    bool tsOk = false;
    const qulonglong ts = cols[0].trimmed().toULongLong(&tsOk);
    if (!tsOk) return false;  // header or malformed line — silently skip

    bool idOk = false;
    QString idTok = cols[1].trimmed();
    if (idTok.startsWith("0x") || idTok.startsWith("0X"))
        idTok = idTok.mid(2);
    const uint32_t id = idTok.toUInt(&idOk, 16);
    if (!idOk) return false;

    out.timestamp_us = static_cast<uint64_t>(ts);
    out.id = id;

    // Optional dlc (col 2) and data (col 3+).
    int explicitDlc = -1;
    int dataColStart = 2;
    if (cols.size() >= 3) {
        bool dlcOk = false;
        const int dlc = cols[2].trimmed().toInt(&dlcOk);
        if (dlcOk && dlc >= 0 && dlc <= 64) {
            explicitDlc = dlc;
            dataColStart = 3;
        }
    }

    // Concatenate remaining columns as the data payload (handles "11 22 33 44"
    // split across CSV cells as well as a single "11223344" cell).
    QString dataStr;
    for (int i = dataColStart; i < cols.size(); ++i)
        dataStr += cols[i].trimmed();
    const int nbytes = parseHexBytes(QStringView(dataStr), out.data);

    out.dlc = static_cast<uint8_t>(explicitDlc >= 0 ? explicitDlc : nbytes);
    return true;
}

void TimedReplayPanel::loadCapture(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_fileLabel->setText(tr("Failed to open: %1").arg(path.section('/', -1)));
        return;
    }

    QVector<socketspy::core::CanFrame> frames;
    QTextStream in(&f);
    socketspy::core::CanFrame fr{};
    while (!in.atEnd()) {
        if (parseLine(in.readLine(), fr))
            frames.append(fr);
    }

    if (frames.isEmpty()) {
        m_fileLabel->setText(tr("No frames parsed in %1").arg(path.section('/', -1)));
        return;
    }

    // Ensure monotonically increasing timestamps so inter-frame deltas are valid.
    std::stable_sort(frames.begin(), frames.end(),
                     [](const socketspy::core::CanFrame& a,
                        const socketspy::core::CanFrame& b) {
                         return a.timestamp_us < b.timestamp_us;
                     });

    m_frames = std::move(frames);
    m_index  = 0;
    m_state  = State::Stopped;
    m_timer->stop();

    const double durSec = static_cast<double>(
        m_frames.last().timestamp_us - m_frames.first().timestamp_us) / 1'000'000.0;
    m_fileLabel->setText(tr("%1  (%2 frames)")
                             .arg(path.section('/', -1))
                             .arg(m_frames.size()));
    m_progress->setValue(0);
    m_statusLabel->setText("00:00.000 / " + formatTime(durSec));
    updateButtons();
}

// ---------------------------------------------------------------------------
// Playback control
// ---------------------------------------------------------------------------

void TimedReplayPanel::onOpen() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open CAN Capture"), {},
        tr("CAN Captures (*.log *.csv *.asc *.txt);;All Files (*)"));
    if (path.isEmpty()) return;
    onStop();
    loadCapture(path);
}

void TimedReplayPanel::onPlay() {
    if (m_frames.isEmpty()) return;

    if (m_state == State::Playing) return;

    if (m_state == State::Stopped) {
        m_index = 0;
        m_progress->setValue(0);
    }

    m_state = State::Playing;

    // Anchor the wall clock to "now == timestamp of the next frame to emit",
    // scaled by speed.  scheduleNext() arms the single-shot timer for the gap
    // until that frame is due.
    m_wallClock->restart();
    scheduleNext();
    updateButtons();
}

void TimedReplayPanel::onPause() {
    if (m_state != State::Playing) return;
    m_state = State::Paused;
    m_timer->stop();
    updateButtons();
}

void TimedReplayPanel::onStop() {
    resetPlayback();
    updateButtons();
    if (!m_frames.isEmpty()) {
        const double durSec = static_cast<double>(
            m_frames.last().timestamp_us - m_frames.first().timestamp_us) / 1'000'000.0;
        m_progress->setValue(0);
        m_statusLabel->setText("00:00.000 / " + formatTime(durSec));
    }
}

void TimedReplayPanel::onSpeedChanged(double speed) {
    m_speed = (speed > 0.0) ? speed : 1.0;
    // Re-anchor so the new rate takes effect from the current position without
    // a timing jump: treat the next pending frame as "now".
    if (m_state == State::Playing && m_index < m_frames.size()) {
        m_timer->stop();
        m_wallClock->restart();
        scheduleNext();
    }
}

// Compute the wall-clock delay (ms) until m_frames[m_index] should be emitted,
// relative to the wall-clock anchor that maps to m_frames[m_index]'s recorded
// time, then arm the single-shot timer.  Because we restart the anchor at each
// (re)schedule the delay is simply the inter-frame delta to the *previous*
// emitted frame — but to avoid cumulative drift we anchor on emit instead.
void TimedReplayPanel::scheduleNext() {
    if (m_index >= m_frames.size()) {
        // End of trace.
        if (m_loopCheck->isChecked() && !m_frames.isEmpty()) {
            m_index = 0;
            m_progress->setValue(0);
            m_wallClock->restart();
        } else {
            onStop();
            return;
        }
    }

    // Anchor: the moment scheduleNext() runs corresponds to the recorded time
    // of the previously emitted frame (or the first frame at start).  Delay to
    // the next frame = (ts[index] - ts[anchorIndex]) / speed.
    const uint64_t anchorTs = (m_index == 0)
        ? m_frames.first().timestamp_us
        : m_frames[m_index - 1].timestamp_us;
    const uint64_t nextTs = m_frames[m_index].timestamp_us;

    const double deltaUs = static_cast<double>(nextTs - anchorTs);
    double waitMs = (deltaUs / 1000.0) / m_speed;
    if (waitMs < 0.0) waitMs = 0.0;

    m_timer->start(static_cast<int>(waitMs + 0.5));
}

void TimedReplayPanel::onTick() {
    if (m_state != State::Playing) return;
    if (m_index >= m_frames.size()) {
        scheduleNext();  // handles loop / stop
        return;
    }

    // Emit the due frame.  Burst-emit any subsequent frames that share an
    // (almost) identical timestamp so zero-gap bursts replay together.
    const uint64_t dueTs = m_frames[m_index].timestamp_us;
    do {
        emit frameReady(m_frames[m_index]);
        ++m_index;
    } while (m_index < m_frames.size() &&
             m_frames[m_index].timestamp_us == dueTs);

    updateProgress();

    if (m_index >= m_frames.size()) {
        if (m_loopCheck->isChecked()) {
            m_index = 0;
            m_progress->setValue(0);
            m_wallClock->restart();
            scheduleNext();
        } else {
            onStop();
        }
        return;
    }

    // Re-anchor on the frame we just emitted to bound cumulative drift.
    m_wallClock->restart();
    scheduleNext();
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void TimedReplayPanel::resetPlayback() {
    m_timer->stop();
    m_state = State::Stopped;
    m_index = 0;
}

void TimedReplayPanel::updateButtons() {
    const bool haveTrace = !m_frames.isEmpty();
    const bool playing   = (m_state == State::Playing);
    const bool paused    = (m_state == State::Paused);

    m_playBtn->setEnabled(haveTrace && !playing);
    m_pauseBtn->setEnabled(playing);
    m_stopBtn->setEnabled(haveTrace && (playing || paused));
}

void TimedReplayPanel::updateProgress() {
    if (m_frames.isEmpty()) return;
    const uint64_t firstTs = m_frames.first().timestamp_us;
    const uint64_t lastTs  = m_frames.last().timestamp_us;
    const uint64_t span    = (lastTs > firstTs) ? (lastTs - firstTs) : 1;

    const int idx = qBound(0, m_index - 1, m_frames.size() - 1);
    const uint64_t curTs = m_frames[idx].timestamp_us;

    const double frac = static_cast<double>(curTs - firstTs) / static_cast<double>(span);
    m_progress->setValue(qBound(0, static_cast<int>(frac * 1000.0), 1000));

    const double curSec = static_cast<double>(curTs - firstTs) / 1'000'000.0;
    const double durSec = static_cast<double>(span) / 1'000'000.0;
    m_statusLabel->setText(formatTime(curSec) + " / " + formatTime(durSec));
}

QString TimedReplayPanel::formatTime(double secs) const {
    if (secs < 0.0) secs = 0.0;
    const int ms = static_cast<int>(secs * 1000.0) % 1000;
    const int s  = static_cast<int>(secs) % 60;
    const int m  = static_cast<int>(secs) / 60;
    return QString("%1:%2.%3")
        .arg(m,  2, 10, QChar('0'))
        .arg(s,  2, 10, QChar('0'))
        .arg(ms, 3, 10, QChar('0'));
}

} // namespace socketspy::gui
