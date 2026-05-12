#pragma once
#include <QMainWindow>
#include <QTabWidget>
#include <QLabel>
#include <QComboBox>
#include <QToolBar>
#include <QString>
#include <QStringList>
#include <memory>

// Temporarily suppress the Qt `signals` keyword macro so that
// socketspy::dbc::Message::signals (a data member) parses correctly.
#pragma push_macro("signals")
#undef signals
#include "dbc_types.h"
#pragma pop_macro("signals")

namespace socketspy::gui {

class CanCapture;
class MonitorPanel;
class TransmitPanel;
class SignalGraphPanel;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onOpenDbc();
    void onExit();
    void onStatsUpdated(uint64_t fps);
    void onCaptureError(QString message);
    void onToggleMonitor(bool visible);
    void onToggleTransmit(bool visible);
    void onToggleGraph(bool visible);
    void onIfaceChanged(const QString& iface);
    void onRefreshIfaces();

private:
    void setupUi();
    void setupMenuBar();
    void setupToolBar();
    void setupCapture(const QString& iface);
    static QStringList scanCanIfaces();

    QTabWidget*       m_tabs{nullptr};
    QLabel*           m_statusLabel{nullptr};

    MonitorPanel*     m_monitor{nullptr};
    TransmitPanel*    m_transmit{nullptr};
    SignalGraphPanel* m_graph{nullptr};

    CanCapture*       m_capture{nullptr};
    QComboBox*        m_ifaceCombo{nullptr};

    std::unique_ptr<socketspy::dbc::DbcDatabase> m_dbc;
    QString m_iface{"vcan0"};
};

} // namespace socketspy::gui
