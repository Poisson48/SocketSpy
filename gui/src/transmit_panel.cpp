#include "transmit_panel.h"
#include "iface_detector.h"
#include "cancore.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QPushButton>
#include <cerrno>
#include <cstring>

using namespace socketspy::core;

namespace socketspy::gui {

TransmitPanel::TransmitPanel(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void TransmitPanel::setupUi() {
    m_iface    = new QComboBox(this);
    m_iface->addItems(IfaceDetector::scanCanIfaces());
    m_iface->setMinimumWidth(120);

    auto* refreshBtn = new QPushButton("↺", this);
    refreshBtn->setObjectName("refreshBtn");
    refreshBtn->setFixedWidth(28);
    refreshBtn->setToolTip("Refresh interface list");
    connect(refreshBtn, &QPushButton::clicked, this, &TransmitPanel::refreshIfaces);

    auto* ifaceRow = new QHBoxLayout;
    ifaceRow->addWidget(m_iface);
    ifaceRow->addWidget(refreshBtn);

    m_id       = new QLineEdit("000", this);
    m_dlc      = new QSpinBox(this);
    m_dlc->setRange(0, 8);
    m_data     = new QLineEdit(this);
    m_data->setPlaceholderText("DE AD BE EF ...");
    m_extended = new QCheckBox("Extended ID (29-bit)", this);
    m_fd       = new QCheckBox("FD frame", this);
    m_send     = new QPushButton("Send once", this);
    m_status   = new QLabel(this);

    connect(m_fd, &QCheckBox::toggled, this, [this](bool checked) {
        m_dlc->setRange(0, checked ? 15 : 8);
    });

    auto* form = new QFormLayout;
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(6);
    form->addRow("Interface:", ifaceRow);
    form->addRow("ID (hex):", m_id);
    form->addRow("DLC:", m_dlc);
    form->addRow("Data (hex bytes):", m_data);
    form->addRow("", m_extended);
    form->addRow("", m_fd);

    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(6);
    btnRow->addWidget(m_send);
    btnRow->addWidget(m_status);
    btnRow->addStretch();

    // ----- Periodic send group (Gap E) -----
    auto* periodicGroup = new QGroupBox("Periodic send", this);
    m_periodicChk  = new QCheckBox("Enable periodic transmit", periodicGroup);
    m_periodicChk->setToolTip("Send the frame above repeatedly at the chosen interval");

    m_intervalMs = new QSpinBox(periodicGroup);
    m_intervalMs->setRange(1, 60000);
    m_intervalMs->setValue(100);
    m_intervalMs->setSuffix(" ms");
    m_intervalMs->setSingleStep(10);
    m_intervalMs->setToolTip("Transmission interval (1 ms – 60 s)");

    m_periodicStatus = new QLabel("–", periodicGroup);
    m_periodicStatus->setStyleSheet("color: #7c8fa6;");

    auto* periodicForm = new QFormLayout(periodicGroup);
    periodicForm->setSpacing(6);
    periodicForm->addRow(m_periodicChk);
    periodicForm->addRow("Interval:", m_intervalMs);
    periodicForm->addRow("Sent:", m_periodicStatus);

    m_periodicTimer = new QTimer(this);
    connect(m_periodicTimer, &QTimer::timeout, this, &TransmitPanel::onPeriodicTick);
    connect(m_periodicChk, &QCheckBox::toggled, this, &TransmitPanel::onTogglePeriodic);
    // Changing interval while running: update the timer period immediately
    connect(m_intervalMs, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int ms) {
                if (m_periodicTimer->isActive())
                    m_periodicTimer->setInterval(ms);
            });

    auto* layout = new QVBoxLayout(this);
    // Consistent outer margin 8px, inner spacing 6px
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    layout->addLayout(form);
    layout->addLayout(btnRow);
    layout->addWidget(periodicGroup);
    layout->addStretch();

    connect(m_send, &QPushButton::clicked, this, &TransmitPanel::onSend);
}

void TransmitPanel::setCurrentIface(const QString& iface) {
    int idx = m_iface->findText(iface);
    if (idx >= 0) { m_iface->setCurrentIndex(idx); return; }
    m_iface->addItem(iface);
    m_iface->setCurrentText(iface);
}

void TransmitPanel::refreshIfaces() {
    const QString current = m_iface->currentText();
    m_iface->blockSignals(true);
    m_iface->clear();
    m_iface->addItems(IfaceDetector::scanCanIfaces());
    m_iface->blockSignals(false);
    setCurrentIface(current);
}

bool TransmitPanel::validate(QString& err) const {
    bool ok = false;
    uint32_t maxId = m_extended->isChecked() ? 0x1FFFFFFFU : 0x7FFU;
    uint32_t id = m_id->text().toUInt(&ok, 16);
    if (!ok || id > maxId) {
        err = m_extended->isChecked()
            ? "ID must be 0..0x1FFFFFFF for extended frames"
            : "ID must be 0..0x7FF for standard frames";
        return false;
    }
    if (!m_data->text().trimmed().isEmpty()) {
        QStringList tokens = m_data->text().trimmed().split(' ', Qt::SkipEmptyParts);
        if (tokens.size() > m_dlc->value()) {
            err = QString("Data has %1 bytes but DLC is %2").arg(tokens.size()).arg(m_dlc->value());
            return false;
        }
        for (const auto& t : tokens) {
            if (t.length() > 2) { err = "Each data byte is max 2 hex digits"; return false; }
            t.toUInt(&ok, 16);
            if (!ok) { err = "Invalid hex byte: " + t; return false; }
        }
    }
    return true;
}

bool TransmitPanel::sendFrame() {
    IfaceHandle h = can_open(m_iface->currentText().toStdString());
    if (!h.valid()) {
        m_status->setText(QString("<font color='red'>can_open: %1</font>").arg(strerror(errno)));
        return false;
    }

    const bool isFd = m_fd->isChecked();
    if (isFd) {
        if (!can_set_fd_mode(h, true)) {
            m_status->setText("<font color='red'>can_set_fd_mode failed</font>");
            can_close(h);
            return false;
        }
    }

    CanFrame f{};
    f.id  = m_id->text().toUInt(nullptr, 16);
    f.dlc = static_cast<uint8_t>(m_dlc->value());
    if (isFd)
        f.flags = static_cast<uint8_t>(FrameFlags::FD);
    QStringList tokens = m_data->text().trimmed().split(' ', Qt::SkipEmptyParts);
    const int maxBytes = isFd ? 64 : 8;
    for (int i = 0; i < tokens.size() && i < maxBytes; ++i)
        f.data[i] = static_cast<uint8_t>(tokens[i].toUInt(nullptr, 16));

    bool ok = can_send(h, f);
    can_close(h);
    return ok;
}

void TransmitPanel::onSend() {
    QString err;
    if (!validate(err)) { m_status->setText("<font color='red'>" + err + "</font>"); return; }

    bool ok = sendFrame();
    m_status->setText(ok ? "<font color='green'>OK</font>"
                         : QString("<font color='red'>send failed: %1</font>").arg(strerror(errno)));
}

void TransmitPanel::onTogglePeriodic(bool checked) {
    if (checked) {
        // Validate before starting
        QString err;
        if (!validate(err)) {
            m_status->setText("<font color='red'>" + err + "</font>");
            m_periodicChk->setChecked(false);
            return;
        }
        m_periodicCount = 0;
        m_periodicStatus->setText("0");
        m_periodicStatus->setStyleSheet("color: #22c55e;");
        m_periodicTimer->start(m_intervalMs->value());
        // Disable send controls while periodic is active to avoid mid-run edits
        m_send->setEnabled(false);
        m_id->setEnabled(false);
        m_dlc->setEnabled(false);
        m_data->setEnabled(false);
        m_extended->setEnabled(false);
        m_fd->setEnabled(false);
        m_status->setText("<font color='#6366f1'>Periodic running…</font>");
    } else {
        m_periodicTimer->stop();
        m_periodicStatus->setStyleSheet("color: #7c8fa6;");
        m_send->setEnabled(true);
        m_id->setEnabled(true);
        m_dlc->setEnabled(true);
        m_data->setEnabled(true);
        m_extended->setEnabled(true);
        m_fd->setEnabled(true);
        m_status->setText(tr("Periodic stopped — %1 frames sent").arg(m_periodicCount));
    }
}

void TransmitPanel::onPeriodicTick() {
    bool ok = sendFrame();
    if (ok) {
        ++m_periodicCount;
        m_periodicStatus->setText(QString::number(m_periodicCount));
    } else {
        // Stop periodic on send error to avoid spamming bad frames
        m_periodicChk->setChecked(false);
        m_status->setText(
            QString("<font color='red'>Periodic stopped: send failed — %1</font>")
            .arg(strerror(errno)));
    }
}

} // namespace socketspy::gui
