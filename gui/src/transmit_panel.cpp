#include "transmit_panel.h"
#include "iface_detector.h"
#include "cancore.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
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
    refreshBtn->setFixedWidth(28);
    refreshBtn->setToolTip("Rafraîchir la liste des interfaces");
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
    m_send     = new QPushButton("Send", this);
    m_status   = new QLabel(this);

    auto* form = new QFormLayout;
    form->addRow("Interface:", ifaceRow);
    form->addRow("ID (hex):", m_id);
    form->addRow("DLC:", m_dlc);
    form->addRow("Data (hex bytes):", m_data);
    form->addRow("", m_extended);

    auto* btnRow = new QHBoxLayout;
    btnRow->addWidget(m_send);
    btnRow->addWidget(m_status);
    btnRow->addStretch();

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addLayout(btnRow);
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

void TransmitPanel::onSend() {
    QString err;
    if (!validate(err)) { m_status->setText("<font color='red'>" + err + "</font>"); return; }

    IfaceHandle h = can_open(m_iface->currentText().toStdString());
    if (!h.valid()) {
        m_status->setText(QString("<font color='red'>can_open: %1</font>").arg(strerror(errno)));
        return;
    }

    CanFrame f{};
    f.id  = m_id->text().toUInt(nullptr, 16);
    f.dlc = static_cast<uint8_t>(m_dlc->value());
    QStringList tokens = m_data->text().trimmed().split(' ', Qt::SkipEmptyParts);
    for (int i = 0; i < tokens.size() && i < 8; ++i)
        f.data[i] = static_cast<uint8_t>(tokens[i].toUInt(nullptr, 16));

    bool ok = can_send(h, f);
    can_close(h);
    m_status->setText(ok ? "<font color='green'>OK</font>"
                         : QString("<font color='red'>send failed: %1</font>").arg(strerror(errno)));
}

} // namespace socketspy::gui
