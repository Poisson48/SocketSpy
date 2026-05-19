#include "elm327_panel.h"
#include "elm327_bridge.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDir>
#include <QBluetoothAddress>

namespace socketspy::gui {

Elm327Panel::Elm327Panel(QWidget* parent)
    : QWidget(parent)
    , m_bridge(new Elm327Bridge(this))
    , m_btAgent(new QBluetoothDeviceDiscoveryAgent(this))
{
    setupUi();

    connect(m_bridge, &Elm327Bridge::frameReceived, this, &Elm327Panel::frameReceived);
    connect(m_bridge, &Elm327Bridge::connectionError, this, [this](const QString& err) {
        m_statusLabel->setText("Error: " + err);
        m_connectBtn->setText("Connect");
        m_connected = false;
    });
    connect(m_bridge, &Elm327Bridge::statusChanged, m_statusLabel, &QLabel::setText);

    connect(m_btAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &Elm327Panel::onDeviceFound);
    connect(m_btAgent, &QBluetoothDeviceDiscoveryAgent::finished,
            this, &Elm327Panel::onScanFinished);
    connect(m_btAgent, &QBluetoothDeviceDiscoveryAgent::canceled,
            this, &Elm327Panel::onScanFinished);
    connect(m_btAgent, &QBluetoothDeviceDiscoveryAgent::errorOccurred,
            this, [this](QBluetoothDeviceDiscoveryAgent::Error) {
                m_scanBtn->setText("Scan");
                m_scanLabel->setText("Error: " + m_btAgent->errorString());
            });
}

void Elm327Panel::setupUi() {
    auto* root = new QVBoxLayout(this);

    auto* group = new QGroupBox("OBD2 / ELM327", this);
    auto* gl = new QVBoxLayout(group);

    // --- Bluetooth section ---
    auto* btGroup = new QGroupBox("Bluetooth", group);
    auto* btLayout = new QVBoxLayout(btGroup);

    auto* scanRow = new QHBoxLayout;
    m_scanBtn = new QPushButton("Scan", btGroup);
    m_scanLabel = new QLabel("Ready", btGroup);
    scanRow->addWidget(m_scanBtn);
    scanRow->addWidget(m_scanLabel);
    scanRow->addStretch();
    btLayout->addLayout(scanRow);

    m_btList = new QListWidget(btGroup);
    m_btList->setMaximumHeight(100);
    m_btList->setSelectionMode(QAbstractItemView::SingleSelection);
    btLayout->addWidget(m_btList);

    gl->addWidget(btGroup);

    // --- USB / Serial section ---
    auto* serialGroup = new QGroupBox("USB / Serial", group);
    auto* serialLayout = new QVBoxLayout(serialGroup);

    auto* portRow = new QHBoxLayout;
    portRow->addWidget(new QLabel("Port:", serialGroup));
    m_portCombo = new QComboBox(serialGroup);
    m_portCombo->setMinimumWidth(150);
    portRow->addWidget(m_portCombo);
    m_refreshBtn = new QPushButton("↺", serialGroup);
    m_refreshBtn->setFixedWidth(28);
    portRow->addWidget(m_refreshBtn);
    portRow->addStretch();
    serialLayout->addLayout(portRow);

    auto* baudRow = new QHBoxLayout;
    baudRow->addWidget(new QLabel("Baud:", serialGroup));
    m_baudCombo = new QComboBox(serialGroup);
    m_baudCombo->addItems({"38400", "115200"});
    baudRow->addWidget(m_baudCombo);
    baudRow->addStretch();
    serialLayout->addLayout(baudRow);

    gl->addWidget(serialGroup);

    // --- Connect + status ---
    m_connectBtn = new QPushButton("Connect", group);
    gl->addWidget(m_connectBtn);

    m_statusLabel = new QLabel("Disconnected", group);
    gl->addWidget(m_statusLabel);

    root->addWidget(group);
    root->addStretch();

    scanSerialPorts();

    connect(m_scanBtn,    &QPushButton::clicked, this, &Elm327Panel::onScanClicked);
    connect(m_refreshBtn, &QPushButton::clicked, this, [this] { scanSerialPorts(); });
    connect(m_connectBtn, &QPushButton::clicked, this, &Elm327Panel::onConnectClicked);
}

void Elm327Panel::scanSerialPorts() {
    m_portCombo->clear();
    const QStringList patterns = {"rfcomm*", "ttyUSB*", "ttyACM*"};
    QDir dev("/dev");
    for (const QString& pattern : patterns) {
        for (const QString& entry : dev.entryList({pattern}, QDir::System | QDir::Files))
            m_portCombo->addItem("/dev/" + entry);
    }
}

void Elm327Panel::onScanClicked() {
    if (m_btAgent->isActive()) {
        m_btAgent->stop();
        return;
    }
    m_btList->clear();
    m_scanBtn->setText("Stop");
    m_scanLabel->setText("Scanning...");
    m_btAgent->start(QBluetoothDeviceDiscoveryAgent::ClassicMethod);
}

void Elm327Panel::onDeviceFound(const QBluetoothDeviceInfo& info) {
    QString label = info.name().isEmpty() ? "Unknown" : info.name();
    label += "   " + info.address().toString();
    auto* item = new QListWidgetItem(label, m_btList);
    item->setData(Qt::UserRole, info.address().toString());
}

void Elm327Panel::onScanFinished() {
    m_scanBtn->setText("Scan");
    const int n = m_btList->count();
    m_scanLabel->setText(QString::number(n) + (n == 1 ? " device found" : " devices found"));
}

void Elm327Panel::onConnectClicked() {
    if (m_connected) {
        m_bridge->close();
        m_connected = false;
        m_connectBtn->setText("Connect");
        m_statusLabel->setText("Disconnected");
        return;
    }

    // Prefer a selected Bluetooth device
    QListWidgetItem* btItem = m_btList->currentItem();
    if (btItem) {
        QBluetoothAddress addr(btItem->data(Qt::UserRole).toString());
        if (!addr.isNull() && m_bridge->openBluetooth(addr)) {
            m_connected = true;
            m_connectBtn->setText("Disconnect");
            return;
        }
    }

    // Fallback: serial port
    const QString port = m_portCombo->currentText();
    if (port.isEmpty()) {
        m_statusLabel->setText("No device selected");
        return;
    }
    if (m_bridge->open(port, m_baudCombo->currentText().toInt())) {
        m_connected = true;
        m_connectBtn->setText("Disconnect");
    }
}

} // namespace socketspy::gui
