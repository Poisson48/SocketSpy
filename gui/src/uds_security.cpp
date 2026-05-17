#include "uds_security.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>

namespace socketspy::gui {

UdsSecurityWidget::UdsSecurityWidget(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void UdsSecurityWidget::setupUi() {
    m_levelSpin = new QSpinBox(this);
    m_levelSpin->setRange(1, 255);
    m_levelSpin->setSingleStep(2);   // odd values only (requestSeed levels)
    m_levelSpin->setValue(1);

    m_seedBtn   = new QPushButton("Request Seed", this);
    m_seedLabel = new QLabel("Seed: --", this);
    m_seedLabel->setStyleSheet("font-family: monospace;");

    auto* seedRow = new QHBoxLayout;
    seedRow->addWidget(m_seedBtn);
    seedRow->addWidget(m_seedLabel, 1);

    m_keyEdit      = new QLineEdit(this);
    m_keyEdit->setPlaceholderText("Key bytes in hex, e.g. DEADBEEF");
    m_applyKeyBtn  = new QPushButton("Send Key", this);

    auto* keyRow = new QHBoxLayout;
    keyRow->addWidget(m_keyEdit, 1);
    keyRow->addWidget(m_applyKeyBtn);

    m_securityStatus = new QLabel(QString::fromUtf8("\xf0\x9f\x94\x92 Locked"), this);
    m_securityStatus->setStyleSheet("font-weight: bold;");

    auto* form = new QFormLayout;
    form->addRow("Level (odd = requestSeed):", m_levelSpin);
    form->addRow("", seedRow);
    form->addRow("Key (hex):", keyRow);
    form->addRow("Status:", m_securityStatus);

    auto* group = new QGroupBox("Security Access (0x27)", this);
    group->setLayout(form);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(group);

    connect(m_seedBtn,    &QPushButton::clicked, this, &UdsSecurityWidget::onRequestSeed);
    connect(m_applyKeyBtn,&QPushButton::clicked, this, &UdsSecurityWidget::onApplyKey);
}

void UdsSecurityWidget::setTransport(UdsTransport* transport) {
    m_transport = transport;
}

// ---------------------------------------------------------------------------
// Slots triggered by buttons
// ---------------------------------------------------------------------------

void UdsSecurityWidget::onRequestSeed() {
    m_lastLevel = m_levelSpin->value();
    // Ensure level is odd (requestSeed). If user typed an even value, force odd.
    if (m_lastLevel % 2 == 0) m_lastLevel = (m_lastLevel > 1) ? m_lastLevel - 1 : 1;

    std::vector<uint8_t> req = {0x27, static_cast<uint8_t>(m_lastLevel)};
    m_pendingOp = "seed";
    setBusy(true);
    setSecurityStatus(QString::fromUtf8("\xe2\x8f\xb3 Requesting seed\xe2\x80\xa6"), "#6b7280");
    emit requestReady(req, "SecurityAccess:seed");
}

void UdsSecurityWidget::onApplyKey() {
    const QString hexStr = m_keyEdit->text().trimmed().remove(QChar(' '));
    if (hexStr.isEmpty()) {
        setSecurityStatus(QString::fromUtf8("\xe2\x9d\x8c Key field is empty"), "#ef4444");
        return;
    }

    std::vector<uint8_t> req;
    req.push_back(0x27);
    req.push_back(static_cast<uint8_t>(m_lastLevel + 1)); // sendKey = level + 1 (even)

    bool ok = true;
    for (int i = 0; i + 1 < hexStr.size(); i += 2) {
        uint8_t byte = static_cast<uint8_t>(hexStr.mid(i, 2).toUInt(&ok, 16));
        if (!ok) break;
        req.push_back(byte);
    }
    if (!ok || req.size() < 3) {
        setSecurityStatus(QString::fromUtf8("\xe2\x9d\x8c Invalid hex key"), "#ef4444");
        return;
    }

    m_pendingOp = "key";
    setBusy(true);
    setSecurityStatus(QString::fromUtf8("\xe2\x8f\xb3 Sending key\xe2\x80\xa6"), "#6b7280");
    emit requestReady(req, "SecurityAccess:key");
}

// ---------------------------------------------------------------------------
// Response handling (called by UdsPanel)
// ---------------------------------------------------------------------------

void UdsSecurityWidget::handleResponse(const std::vector<uint8_t>& data) {
    if (!m_pending) return;
    setBusy(false);

    // Negative response: 0x7F 0x27 <NRC>
    if (data.size() >= 3 && data[0] == 0x7F && data[1] == 0x27) {
        const uint8_t nrc = data[2];
        if (nrc == 0x35) {
            setSecurityStatus(QString::fromUtf8("\xe2\x9d\x8c SecurityAccessDenied (invalidKey)"), "#ef4444");
        } else if (nrc == 0x36) {
            setSecurityStatus(QString::fromUtf8("\xe2\x9d\x8c ExceededNumberOfAttempts"), "#ef4444");
        } else {
            setSecurityStatus(
                QString::fromUtf8("\xe2\x9d\x8c NRC 0x%1").arg(nrc, 2, 16, QChar('0')).toUpper(),
                "#ef4444");
        }
        return;
    }

    if (m_pendingOp == "seed") {
        // Positive response: 0x67 <level> <seed_bytes...>
        if (data.size() >= 2 && data[0] == 0x67) {
            QString seedHex;
            for (std::size_t i = 2; i < data.size(); ++i)
                seedHex += QString("%1").arg(data[i], 2, 16, QChar('0')).toUpper();
            if (seedHex.isEmpty()) seedHex = "(empty — already unlocked?)";
            m_seedLabel->setText("Seed: " + seedHex);
            setSecurityStatus(QString::fromUtf8("\xf0\x9f\x94\x92 Seed received — enter key"), "#f59e0b");
        } else {
            setSecurityStatus(QString::fromUtf8("\xe2\x9d\x8c Unexpected seed response"), "#ef4444");
        }
    } else if (m_pendingOp == "key") {
        // Positive response: 0x67 <level+1>
        if (data.size() >= 2 && data[0] == 0x67) {
            setSecurityStatus(QString::fromUtf8("\xf0\x9f\x94\x93 Unlocked"), "#22c55e");
        } else {
            setSecurityStatus(QString::fromUtf8("\xe2\x9d\x8c Unexpected key response"), "#ef4444");
        }
    }
}

void UdsSecurityWidget::handleError(const QString& message) {
    if (!m_pending) return;
    setBusy(false);
    setSecurityStatus(QString::fromUtf8("\xe2\x9d\x8c ") + message, "#ef4444");
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void UdsSecurityWidget::setSecurityStatus(const QString& text, const QString& color) {
    m_securityStatus->setText(text);
    m_securityStatus->setStyleSheet(
        QString("font-weight: bold; color: %1;").arg(color));
}

void UdsSecurityWidget::setBusy(bool busy) {
    m_pending = busy;
    m_seedBtn->setEnabled(!busy);
    m_applyKeyBtn->setEnabled(!busy);
    m_levelSpin->setEnabled(!busy);
}

} // namespace socketspy::gui
