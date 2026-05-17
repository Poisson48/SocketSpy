#include "uds_scanner.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QFileDialog>
#include <QDateTime>
#include <QFile>
#include <QTextStream>

namespace socketspy::gui {

// ── NRC descriptions ────────────────────────────────────────────────────────
const char* UdsScannerWidget::nrcDesc(uint8_t nrc) {
    switch (nrc) {
        case 0x10: return "generalReject";
        case 0x11: return "serviceNotSupported";
        case 0x12: return "subFunctionNotSupported";
        case 0x13: return "incorrectMessageLength";
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

// ── Constructor ─────────────────────────────────────────────────────────────
UdsScannerWidget::UdsScannerWidget(UdsTransport* transport, QWidget* parent)
    : QWidget(parent), m_transport(transport)
{
    setupUi();

    m_stepTimer = new QTimer(this);
    m_stepTimer->setSingleShot(true);
    connect(m_stepTimer, &QTimer::timeout, this, &UdsScannerWidget::onStep);

    m_p2Timer = new QTimer(this);
    m_p2Timer->setSingleShot(true);
    connect(m_p2Timer, &QTimer::timeout, this, &UdsScannerWidget::onScanTimeout);
}

void UdsScannerWidget::setTransport(UdsTransport* transport) {
    m_transport = transport;
}

// ── UI setup ─────────────────────────────────────────────────────────────────
void UdsScannerWidget::setupUi() {
    m_scanBtn   = new QPushButton("Scan",         this);
    m_stopBtn   = new QPushButton("Stop",         this);
    m_exportBtn = new QPushButton("Export CSV...", this);
    m_stopBtn->setEnabled(false);

    auto* btnRow = new QHBoxLayout;
    btnRow->addWidget(m_scanBtn);
    btnRow->addWidget(m_stopBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_exportBtn);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 255);
    m_progress->setValue(0);
    m_progress->setFormat("SID 0x%v / 0xFF");

    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels({"SID (hex)", "Status", "NRC", "Response Time (ms)"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->hide();
    m_table->verticalHeader()->setDefaultSectionSize(22);
    m_table->setAlternatingRowColors(true);

    auto* group  = new QGroupBox("Service Scanner", this);
    auto* glay   = new QVBoxLayout(group);
    glay->addLayout(btnRow);
    glay->addWidget(m_progress);
    glay->addWidget(m_table, 1);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(group, 1);

    connect(m_scanBtn,   &QPushButton::clicked, this, &UdsScannerWidget::onScan);
    connect(m_stopBtn,   &QPushButton::clicked, this, &UdsScannerWidget::onStop);
    connect(m_exportBtn, &QPushButton::clicked, this, &UdsScannerWidget::onExportCsv);
}

// ── Scan control ─────────────────────────────────────────────────────────────
void UdsScannerWidget::onScan() {
    if (!m_transport) return;
    m_scanning = true;
    m_currentSid = 0;
    m_table->setRowCount(0);
    m_progress->setValue(0);
    m_scanBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);

    // Intercept transport signals for the duration of the scan
    connect(m_transport, &UdsTransport::responseReceived,
            this, &UdsScannerWidget::onScanResponse, Qt::UniqueConnection);
    connect(m_transport, &UdsTransport::errorOccurred,
            this, [this](const QString&) { onScanTimeout(); }, Qt::UniqueConnection);

    startStep();
}

void UdsScannerWidget::onStop() {
    m_scanning = false;
    m_stepTimer->stop();
    m_p2Timer->stop();
    finishScan();
}

void UdsScannerWidget::startStep() {
    if (!m_scanning || m_currentSid > 0xFF) { finishScan(); return; }

    m_progress->setValue(m_currentSid);
    m_stepStart = QDateTime::currentMSecsSinceEpoch();
    m_transport->sendRequest({static_cast<uint8_t>(m_currentSid)});
    m_p2Timer->start(m_p2Ms + 20);   // slight margin above P2
}

void UdsScannerWidget::onStep() {
    // Called after 10 ms inter-request gap — move to next SID
    ++m_currentSid;
    startStep();
}

void UdsScannerWidget::onScanResponse(std::vector<uint8_t> data) {
    if (!m_scanning) return;
    m_p2Timer->stop();

    int ms = static_cast<int>(QDateTime::currentMSecsSinceEpoch() - m_stepStart);
    uint8_t sid = static_cast<uint8_t>(m_currentSid);

    if (data.size() >= 3 && data[0] == 0x7F) {
        // Negative response: 0x7F <echo-SID> <NRC>
        uint8_t nrc = data[2];
        addRow(sid,
               QString("NRC 0x%1").arg(nrc, 2, 16, QChar('0')).toUpper(),
               QString("0x%1 %2").arg(nrc, 2, 16, QChar('0')).toUpper()
                                  .append(' ').append(nrcDesc(nrc)),
               ms);
    } else if (!data.empty() && data[0] == (sid | 0x40)) {
        // Positive response: echo SID+0x40
        addRow(sid, "Supported", "", ms);
    } else {
        // Unexpected response — still interesting
        addRow(sid, "Unknown response", "", ms);
    }

    m_stepTimer->start(10);
}

void UdsScannerWidget::onScanTimeout() {
    if (!m_scanning) return;
    int ms = static_cast<int>(QDateTime::currentMSecsSinceEpoch() - m_stepStart);
    addRow(static_cast<uint8_t>(m_currentSid), "Timeout", "", ms);
    m_stepTimer->start(10);
}

void UdsScannerWidget::finishScan() {
    m_scanning = false;
    m_scanBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_progress->setValue(m_currentSid > 0xFF ? 255 : m_currentSid);
    if (m_transport) {
        disconnect(m_transport, &UdsTransport::responseReceived,
                   this, &UdsScannerWidget::onScanResponse);
    }
}

// ── Table helpers ─────────────────────────────────────────────────────────────
void UdsScannerWidget::addRow(uint8_t sid, const QString& status,
                               const QString& nrc, int ms)
{
    int row = m_table->rowCount();
    m_table->insertRow(row);
    m_table->setItem(row, 0, new QTableWidgetItem(
        QString("0x%1").arg(sid, 2, 16, QChar('0')).toUpper()));
    m_table->setItem(row, 1, new QTableWidgetItem(status));
    m_table->setItem(row, 2, new QTableWidgetItem(nrc));
    m_table->setItem(row, 3, new QTableWidgetItem(QString::number(ms)));
}

// ── CSV export ────────────────────────────────────────────────────────────────
void UdsScannerWidget::onExportCsv() {
    QString path = QFileDialog::getSaveFileName(
        this, "Export Service Scan", "uds_scan.csv", "CSV (*.csv)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream ts(&f);
    ts << "SID (hex),Status,NRC,Response Time (ms)\n";
    for (int r = 0; r < m_table->rowCount(); ++r) {
        for (int c = 0; c < 4; ++c) {
            if (c) ts << ',';
            ts << (m_table->item(r, c) ? m_table->item(r, c)->text() : "");
        }
        ts << '\n';
    }
}

} // namespace socketspy::gui
