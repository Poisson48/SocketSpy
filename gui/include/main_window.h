#pragma once
#include <QMainWindow>
#include <QLabel>
#include <QComboBox>
#include <QToolBar>
#include <QDockWidget>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QString>
#include <QStringList>
#include <QHash>
#include <atomic>
#include <memory>
#include "filter_panel.h"
#include "trigger_panel.h"

#pragma push_macro("signals")
#undef signals
#include "dbc_types.h"
#pragma pop_macro("signals")

#include "log_recorder.h"
#include "replay_panel.h"
#include "project.h"
#include "project_registry.h"
#include "welcome_screen.h"

namespace socketspy::gui {

class CanCapture;
class MonitorPanel;
class TransmitPanel;
class SignalGraphPanel;
class StatsPanel;
class Elm327Panel;
class SimulatorPanel;
class ScriptingPanel;
class ProtocolPanel;
class DbcBuilderPanel;
class WelcomeScreen;
class McpPanel;
class FuzzerPanel;
class DiffPanel;
class UdsPanel;
class TemporalPanel;
class HeatmapPanel;
class BisectPanel;
class RangeStatePanel;
class XcpPanel;
class DoipPanel;
class OpenDbcPanel;
class SidebarNav;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onOpenDbc();
    void loadDbcFromPath(const QString& path);
    void onExit();
    void onAnyFrameReceived();
    void onFpsTick();
    void onCaptureError(QString message);
    void onToggleMonitor(bool visible);
    void onToggleTransmit(bool visible);
    void onToggleGraph(bool visible);
    void onIfaceChanged(const QString& iface);
    void onBitrateChanged(int index);
    void onRefreshIfaces();
    void onNetChanged(const QString& path);
    void onStartRecording();
    void onStopRecording();
    void onTriggerFired();
    void onStatusTimeout();
    void onInstallUdevRules();
    void onGrantCanPermissions();
    void onNewProject();
    void onOpenProject();
    void onSaveProject();
    void onSaveProjectAs();
    void onShowProjectBrowser();
    void onSignalAliased(const QString& canonical, const QString& alias);
    void onSaveDbc();
    void onWelcomeOpenProject(const QString& path);
    void onWelcomeQuickConnect();
    void onExportMonitorCsv();
    void onExportBlf();
    void onExportMdf4();
    void onExportAsc();
    void onImportAsc();
    void onExportTrc();
    void onImportTrc();
    void onExportPcap();
    void onImportPcap();
    void onImportCsv();
    void onAddBus();
    void onRemoveBus();

private:
    void setupUi();
    void setupMenuBar();
    void setupToolBar();
    void setupCapture(const QString& iface);
    void setConnStatus(bool active);
    ProjectData collectProject() const;
    void applyProject(const ProjectData& p);

    SidebarNav*          m_sidebar{nullptr};
    WelcomeScreen*       m_welcomeScreen{nullptr};
    QLabel*              m_statusLabel{nullptr};
    QLabel*              m_connStatusLabel{nullptr};
    QLabel*              m_recLabel{nullptr};

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
    ScriptingPanel*      m_scriptPanel{nullptr};
    ProtocolPanel*       m_protocolPanel{nullptr};
    DbcBuilderPanel*     m_dbcBuilder{nullptr};
    McpPanel*            m_mcpPanel{nullptr};
    FuzzerPanel*         m_fuzzerPanel{nullptr};
    DiffPanel*           m_diffPanel{nullptr};
    UdsPanel*            m_udsPanel{nullptr};
    TemporalPanel*       m_temporalPanel{nullptr};
    HeatmapPanel*        m_heatmapPanel{nullptr};
    BisectPanel*         m_bisectPanel{nullptr};
    RangeStatePanel*     m_rangeStatePanel{nullptr};
    XcpPanel*            m_xcpPanel{nullptr};
    DoipPanel*           m_doipPanel{nullptr};
    OpenDbcPanel*        m_openDbcPanel{nullptr};

    CanCapture*          m_capture{nullptr};
    CanCapture*          m_capture2{nullptr};  // optional second bus
    QComboBox*           m_ifaceCombo{nullptr};
    QComboBox*           m_bitrateCombo{nullptr};
    QFileSystemWatcher*  m_netWatcher{nullptr};
    QTimer*              m_statusTimer{nullptr};
    QTimer*                  m_fpsTimer{nullptr};
    std::atomic<uint32_t>    m_fpsCount{0};
    double                   m_smoothFps{0.0};
    int                  m_bitrate{500000};

    std::unique_ptr<socketspy::dbc::DbcDatabase> m_dbc;
    QString     m_iface{"vcan0"};
    QStringList m_knownIfaces;
    bool        m_userPicked{false};

    LogRecorder     m_recorder;
    TriggerConfig   m_lastTriggerCfg;
    QString         m_projectPath;
    QString         m_dbcPath;
    QString         m_defaultLogPath;   // restored from ProjectData.logPath
    ProjectRegistry m_projectRegistry;
    QHash<QString, QString> m_signalAliases;
};

} // namespace socketspy::gui
