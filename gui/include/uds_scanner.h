#pragma once
#include <QWidget>
#include <QGroupBox>
#include <QPushButton>
#include <QProgressBar>
#include <QTableWidget>
#include <QTimer>
#include <vector>
#include <cstdint>
#include "uds_transport.h"

namespace socketspy::gui {

// Scans UDS services SID 0x00–0xFF by sending single-byte requests
// and recording positive / NRC / timeout results in a table.
class UdsScannerWidget : public QWidget {
    Q_OBJECT

public:
    explicit UdsScannerWidget(UdsTransport* transport, QWidget* parent = nullptr);

    // Must be called whenever the scanner should refresh its transport reference
    void setTransport(UdsTransport* transport);

private slots:
    void onScan();
    void onStop();
    void onExportCsv();
    void onStep();
    void onScanResponse(std::vector<uint8_t> data);
    void onScanTimeout();

private:
    void setupUi();
    void startStep();
    void finishScan();
    void addRow(uint8_t sid, const QString& status, const QString& nrc, int ms);
    static const char* nrcDesc(uint8_t nrc);

    UdsTransport*  m_transport{nullptr};
    QPushButton*   m_scanBtn{nullptr};
    QPushButton*   m_stopBtn{nullptr};
    QPushButton*   m_exportBtn{nullptr};
    QProgressBar*  m_progress{nullptr};
    QTableWidget*  m_table{nullptr};

    QTimer*        m_stepTimer{nullptr};   // 10 ms gap between requests
    QTimer*        m_p2Timer{nullptr};     // P2 timeout per SID

    bool           m_scanning{false};
    int            m_currentSid{0};
    qint64         m_stepStart{0};         // ms timestamp of current send
    int            m_p2Ms{50};            // mirrors UdsTransport P2 timeout
};

} // namespace socketspy::gui
