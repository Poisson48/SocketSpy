#pragma once
#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include "cancore.h"

namespace socketspy::gui {

class Elm327Bridge;

class Elm327Panel : public QWidget {
    Q_OBJECT

public:
    explicit Elm327Panel(QWidget* parent = nullptr);

signals:
    void frameReceived(socketspy::core::CanFrame frame);

private slots:
    void onConnectClicked();
    void onScanClicked();
    void onDeviceFound(const QBluetoothDeviceInfo& info);
    void onScanFinished();

private:
    void setupUi();
    void scanSerialPorts();

    // Bluetooth section
    QPushButton*  m_scanBtn{nullptr};
    QListWidget*  m_btList{nullptr};
    QLabel*       m_scanLabel{nullptr};

    // Serial section
    QComboBox*    m_portCombo{nullptr};
    QComboBox*    m_baudCombo{nullptr};
    QPushButton*  m_refreshBtn{nullptr};

    // Shared
    QPushButton*  m_connectBtn{nullptr};
    QLabel*       m_statusLabel{nullptr};

    Elm327Bridge*                    m_bridge{nullptr};
    QBluetoothDeviceDiscoveryAgent*  m_btAgent{nullptr};
    bool                             m_connected{false};
};

} // namespace socketspy::gui
