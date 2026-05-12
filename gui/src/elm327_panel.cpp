#include "elm327_panel.h"
#include "elm327_bridge.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDir>
#include <QStringList>

namespace socketspy::gui {

Elm327Panel::Elm327Panel(QWidget* parent)
    : QWidget(parent)
    , m_bridge(new Elm327Bridge(this))
{
    setupUi();

    connect(m_bridge, &Elm327Bridge::frameReceived,
            this,     &Elm327Panel::frameReceived);
    connect(m_bridge, &Elm327Bridge::connectionError,
            this, [this](const QString& err) {
                m_statusLabel->setText("Error: " + err);
                m_connectBtn->setText("Connect");
                m_connected = false;
            });
    connect(m_bridge, &Elm327Bridge::statusChanged,
            m_statusLabel, &QLabel::setText);
}

void Elm327Panel::setupUi() {
    auto* root = new QVBoxLayout(this);

    auto* group = new QGroupBox("OBD2 / ELM327 Bluetooth", this);
    auto* gl = new QVBoxLayout(group);

    auto* row1 = new QHBoxLayout;
    row1->addWidget(new QLabel("Port:", this));
    m_portCombo = new QComboBox(this);
    m_portCombo->setMinimumWidth(160);
    row1->addWidget(m_portCombo);

    m_refreshBtn = new QPushButton("Refresh", this);
    m_refreshBtn->setFixedWidth(70);
    row1->addWidget(m_refreshBtn);
    row1->addStretch();
    gl->addLayout(row1);

    auto* row2 = new QHBoxLayout;
    row2->addWidget(new QLabel("Baud:", this));
    m_baudCombo = new QComboBox(this);
    m_baudCombo->addItems({"38400", "115200"});
    row2->addWidget(m_baudCombo);
    row2->addStretch();
    gl->addLayout(row2);

    auto* row3 = new QHBoxLayout;
    m_connectBtn = new QPushButton("Connect", this);
    row3->addWidget(m_connectBtn);
    row3->addStretch();
    gl->addLayout(row3);

    m_statusLabel = new QLabel("Disconnected", this);
    gl->addWidget(m_statusLabel);

    root->addWidget(group);
    root->addStretch();

    scanPorts();

    connect(m_connectBtn, &QPushButton::clicked, this, &Elm327Panel::onConnectClicked);
    connect(m_refreshBtn, &QPushButton::clicked, this, &Elm327Panel::onRefreshClicked);
}

void Elm327Panel::scanPorts() {
    m_portCombo->clear();

    QStringList patterns = {"rfcomm*", "ttyUSB*"};
    QDir dev("/dev");
    for (const QString& pattern : patterns) {
        const auto entries = dev.entryList({pattern}, QDir::System | QDir::Files);
        for (const QString& entry : entries)
            m_portCombo->addItem("/dev/" + entry);
    }

    if (m_portCombo->count() == 0)
        m_portCombo->addItem("/dev/rfcomm0");
}

void Elm327Panel::onRefreshClicked() {
    scanPorts();
}

void Elm327Panel::onConnectClicked() {
    if (m_connected) {
        m_bridge->close();
        m_connected = false;
        m_connectBtn->setText("Connect");
        m_statusLabel->setText("Disconnected");
        return;
    }

    const QString port = m_portCombo->currentText();
    const int baud     = m_baudCombo->currentText().toInt();

    if (m_bridge->open(port, baud)) {
        m_connected = true;
        m_connectBtn->setText("Disconnect");
    }
}

} // namespace socketspy::gui
