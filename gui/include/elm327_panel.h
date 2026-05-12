#pragma once
#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
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
    void onRefreshClicked();

private:
    void setupUi();
    void scanPorts();

    QComboBox*    m_portCombo{nullptr};
    QComboBox*    m_baudCombo{nullptr};
    QPushButton*  m_connectBtn{nullptr};
    QPushButton*  m_refreshBtn{nullptr};
    QLabel*       m_statusLabel{nullptr};

    Elm327Bridge* m_bridge{nullptr};
    bool          m_connected{false};
};

} // namespace socketspy::gui
