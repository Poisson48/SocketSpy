#pragma once
#include <QMainWindow>
#include <QTabWidget>
#include <QLabel>
#include <QComboBox>
#include <QToolBar>
#include <QDockWidget>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QString>
#include <QStringList>
#include <memory>
#include "filter_panel.h"
#include "trigger_panel.h"

#pragma push_macro("signals")
#undef signals
#include "dbc_types.h"
#pragma pop_macro("signals")

#include "log_recorder.h"
#include "replay_panel.h"

namespace socketspy::gui {

class CanCapture;
class MonitorPanel;
class TransmitPanel;
class SignalGraphPanel;
class StatsPanel;
class Elm327Panel;
class SimulatorPanel;

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
    void onNetChanged(const QString& path);
    void onStartRecording();
    void onStopRecording();
    void onTriggerFired();
    void onStatusTimeout();

private:
    void setupUi();
    void setupMenuBar();
    void setupToolBar();
    void setupCapture(const QString& iface);
    void setConnStatus(bool active);

    QTabWidget*          m_tabs{nullptr};
    QLabel*              m_statusLabel{nullptr};
    QLabel*              m_connStatusLabel{nullptr};

    MonitorPanel*        m_monitor{nullptr};
    TransmitPanel*       m_transmit{nullptr};
    SignalGraphPanel*    m_graph{nullptr};
    ReplayPanel*         m_replay{nullptr};
    FilterPanel*         m_filterPanel{nullptr};
    QDockWidget*         m_filterDock{nullptr};
    TriggerPanel*        m_triggerPanel{nullptr};
    QDockWidget*         m_triggerDock{nullptr};
    StatsPanel*          m_stats{nullptr};
    Elm327Panel*         m_elm327Panel{nullptr};
    SimulatorPanel*      m_simulator{nullptr};

    CanCapture*          m_capture{nullptr};
    QComboBox*           m_ifaceCombo{nullptr};
    QFileSystemWatcher*  m_netWatcher{nullptr};
    QTimer*              m_statusTimer{nullptr};

    std::unique_ptr<socketspy::dbc::DbcDatabase> m_dbc;
    QString     m_iface{"vcan0"};
    QStringList m_knownIfaces;
    bool        m_userPicked{false};

    LogRecorder   m_recorder;
    TriggerConfig m_lastTriggerCfg;
};

} // namespace socketspy::gui
