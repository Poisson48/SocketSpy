#include "main_window.h"
#include "can_capture.h"
#include "monitor_panel.h"
#include "transmit_panel.h"
#include "signal_graph.h"
#include "log_recorder.h"
#include "replay_panel.h"
#include "filter_panel.h"
#include "stats_panel.h"
#include "trigger_panel.h"
#include "elm327_panel.h"
#include "simulator_panel.h"
#include "scripting_panel.h"
#include "protocol_panel.h"
#include "dbc_builder_panel.h"
#include "welcome_screen.h"
#include "mcp_panel.h"
#include "fuzzer_panel.h"
#include "diff_panel.h"
#include "uds_panel.h"
#include "temporal_panel.h"
#include "heatmap_panel.h"
#include "bisect_panel.h"
#include "range_state_panel.h"
#include "signal_detective_panel.h"
#include "xcp_panel.h"
#include "doip_panel.h"
#include "opendbc_panel.h"
#include "sidebar_nav.h"
#include "iface_detector.h"
#include "can_iface_config.h"

// Permanently undef Qt's `signals` macro so we can access dbc::Message::signals.
#include "dbc_compat.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QStatusBar>
#include <QPushButton>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QShortcut>
#include <QMessageBox>
#include <QInputDialog>
#include <QSettings>

using namespace socketspy::dbc;

namespace socketspy::gui {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    m_dbc = std::make_unique<DbcDatabase>();
    setupUi();
    setupMenuBar();
    setupToolBar();
    m_knownIfaces = IfaceDetector::scanCanIfaces();
    setupCapture(m_iface);
}

MainWindow::~MainWindow() {
    if (m_capture) {
        m_capture->stop();
        m_capture->wait();
    }
    if (m_capture2) {
        m_capture2->stop();
        m_capture2->wait();
    }
}

void MainWindow::setupUi() {
    setWindowTitle(QString("SocketSpy v%1").arg(APP_VERSION));
    resize(1200, 700);
    setMinimumSize(800, 500);

    m_welcomeScreen = new WelcomeScreen(m_projectRegistry, this);
    connectWelcomeSignals();

    m_monitor   = new MonitorPanel(this);
    m_transmit  = new TransmitPanel(this);
    m_graph     = new SignalGraphPanel(this);
    m_stats     = new StatsPanel(this);
    m_replay    = new ReplayPanel(this);
    m_elm327Panel = new Elm327Panel(this);
    m_simulator     = new SimulatorPanel(this);
    m_scriptPanel   = new ScriptingPanel(this);
    m_protocolPanel = new ProtocolPanel(this);
    m_dbcBuilder    = new DbcBuilderPanel(this);
    m_mcpPanel      = new McpPanel(this);
    m_fuzzerPanel     = new FuzzerPanel(this);
    m_diffPanel       = new DiffPanel(this);
    m_udsPanel        = new UdsPanel(this);
    m_temporalPanel   = new TemporalPanel(this);
    m_heatmapPanel    = new HeatmapPanel(this);
    m_bisectPanel     = new BisectPanel(this);
    m_rangeStatePanel    = new RangeStatePanel(this);
    m_signalDetective   = new SignalDetectivePanel(this);
    m_xcpPanel        = new XcpPanel(this);
    m_doipPanel       = new DoipPanel(this);
    m_openDbcPanel    = new OpenDbcPanel(this);
    connect(m_openDbcPanel, &OpenDbcPanel::dbcFileSelected,
            this,           &MainWindow::loadDbcFromPath);

    m_sidebar = new SidebarNav(this);
    m_sidebar->addPanel("\xe2\x8a\x9e",  tr("Monitor"),    m_monitor);
    m_sidebar->addPanel("\xe2\x86\x91",  tr("Transmit"),   m_transmit);
    m_sidebar->addPanel("\xe2\x88\xbf",  tr("Graph"),      m_graph);
    m_sidebar->addPanel("\xe2\x96\xb6",  tr("Replay"),     m_replay);
    m_sidebar->addPanel("\xe2\x89\xa1",  tr("Stats"),      m_stats);
    m_sidebar->addPanel("\xe2\x9a\xa1",  tr("OBD2"),       m_elm327Panel);
    m_sidebar->addPanel("\xe2\x9a\x99",  tr("Simulator"),  m_simulator);
    m_sidebar->addPanel("\xce\xbb",      tr("Scripts"),    m_scriptPanel);
    m_sidebar->addPanel("\xe2\x8a\x95",  tr("Protocols"),  m_protocolPanel);
    m_sidebar->addPanel("\xe2\x9c\x8f",  tr("DBC Bld"),    m_dbcBuilder);
    m_sidebar->addPanel("\xe2\x9a\x99",  tr("MCP"),        m_mcpPanel);
    m_sidebar->addPanel("\xe2\x9a\xa1",  tr("Fuzzer"),     m_fuzzerPanel);
    m_sidebar->addPanel("\xe2\x89\xa0",  tr("Diff"),       m_diffPanel);
    m_sidebar->addPanel("\xf0\x9f\x94\xa7", tr("UDS"),     m_udsPanel);
    m_sidebar->addPanel("\xe2\x8f\xb1",  tr("Temporal"),   m_temporalPanel);
    m_sidebar->addPanel("\xe2\xac\x9c",  tr("Heatmap"),    m_heatmapPanel);
    m_sidebar->addPanel("\xe2\xac\xa1",  tr("Bisect"),     m_bisectPanel);
    m_sidebar->addPanel("\xe2\x8a\x99",  tr("RngScan"),    m_rangeStatePanel);
    m_sidebar->addPanel("\xf0\x9f\x94\x8d", tr("Detect"),     m_signalDetective);
    m_sidebar->addPanel("X",             tr("XCP"),         m_xcpPanel);
    m_sidebar->addPanel("\xe2\xac\xa6",  tr("DoIP"),        m_doipPanel);
    m_sidebar->addPanel("\xe2\xac\x87",  tr("OpenDBC"),     m_openDbcPanel);

    auto* central = new QWidget(this);
    auto* hbox = new QHBoxLayout(central);
    hbox->setContentsMargins(0, 0, 0, 0);
    hbox->setSpacing(0);
    hbox->addWidget(m_sidebar);
    hbox->addWidget(m_sidebar->stack(), 1);
    setCentralWidget(central);

    m_recLabel = new QLabel(this);
    m_recLabel->setObjectName("recLabel");
    m_recLabel->hide();
    statusBar()->addPermanentWidget(m_recLabel);

    m_statusLabel = new QLabel(m_iface + "  \xc2\xb7  0 fps", this);
    m_statusLabel->setObjectName("statusFps");
    statusBar()->addPermanentWidget(m_statusLabel);

    m_fpsTimer = new QTimer(this);
    m_fpsTimer->setInterval(1000);
    connect(m_fpsTimer, &QTimer::timeout, this, &MainWindow::onFpsTick);
    m_fpsTimer->start();

    connect(m_monitor, &MonitorPanel::signalDoubleClicked,
            m_graph,   &SignalGraphPanel::addSignal);
    connect(m_monitor, &MonitorPanel::frameGraphRequested,
            m_graph,   &SignalGraphPanel::addFrameSignals);
    connect(m_replay,  &ReplayPanel::replayFrame,
            m_monitor, &MonitorPanel::onFrameReceived);
    connect(m_replay,  &ReplayPanel::replayFrame,
            m_graph,   &SignalGraphPanel::onFrameReceived);
    connect(m_replay,  &ReplayPanel::replayFrame,
            m_stats,   &StatsPanel::onFrameReceived);
    connect(m_replay,  &ReplayPanel::replayFrame,
            m_protocolPanel, &ProtocolPanel::onFrameReceived);
    connect(m_graph, &SignalGraphPanel::signalAliased,
            this,    &MainWindow::onSignalAliased);

    auto wireFrames = [&](auto* src, auto sig) {
        connect(src, sig, m_monitor,       &MonitorPanel::onFrameReceived);
        connect(src, sig, m_graph,         &SignalGraphPanel::onFrameReceived);
        connect(src, sig, m_stats,         &StatsPanel::onFrameReceived);
        connect(src, sig, m_protocolPanel, &ProtocolPanel::onFrameReceived);
        connect(src, sig, m_dbcBuilder,      &DbcBuilderPanel::onFrameReceived);
        connect(src, sig, m_scriptPanel,     &ScriptingPanel::onFrameReceived);
        connect(src, sig, m_temporalPanel,   &TemporalPanel::onFrameReceived);
        connect(src, sig, m_heatmapPanel,    &HeatmapPanel::onFrameReceived);
        connect(src, sig, m_rangeStatePanel, &RangeStatePanel::onFrameReceived);
        connect(src, sig, m_signalDetective,  &SignalDetectivePanel::onFrameReceived);
    };
    wireFrames(m_elm327Panel, &Elm327Panel::frameReceived);
    wireFrames(m_simulator,   &SimulatorPanel::frameGenerated);
    connect(m_elm327Panel, &Elm327Panel::frameReceived,
            this, &MainWindow::onAnyFrameReceived, Qt::DirectConnection);
    connect(m_simulator,   &SimulatorPanel::frameGenerated,
            this, &MainWindow::onAnyFrameReceived, Qt::DirectConnection);

    connect(m_dbcBuilder, &DbcBuilderPanel::dbcUpdated,
            this, [this](const socketspy::dbc::DbcDatabase& db) {
                *m_dbc = db;
                m_monitor->onDbcLoaded(db);
                m_graph->onDbcLoaded(db);
                m_stats->onDbcLoaded(db);
            });


    m_filterPanel = new FilterPanel(this);
    m_filterDock  = new QDockWidget(tr("Filter"), this);
    m_filterDock->setWidget(m_filterPanel);
    m_filterDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::RightDockWidgetArea, m_filterDock);
    m_filterDock->hide();
    connect(m_filterPanel, &FilterPanel::filterChanged,
            m_monitor,     &MonitorPanel::onFilterChanged);

    m_triggerPanel = new TriggerPanel(this);
    m_triggerDock  = new QDockWidget(tr("Trigger Capture"), this);
    m_triggerDock->setWidget(m_triggerPanel);
    m_triggerDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::RightDockWidgetArea, m_triggerDock);
    m_triggerDock->hide();

    // Ctrl+1..9: switch to nth panel
    for (int n = 1; n <= 9; ++n) {
        auto* sc = new QShortcut(QKeySequence(Qt::CTRL | static_cast<Qt::Key>(Qt::Key_0 + n)), this);
        connect(sc, &QShortcut::activated, this, [this, n]() {
            auto* w = m_sidebar->stack()->widget(n - 1);
            if (w) m_sidebar->showPanel(w);
        });
    }
}

void MainWindow::setupMenuBar() {
    auto* fileMenu = menuBar()->addMenu(tr("&File"));
    auto addF = [&](const QString& lbl, auto slot, QKeySequence k = {}) {
        auto* a = fileMenu->addAction(lbl); if (!k.isEmpty()) a->setShortcut(k);
        connect(a, &QAction::triggered, this, slot);
    };
    addF(tr("&Open DBC…"),         &MainWindow::onOpenDbc);
    addF(tr("&Save DBC…"),         &MainWindow::onSaveDbc);
    fileMenu->addSeparator();
    addF(tr("&New Project"),       &MainWindow::onNewProject,     QKeySequence::New);
    addF(tr("&Open Project…"),     &MainWindow::onOpenProject,    QKeySequence::Open);
    addF(tr("&Save Project"),      &MainWindow::onSaveProject,    QKeySequence::Save);
    addF(tr("Save Project &As…"),  &MainWindow::onSaveProjectAs);
    fileMenu->addSeparator();
    addF(tr("Export Monitor &CSV…"), &MainWindow::onExportMonitorCsv, QKeySequence("Ctrl+Shift+E"));
    addF(tr("Export BLF…"),          &MainWindow::onExportBlf);
    addF(tr("Export MDF4…"),         &MainWindow::onExportMdf4);
    addF(tr("Export ASC…"),          &MainWindow::onExportAsc);
    addF(tr("Export TRC…"),          &MainWindow::onExportTrc);
    addF(tr("Export PCAP…"),         &MainWindow::onExportPcap);
    fileMenu->addSeparator();
    addF(tr("Import ASC…"),          &MainWindow::onImportAsc);
    addF(tr("Import TRC…"),          &MainWindow::onImportTrc);
    addF(tr("Import PCAP…"),         &MainWindow::onImportPcap);
    addF(tr("Import CSV…"),          &MainWindow::onImportCsv, QKeySequence("Ctrl+Shift+I"));
    fileMenu->addSeparator();
    addF(tr("E&xit"),              &MainWindow::onExit,           QKeySequence::Quit);

    auto* viewMenu = menuBar()->addMenu(tr("&View"));
    auto addV = [&](const QString& lbl, auto slot) {
        auto* a = viewMenu->addAction(lbl); a->setCheckable(true); a->setChecked(true);
        connect(a, &QAction::toggled, this, slot);
    };
    addV(tr("Monitor Panel"),  &MainWindow::onToggleMonitor);
    addV(tr("Transmit Panel"), &MainWindow::onToggleTransmit);
    addV(tr("Signal Graph"),   &MainWindow::onToggleGraph);
    viewMenu->addSeparator();

    auto addDockToggle = [&](const QString& label, const QString& shortcut, QDockWidget* dock) {
        auto* act = viewMenu->addAction(label);
        act->setShortcut(QKeySequence(shortcut));
        act->setCheckable(true); act->setChecked(false);
        connect(act,  &QAction::toggled,           dock, &QDockWidget::setVisible);
        connect(dock, &QDockWidget::visibilityChanged, act, &QAction::setChecked);
    };
    addDockToggle(tr("Show Filter Panel"),  "Ctrl+F", m_filterDock);
    addDockToggle(tr("Show Trigger Panel"), "Ctrl+T", m_triggerDock);

    auto* toolsMenu  = menuBar()->addMenu(tr("&Tools"));
    auto* rulesAct   = toolsMenu->addAction(tr("Install udev rules…"));
    auto* permsAct   = toolsMenu->addAction(tr("Configure CAN permissions (one-time)…"));
    connect(rulesAct,  &QAction::triggered, this, &MainWindow::onInstallUdevRules);
    connect(permsAct,  &QAction::triggered, this, &MainWindow::onGrantCanPermissions);
    toolsMenu->addSeparator();
    auto* addBusAct    = toolsMenu->addAction(tr("+ Add Second Bus…"));
    auto* removeBusAct = toolsMenu->addAction(tr("- Remove Second Bus"));
    connect(addBusAct,    &QAction::triggered, this, &MainWindow::onAddBus);
    connect(removeBusAct, &QAction::triggered, this, &MainWindow::onRemoveBus);

    auto* helpMenu = menuBar()->addMenu(tr("&Help"));

    // Language submenu
    auto* langMenu = helpMenu->addMenu(tr("Language"));
    QSettings settings;
    const QString curLang = settings.value("language", "en").toString();

    auto addLang = [&](const QString& label, const QString& code) {
        auto* act = langMenu->addAction(label);
        act->setCheckable(true);
        act->setChecked(curLang == code);
        connect(act, &QAction::triggered, this, [this, code]() {
            QSettings s;
            s.setValue("language", code);
            QMessageBox::information(this,
                tr("Language changed"),
                tr("Please restart SocketSpy to apply the new language."));
        });
    };
    addLang("English",  "en");
    addLang("Français", "fr");

    helpMenu->addSeparator();
    auto* updateAct = helpMenu->addAction(tr("Check for updates…"));
    connect(updateAct, &QAction::triggered, this, &MainWindow::onCheckForUpdates);

    helpMenu->addSeparator();
    auto* aboutAct = helpMenu->addAction(tr("About SocketSpy"));
    connect(aboutAct, &QAction::triggered, this, [this]() {
        QMessageBox::about(this,
            tr("About SocketSpy"),
            QString("<b>SocketSpy v%1</b><br>"
                    "%2<br><br>"
                    "%3<br>"
                    "Built on Linux SocketCAN")
            .arg(APP_VERSION,
                 tr("Linux CAN bus analysis platform"),
                 tr("100% local · no telemetry · MIT license")));
    });
}

void MainWindow::setupCapture(const QString& iface) {
    if (m_capture) {
        m_capture->stop();
        m_capture->wait();
        delete m_capture;
    }
    m_capture = new CanCapture(iface, this);

    connect(m_capture, &CanCapture::frameReceived, m_monitor,       &MonitorPanel::onFrameReceived);
    connect(m_capture, &CanCapture::frameReceived, m_graph,         &SignalGraphPanel::onFrameReceived);
    connect(m_capture, &CanCapture::frameReceived, m_stats,         &StatsPanel::onFrameReceived);
    connect(m_capture, &CanCapture::frameReceived, m_protocolPanel, &ProtocolPanel::onFrameReceived);
    connect(m_capture, &CanCapture::frameReceived, m_dbcBuilder,      &DbcBuilderPanel::onFrameReceived);
    connect(m_capture, &CanCapture::frameReceived, m_scriptPanel,     &ScriptingPanel::onFrameReceived);
    connect(m_capture, &CanCapture::frameReceived, m_temporalPanel,   &TemporalPanel::onFrameReceived);
    connect(m_capture, &CanCapture::frameReceived, m_heatmapPanel,    &HeatmapPanel::onFrameReceived);
    connect(m_capture, &CanCapture::frameReceived, m_rangeStatePanel, &RangeStatePanel::onFrameReceived);
    connect(m_capture, &CanCapture::frameReceived, m_signalDetective,  &SignalDetectivePanel::onFrameReceived);
    connect(m_capture, &CanCapture::frameReceived, this,              &MainWindow::onAnyFrameReceived,
            Qt::DirectConnection);
    connect(m_capture, &CanCapture::frameReceived, m_simulator, &SimulatorPanel::onFrameReceived);
    connect(m_capture, &CanCapture::errorOccurred, this,      &MainWindow::onCaptureError);
    connect(m_capture, &CanCapture::triggerFired,  this,      &MainWindow::onTriggerFired);
    connect(m_triggerPanel, &TriggerPanel::triggerConfigChanged, m_capture, &CanCapture::setTrigger);
    connect(m_triggerPanel, &TriggerPanel::triggerConfigChanged,
            this, [this](TriggerConfig cfg) { m_lastTriggerCfg = cfg; });

    m_capture->start();
}

void MainWindow::setupToolBar() {
    auto* tb = addToolBar(tr("Interface"));
    tb->setMovable(false);
    tb->addWidget(new QLabel("  " + tr("Interface: "), this));

    m_ifaceCombo = new QComboBox(this);
    m_ifaceCombo->setMinimumWidth(120);
    m_knownIfaces = IfaceDetector::scanCanIfaces();
    m_ifaceCombo->addItems(m_knownIfaces);
    int idx = m_ifaceCombo->findText(m_iface);
    if (idx >= 0) m_ifaceCombo->setCurrentIndex(idx);
    tb->addWidget(m_ifaceCombo);
    auto* refreshBtn = new QPushButton(QString::fromUtf8("↺"), this);
    refreshBtn->setObjectName("refreshBtn");
    refreshBtn->setFixedWidth(28); refreshBtn->setToolTip(tr("Refresh interface list"));
    tb->addWidget(refreshBtn); tb->addSeparator();

    tb->addWidget(new QLabel(tr("Bitrate: "), this));
    m_bitrateCombo = new QComboBox(this);
    for (auto [label, val] : {std::pair{"125 kbit/s",125000},{"250 kbit/s",250000},
                                         {"500 kbit/s",500000},{"800 kbit/s",800000},
                                         {"1000 kbit/s",1000000}})
        m_bitrateCombo->addItem(label, val);
    m_bitrateCombo->addItem(tr("Custom\xe2\x80\xa6"), -1);
    m_bitrateCombo->setCurrentIndex(2);
    tb->addWidget(m_bitrateCombo); tb->addSeparator();

    m_connStatusLabel = new QLabel(QString::fromUtf8("●  \xe2\x80\x93 \xe2\x80\x93"), this);
    m_connStatusLabel->setStyleSheet("color: #4b5563; font-size: 11px; font-weight:700; letter-spacing:1px;");
    m_connStatusLabel->setToolTip(tr("No traffic received"));
    tb->addWidget(m_connStatusLabel); tb->addSeparator();

    connect(m_ifaceCombo,   &QComboBox::currentTextChanged, this,        &MainWindow::onIfaceChanged);
    connect(m_ifaceCombo,   &QComboBox::currentTextChanged, m_transmit, &TransmitPanel::setCurrentIface);
    connect(m_bitrateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onBitrateChanged);
    connect(refreshBtn,     &QPushButton::clicked,                     this, &MainWindow::onRefreshIfaces);

    m_netWatcher = new QFileSystemWatcher(this);
    m_netWatcher->addPath("/sys/class/net");
    connect(m_netWatcher, &QFileSystemWatcher::directoryChanged,
            this, &MainWindow::onNetChanged);

    m_statusTimer = new QTimer(this);
    m_statusTimer->setSingleShot(true);
    m_statusTimer->setInterval(2000);
    connect(m_statusTimer, &QTimer::timeout, this, &MainWindow::onStatusTimeout);

    auto* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    tb->addWidget(spacer);

    auto* recordBtn  = new QPushButton(tr("Record"),   this);
    auto* stopRecBtn = new QPushButton(tr("Stop Rec"), this);
    recordBtn->setObjectName("recordBtn");
    stopRecBtn->setObjectName("stopRecBtn");
    recordBtn->setToolTip(tr("Start recording to .log file"));
    stopRecBtn->setToolTip(tr("Stop recording"));
    stopRecBtn->setEnabled(false);
    tb->addWidget(recordBtn);
    tb->addWidget(stopRecBtn);

    connect(recordBtn,  &QPushButton::clicked, this, [this, recordBtn, stopRecBtn]() {
        onStartRecording();
        if (m_recorder.isOpen()) { recordBtn->setEnabled(false); stopRecBtn->setEnabled(true); }
    });
    connect(stopRecBtn, &QPushButton::clicked, this, [this, recordBtn, stopRecBtn]() {
        onStopRecording();
        recordBtn->setEnabled(true); stopRecBtn->setEnabled(false);
    });
}

void MainWindow::onNetChanged(const QString&) {
    const QStringList before = m_knownIfaces;
    onRefreshIfaces();
    const QStringList after = IfaceDetector::scanCanIfaces();
    if (!m_userPicked || m_iface == "vcan0") {
        for (const QString& iface : after) {
            if (!before.contains(iface) && !iface.startsWith("slcan:")) {
                m_ifaceCombo->setCurrentText(iface);
                return;
            }
        }
    }
}

void MainWindow::onBitrateChanged(int index) {
    const int val = m_bitrateCombo->itemData(index).toInt();
    if (val == -1) {
        // Custom entry: prompt user for a free bitrate value
        bool ok = false;
        const int custom = QInputDialog::getInt(this, tr("Custom bitrate"),
            tr("Enter bitrate (bit/s):"), m_bitrate, 1000, 8000000, 1000, &ok);
        if (!ok) {
            // Revert to the previously selected item without re-triggering this slot
            m_bitrateCombo->blockSignals(true);
            m_bitrateCombo->setCurrentIndex(m_bitrateCombo->findData(m_bitrate));
            m_bitrateCombo->blockSignals(false);
            return;
        }
        m_bitrate = custom;
    } else {
        m_bitrate = val;
    }
    m_stats->setBitrate(m_bitrate);
    if (!canIfaceIsVirtual(m_iface) && !m_iface.startsWith("slcan:")) {
        if (canIfaceApply(m_iface, m_bitrate)) setupCapture(m_iface);
        else statusBar()->showMessage(tr("Error: cannot configure %1").arg(m_iface), 5000);
    }
    m_bitrateCombo->setEnabled(!canIfaceIsVirtual(m_iface));
}

void MainWindow::onIfaceChanged(const QString& iface) {
    if (iface == m_iface) return;
    m_userPicked = (iface != "vcan0");
    m_iface = iface;
    m_fpsCount = 0;
    m_smoothFps = 0.0;
    m_statusLabel->setText(m_iface + "  \xc2\xb7  0 fps");
    setConnStatus(false);
    m_bitrateCombo->setEnabled(!canIfaceIsVirtual(iface));

    if (iface.startsWith("slcan:")) {
        statusBar()->showMessage(tr("Configuring slcan on %1…").arg(iface.mid(6)), 0);
        auto [ok, slIface] = canSlcanSetup(iface.mid(6), m_bitrate);
        if (ok) { m_iface = slIface; setupCapture(m_iface); }
        else    { statusBar()->showMessage(tr("Error: cannot configure %1").arg(iface.mid(6)), 8000); }
        return;
    }
    if (!canIfaceIsVirtual(iface))
        canIfaceApply(iface, m_bitrate);
    setupCapture(m_iface);
}

void MainWindow::onRefreshIfaces() {
    const QString current = m_ifaceCombo->currentText();
    m_knownIfaces = IfaceDetector::scanCanIfaces();
    m_ifaceCombo->blockSignals(true);
    m_ifaceCombo->clear();
    m_ifaceCombo->addItems(m_knownIfaces);
    int idx = m_ifaceCombo->findText(current);
    m_ifaceCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    m_ifaceCombo->blockSignals(false);
    if (m_ifaceCombo->currentText() != current)
        onIfaceChanged(m_ifaceCombo->currentText());
}

void MainWindow::onAnyFrameReceived() {
    ++m_fpsCount;
}

void MainWindow::onFpsTick() {
    constexpr double alpha = 0.3;
    const uint32_t count = m_fpsCount.exchange(0);
    m_smoothFps = alpha * static_cast<double>(count) + (1.0 - alpha) * m_smoothFps;
    const QString txt = m_iface + "  \xc2\xb7  " + QString::number(qRound(m_smoothFps)) + " fps";
    m_statusLabel->setText(txt);
    if (m_smoothFps > 0.5) { m_statusTimer->stop(); setConnStatus(true); }
    else if (!m_statusTimer->isActive()) { m_statusTimer->start(); }
}

void MainWindow::onStatusTimeout()               { setConnStatus(false); }
void MainWindow::onCaptureError(QString message) { statusBar()->showMessage(tr("Capture error: ") + message, 8000); }
void MainWindow::onExit()                        { close(); }

void MainWindow::onSignalAliased(const QString& canonical, const QString& alias) {
    m_signalAliases[canonical] = alias;
    m_monitor->setAliases(m_signalAliases);
}

void MainWindow::connectWelcomeSignals() {
    connect(m_welcomeScreen, &WelcomeScreen::newProjectRequested,
            this,            &MainWindow::onNewProject);
    connect(m_welcomeScreen, &WelcomeScreen::openProjectRequested,
            this,            &MainWindow::onWelcomeOpenProject);
    connect(m_welcomeScreen, &WelcomeScreen::openDbcRequested,
            this,            &MainWindow::onOpenDbc);
    connect(m_welcomeScreen, &WelcomeScreen::quickConnectRequested,
            this,            &MainWindow::onWelcomeQuickConnect);
    connect(m_welcomeScreen, &WelcomeScreen::showSimulatorRequested,
            this, [this]() { m_sidebar->showPanel(m_simulator); });
    connect(m_welcomeScreen, &WelcomeScreen::showMonitorRequested,
            this, [this]() { m_sidebar->showPanel(m_monitor); });
}

void MainWindow::showWelcome() {
    m_welcomeScreen->refreshRecentProjects();
    m_welcomeScreen->show();
}

} // namespace socketspy::gui
