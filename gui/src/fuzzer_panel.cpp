#include "fuzzer_panel.h"
#include "iface_detector.h"
#include "cancore.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QRandomGenerator>
#include <cerrno>
#include <cstring>
#include <cstdlib>

using namespace socketspy::core;

namespace socketspy::gui {

FuzzerPanel::FuzzerPanel(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void FuzzerPanel::setupUi() {
    m_iface = new QComboBox(this);
    m_iface->addItems(IfaceDetector::scanCanIfaces());

    auto* refreshBtn = new QPushButton(QString::fromUtf8("↺"), this);
    refreshBtn->setFixedWidth(28);
    refreshBtn->setToolTip("Refresh interface list");
    connect(refreshBtn, &QPushButton::clicked, this, &FuzzerPanel::refreshIfaces);

    auto* ifaceRow = new QHBoxLayout;
    ifaceRow->addWidget(m_iface, 1);
    ifaceRow->addWidget(refreshBtn);

    m_id = new QLineEdit("000", this);
    m_id->setPlaceholderText("CAN ID (hex)");

    m_dlc = new QSpinBox(this);
    m_dlc->setRange(0, 8);
    m_dlc->setValue(8);

    m_fd = new QCheckBox("CAN FD", this);
    connect(m_fd, &QCheckBox::toggled, this, [this](bool on) {
        m_dlc->setRange(0, on ? 64 : 8);
    });

    m_mode = new QComboBox(this);
    m_mode->addItem("Random",    0);
    m_mode->addItem("Increment", 1);
    m_mode->addItem("Bit-flip",  2);

    m_interval = new QSpinBox(this);
    m_interval->setRange(1, 60000);
    m_interval->setValue(100);
    m_interval->setSuffix(" ms");

    auto* form = new QFormLayout;
    form->setSpacing(6);
    form->addRow("Interface:", ifaceRow);
    form->addRow("CAN ID (hex):", m_id);
    form->addRow("DLC:", m_dlc);
    form->addRow("", m_fd);
    form->addRow("Fuzz mode:", m_mode);
    form->addRow("Interval:", m_interval);

    m_startStop = new QPushButton("Start", this);
    m_startStop->setCheckable(true);
    m_startStop->setObjectName("recordBtn");

    m_counter = new QLabel("0 frames sent", this);
    m_counter->setStyleSheet("color: #6b7280;");

    m_status = new QLabel(this);

    auto* btnRow = new QHBoxLayout;
    btnRow->addWidget(m_startStop);
    btnRow->addWidget(m_counter);
    btnRow->addWidget(m_status);
    btnRow->addStretch();

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    layout->addLayout(form);
    layout->addLayout(btnRow);
    layout->addStretch();

    m_timer = new QTimer(this);
    connect(m_timer,    &QTimer::timeout,         this, &FuzzerPanel::onFuzzTick);
    connect(m_startStop, &QPushButton::toggled,   this, &FuzzerPanel::onToggleFuzz);
    connect(m_interval, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int ms) {
        if (m_timer->isActive()) m_timer->setInterval(ms);
    });
}

void FuzzerPanel::refreshIfaces() {
    const QString cur = m_iface->currentText();
    m_iface->blockSignals(true);
    m_iface->clear();
    m_iface->addItems(IfaceDetector::scanCanIfaces());
    m_iface->blockSignals(false);
    int idx = m_iface->findText(cur);
    if (idx >= 0) m_iface->setCurrentIndex(idx);
}

bool FuzzerPanel::buildFrame(CanFrame& frame, QString& err) {
    bool ok = false;
    uint32_t id = m_id->text().toUInt(&ok, 16);
    if (!ok) { err = "Invalid CAN ID"; return false; }

    frame = CanFrame{};
    frame.id  = id;
    frame.dlc = static_cast<uint8_t>(m_dlc->value());
    if (m_fd->isChecked())
        frame.flags = static_cast<uint8_t>(FrameFlags::FD);

    const int modeIdx = m_mode->currentIndex();
    const int dlc     = frame.dlc;

    if (modeIdx == 0) {
        // Random
        for (int i = 0; i < dlc; ++i)
            frame.data[i] = static_cast<uint8_t>(QRandomGenerator::global()->bounded(256));
    } else if (modeIdx == 1) {
        // Increment
        for (int i = 0; i < dlc; ++i)
            frame.data[i] = m_incrementState[i];
        // increment with carry
        for (int i = dlc - 1; i >= 0; --i) {
            if (++m_incrementState[i] != 0) break;
        }
    } else {
        // Bit-flip: start with zeros, flip one bit
        if (dlc > 0) {
            int totalBits = dlc * 8;
            m_bitFlipPos  = static_cast<int>(m_count % static_cast<uint64_t>(totalBits));
            frame.data[m_bitFlipPos / 8] = static_cast<uint8_t>(1u << (m_bitFlipPos % 8));
        }
    }
    return true;
}

void FuzzerPanel::onToggleFuzz(bool checked) {
    if (checked) {
        // Validate ID
        bool ok = false;
        m_id->text().toUInt(&ok, 16);
        if (!ok) {
            m_status->setText("<font color='red'>Invalid CAN ID</font>");
            m_startStop->setChecked(false);
            return;
        }
        m_count = 0;
        std::memset(m_incrementState, 0, sizeof(m_incrementState));
        m_bitFlipPos = 0;
        m_counter->setText("0 frames sent");
        m_counter->setStyleSheet("color: #22c55e;");
        m_startStop->setText("Stop");
        m_status->setText("<font color='#6366f1'>Fuzzing…</font>");
        m_running = true;
        m_timer->start(m_interval->value());
        m_id->setEnabled(false);
        m_dlc->setEnabled(false);
        m_mode->setEnabled(false);
        m_fd->setEnabled(false);
        m_iface->setEnabled(false);
    } else {
        m_timer->stop();
        m_running = false;
        m_startStop->setText("Start");
        m_counter->setStyleSheet("color: #6b7280;");
        m_status->setText(QString("Stopped — %1 frames sent").arg(m_count));
        m_id->setEnabled(true);
        m_dlc->setEnabled(true);
        m_mode->setEnabled(true);
        m_fd->setEnabled(true);
        m_iface->setEnabled(true);
    }
}

void FuzzerPanel::onFuzzTick() {
    CanFrame frame;
    QString  err;
    if (!buildFrame(frame, err)) {
        m_startStop->setChecked(false);
        m_status->setText("<font color='red'>Build error: " + err + "</font>");
        return;
    }

    IfaceHandle h = can_open(m_iface->currentText().toStdString());
    if (!h.valid()) {
        m_startStop->setChecked(false);
        m_status->setText(QString("<font color='red'>can_open failed: %1</font>").arg(strerror(errno)));
        return;
    }
    if (m_fd->isChecked()) can_set_fd_mode(h, true);
    bool ok = can_send(h, frame);
    can_close(h);

    if (ok) {
        ++m_count;
        m_counter->setText(QString::number(m_count) + " frames sent");
    } else {
        m_startStop->setChecked(false);
        m_status->setText(QString("<font color='red'>Send failed: %1</font>").arg(strerror(errno)));
    }
}

} // namespace socketspy::gui
