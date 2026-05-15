#include "uds_panel.h"
#include "iface_detector.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QTableWidgetItem>

namespace socketspy::gui {

// NRC (Negative Response Code) descriptions
static const char* nrcDescription(uint8_t nrc) {
    switch (nrc) {
        case 0x10: return "generalReject";
        case 0x11: return "serviceNotSupported";
        case 0x12: return "subFunctionNotSupported";
        case 0x13: return "incorrectMessageLengthOrInvalidFormat";
        case 0x14: return "responseTooLong";
        case 0x21: return "busyRepeatRequest";
        case 0x22: return "conditionsNotCorrect";
        case 0x24: return "requestSequenceError";
        case 0x31: return "requestOutOfRange";
        case 0x33: return "securityAccessDenied";
        case 0x35: return "invalidKey";
        case 0x36: return "exceededNumberOfAttempts";
        case 0x37: return "requiredTimeDelayNotExpired";
        case 0x70: return "uploadDownloadNotAccepted";
        case 0x71: return "transferDataSuspended";
        case 0x72: return "generalProgrammingFailure";
        case 0x73: return "wrongBlockSequenceCounter";
        case 0x78: return "requestCorrectlyReceivedResponsePending";
        case 0x7E: return "subFunctionNotSupportedInActiveSession";
        case 0x7F: return "serviceNotSupportedInActiveSession";
        default:   return "unknown";
    }
}

UdsPanel::UdsPanel(QWidget* parent) : QWidget(parent) {
    m_transport = new UdsTransport(this);
    setupUi();
    connect(m_transport, &UdsTransport::responseReceived,
            this, &UdsPanel::onResponseReceived);
    connect(m_transport, &UdsTransport::errorOccurred,
            this, &UdsPanel::onUdsError);
}

void UdsPanel::setupUi() {
    m_iface = new QComboBox(this);
    m_iface->addItems(IfaceDetector::scanCanIfaces());

    auto* refreshBtn = new QPushButton(QString::fromUtf8("↺"), this);
    refreshBtn->setFixedWidth(28);
    connect(refreshBtn, &QPushButton::clicked, this, &UdsPanel::refreshIfaces);

    auto* ifaceRow = new QHBoxLayout;
    ifaceRow->addWidget(m_iface, 1);
    ifaceRow->addWidget(refreshBtn);

    m_txId = new QLineEdit("7DF", this);
    m_rxId = new QLineEdit("7E8", this);

    m_p2 = new QSpinBox(this);
    m_p2->setRange(10, 5000);
    m_p2->setValue(50);
    m_p2->setSuffix(" ms");

    m_applyBtn = new QPushButton("Apply", this);

    auto* configGroup = new QGroupBox("ECU Configuration", this);
    auto* configForm  = new QFormLayout(configGroup);
    configForm->addRow("Interface:", ifaceRow);
    configForm->addRow("TX ID (hex):", m_txId);
    configForm->addRow("RX ID (hex):", m_rxId);
    configForm->addRow("P2 Timeout:", m_p2);
    configForm->addRow("", m_applyBtn);

    m_sessionLabel = new QLabel("Session: Default (0x01)", this);
    m_sessionLabel->setStyleSheet("font-weight: bold; color: #3b82f6;");

    m_readDtcBtn  = new QPushButton("Read DTC",   this);
    m_clearDtcBtn = new QPushButton("Clear DTC",  this);
    m_ecuInfoBtn  = new QPushButton("Read ECU Info", this);

    auto* btnRow = new QHBoxLayout;
    btnRow->addWidget(m_readDtcBtn);
    btnRow->addWidget(m_clearDtcBtn);
    btnRow->addWidget(m_ecuInfoBtn);
    btnRow->addStretch();

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: #6b7280;");

    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels({"Source", "Key", "Value", "Details"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->hide();
    m_table->verticalHeader()->setDefaultSectionSize(24);
    m_table->setAlternatingRowColors(true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    layout->addWidget(configGroup);
    layout->addWidget(m_sessionLabel);
    layout->addLayout(btnRow);
    layout->addWidget(m_statusLabel);
    layout->addWidget(m_table, 1);

    connect(m_applyBtn,   &QPushButton::clicked, this, &UdsPanel::onApplyConfig);
    connect(m_readDtcBtn, &QPushButton::clicked, this, &UdsPanel::onReadDtc);
    connect(m_clearDtcBtn,&QPushButton::clicked, this, &UdsPanel::onClearDtc);
    connect(m_ecuInfoBtn, &QPushButton::clicked, this, &UdsPanel::onReadEcuInfo);
}

void UdsPanel::refreshIfaces() {
    const QString cur = m_iface->currentText();
    m_iface->blockSignals(true);
    m_iface->clear();
    m_iface->addItems(IfaceDetector::scanCanIfaces());
    m_iface->blockSignals(false);
    int idx = m_iface->findText(cur);
    if (idx >= 0) m_iface->setCurrentIndex(idx);
}

void UdsPanel::onApplyConfig() {
    bool ok1 = false, ok2 = false;
    uint32_t tx = m_txId->text().toUInt(&ok1, 16);
    uint32_t rx = m_rxId->text().toUInt(&ok2, 16);
    if (!ok1 || !ok2) { setStatus("Invalid TX or RX ID", false); return; }

    m_transport->setInterface(m_iface->currentText());
    m_transport->setTxId(tx);
    m_transport->setRxId(rx);
    m_transport->setP2Timeout(m_p2->value());
    setStatus("Configuration applied");
}

void UdsPanel::sendRequest(const std::vector<uint8_t>& req, const QString& label) {
    m_pendingLabel = label;
    setStatus("Sending " + label + "…");
    m_readDtcBtn->setEnabled(false);
    m_clearDtcBtn->setEnabled(false);
    m_ecuInfoBtn->setEnabled(false);
    m_transport->sendRequest(req);
}

void UdsPanel::onReadDtc() {
    // Service 0x19 (ReadDTCInformation), subfunction 0x02 (reportDTCByStatusMask), mask=0xFF
    sendRequest({0x19, 0x02, 0xFF}, "Read DTC");
}

void UdsPanel::onClearDtc() {
    // Service 0x14 (ClearDiagnosticInformation), groupOfDTC=0xFFFFFF (all)
    sendRequest({0x14, 0xFF, 0xFF, 0xFF}, "Clear DTC");
}

void UdsPanel::onReadEcuInfo() {
    // Service 0x22 (ReadDataByIdentifier) for multiple DIDs
    // Send first DID; chain the rest in response handler via m_pendingLabel
    sendRequest({0x22, 0xF1, 0x86}, "ReadDID:F186");
}

void UdsPanel::onResponseReceived(std::vector<uint8_t> data) {
    m_readDtcBtn->setEnabled(true);
    m_clearDtcBtn->setEnabled(true);
    m_ecuInfoBtn->setEnabled(true);

    if (data.empty()) { setStatus("Empty response", false); return; }

    // Check for NRC (negative response: 0x7F + service + NRC)
    if (data.size() >= 3 && data[0] == 0x7F) {
        uint8_t nrc = data[2];
        setStatus(QString("Negative response: 0x%1 — %2")
            .arg(nrc, 2, 16, QChar('0')).toUpper()
            .arg(nrcDescription(nrc)), false);
        return;
    }

    const QString& label = m_pendingLabel;

    if (label == "Read DTC") {
        parseAndShowDtcs(data);
    } else if (label == "Clear DTC") {
        if (!data.empty() && data[0] == 0x54) {
            setStatus("DTC cleared successfully");
        } else {
            setStatus("Unexpected response to ClearDTC", false);
        }
    } else if (label.startsWith("ReadDID:")) {
        parseAndShowDids(data, label);

        // Chain next DID requests
        static const QStringList kDidChain = {
            "ReadDID:F187", "ReadDID:F18C", "ReadDID:F190"
        };
        static const QList<std::vector<uint8_t>> kDidReqs = {
            {0x22, 0xF1, 0x87}, {0x22, 0xF1, 0x8C}, {0x22, 0xF1, 0x90}
        };
        int idx = kDidChain.indexOf(label);
        if (idx < 0) {
            // This is F186, start chain from F187
            sendRequest({0x22, 0xF1, 0x87}, "ReadDID:F187");
        } else if (idx + 1 < kDidChain.size()) {
            sendRequest(kDidReqs[idx + 1], kDidChain[idx + 1]);
        }
    }
}

void UdsPanel::parseAndShowDtcs(const std::vector<uint8_t>& resp) {
    if (resp.empty() || resp[0] != 0x59) { setStatus("Unexpected DTC response", false); return; }

    m_table->setRowCount(0);
    // Response: 0x59 0x02 DTCHighByte DTCMiddleByte DTCLowByte StatusByte ...
    int offset = 3; // skip 0x59, subfunction, DTCStatusAvailabilityMask
    int dtcCount = 0;
    while (offset + 3 < static_cast<int>(resp.size())) {
        uint32_t dtc = (static_cast<uint32_t>(resp[offset]) << 16)
                     | (static_cast<uint32_t>(resp[offset+1]) << 8)
                     | static_cast<uint32_t>(resp[offset+2]);
        uint8_t status = resp[offset + 3];
        offset += 4;
        ++dtcCount;

        int row = m_table->rowCount();
        m_table->insertRow(row);
        m_table->setItem(row, 0, new QTableWidgetItem("DTC"));
        m_table->setItem(row, 1, new QTableWidgetItem(
            QString("P%1").arg(dtc, 4, 16, QChar('0')).toUpper()));
        m_table->setItem(row, 2, new QTableWidgetItem(
            QString("Status: 0x%1").arg(status, 2, 16, QChar('0')).toUpper()));
        m_table->setItem(row, 3, new QTableWidgetItem("–"));
    }
    setStatus(QString("Read DTC: %1 fault code(s)").arg(dtcCount));
}

void UdsPanel::parseAndShowDids(const std::vector<uint8_t>& resp, const QString& label) {
    if (resp.size() < 3 || resp[0] != 0x62) {
        // Maybe NRC — already handled above
        return;
    }
    uint16_t did = (static_cast<uint16_t>(resp[1]) << 8) | resp[2];

    static const QHash<uint16_t, QString> kDidNames = {
        {0xF186, "Active Session"},
        {0xF187, "Spare Part Number"},
        {0xF18C, "ECU Serial Number"},
        {0xF190, "VIN"},
    };
    QString didName = kDidNames.value(did, QString("DID 0x%1").arg(did, 4, 16, QChar('0')).toUpper());

    QString value;
    for (int i = 3; i < static_cast<int>(resp.size()); ++i) {
        uint8_t b = resp[i];
        if (b >= 0x20 && b < 0x7F) value += static_cast<char>(b);
        else value += QString("\\x%1").arg(b, 2, 16, QChar('0')).toUpper();
    }
    if (value.isEmpty()) value = "(empty)";

    if (did == 0xF186 && resp.size() >= 4) {
        m_sessionId = resp[3];
        m_sessionLabel->setText(QString("Session: 0x%1").arg(m_sessionId, 2, 16, QChar('0')).toUpper());
    }

    int row = m_table->rowCount();
    m_table->insertRow(row);
    m_table->setItem(row, 0, new QTableWidgetItem("ECU Info"));
    m_table->setItem(row, 1, new QTableWidgetItem(didName));
    m_table->setItem(row, 2, new QTableWidgetItem(value));
    m_table->setItem(row, 3, new QTableWidgetItem(
        QString("0x%1").arg(did, 4, 16, QChar('0')).toUpper()));

    setStatus(label.mid(8) + " read OK");
}

void UdsPanel::onUdsError(QString message) {
    m_readDtcBtn->setEnabled(true);
    m_clearDtcBtn->setEnabled(true);
    m_ecuInfoBtn->setEnabled(true);
    setStatus(message, false);
}

void UdsPanel::setStatus(const QString& msg, bool ok) {
    m_statusLabel->setText(ok
        ? "<font color='#22c55e'>" + msg + "</font>"
        : "<font color='#ef4444'>" + msg + "</font>");
}

} // namespace socketspy::gui
