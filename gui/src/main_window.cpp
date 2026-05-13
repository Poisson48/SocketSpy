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
#include "iface_detector.h"
#include "can_iface_config.h"

#pragma push_macro("signals")
#undef signals
#include "dbc_parser.h"
#pragma pop_macro("signals")

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QStatusBar>
#include <QPushButton>
#include <QDockWidget>
#include <QKeySequence>

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
}

void MainWindow::setupUi() {
    setWindowTitle("SocketSpy");
    resize(1200, 700);

    m_tabs = new QTabWidget(this);
    setCentralWidget(m_tabs);

    m_monitor   = new MonitorPanel(this);
    m_transmit  = new TransmitPanel(this);
    m_graph     = new SignalGraphPanel(this);
    m_stats     = new StatsPanel(this);
    m_replay    = new ReplayPanel(this);
    m_elm327Panel = new Elm327Panel(this);
    m_simulator = new SimulatorPanel(this);

    m_tabs->addTab(m_monitor,    "Monitor");
    m_tabs->addTab(m_transmit,   "Transmit");
    m_tabs->addTab(m_graph,      "Signal Graph");
    m_tabs->addTab(m_replay,     "Replay");
    m_tabs->addTab(m_stats,      "Statistics");
    m_tabs->addTab(m_elm327Panel, "OBD2/BT");
    m_tabs->addTab(m_simulator,  "Simulator");

    m_statusLabel = new QLabel("Interface: " + m_iface + "  |  0 fps", this);
    statusBar()->addPermanentWidget(m_statusLabel);

    connect(m_monitor, &MonitorPanel::signalDoubleClicked,
            m_graph,   &SignalGraphPanel::addSignal);
    connect(m_monitor, &MonitorPanel::frameGraphRequested,
            m_graph,   &SignalGraphPanel::addFrameSignals);
    connect(m_replay,  &ReplayPanel::replayFrame,
            m_monitor, &MonitorPanel::onFrameReceived);

    auto wireFrames = [&](auto* src, auto sig) {
        connect(src, sig, m_monitor, &MonitorPanel::onFrameReceived);
        connect(src, sig, m_graph,   &SignalGraphPanel::onFrameReceived);
        connect(src, sig, m_stats,   &StatsPanel::onFrameReceived);
    };
    wireFrames(m_elm327Panel, &Elm327Panel::frameReceived);
    wireFrames(m_simulator,   &SimulatorPanel::frameGenerated);

    m_filterPanel = new FilterPanel(this);
    m_filterDock  = new QDockWidget("Filter", this);
    m_filterDock->setWidget(m_filterPanel);
    m_filterDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::RightDockWidgetArea, m_filterDock);
    m_filterDock->hide();
    connect(m_filterPanel, &FilterPanel::filterChanged,
            m_monitor,     &MonitorPanel::onFilterChanged);

    m_triggerPanel = new TriggerPanel(this);
    m_triggerDock  = new QDockWidget("Trigger Capture", this);
    m_triggerDock->setWidget(m_triggerPanel);
    m_triggerDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::RightDockWidgetArea, m_triggerDock);
    m_triggerDock->hide();
}

void MainWindow::setupMenuBar() {
    auto* fileMenu = menuBar()->addMenu("&File");
    auto addF = [&](const QString& lbl, auto slot, QKeySequence k = {}) {
        auto* a = fileMenu->addAction(lbl); if (!k.isEmpty()) a->setShortcut(k);
        connect(a, &QAction::triggered, this, slot);
    };
    addF("&Open DBC…",         &MainWindow::onOpenDbc);
    fileMenu->addSeparator();
    addF("&New Project",       &MainWindow::onNewProject,     QKeySequence::New);
    addF("&Open Project…",     &MainWindow::onOpenProject,    QKeySequence::Open);
    addF("&Save Project",      &MainWindow::onSaveProject,    QKeySequence::Save);
    addF("Save Project &As…",  &MainWindow::onSaveProjectAs);
    fileMenu->addSeparator();
    addF("E&xit",              &MainWindow::onExit,           QKeySequence::Quit);

    auto* viewMenu = menuBar()->addMenu("&View");
    auto addV = [&](const QString& lbl, auto slot) {
        auto* a = viewMenu->addAction(lbl); a->setCheckable(true); a->setChecked(true);
        connect(a, &QAction::toggled, this, slot);
    };
    addV("Monitor Panel",  &MainWindow::onToggleMonitor);
    addV("Transmit Panel", &MainWindow::onToggleTransmit);
    addV("Signal Graph",   &MainWindow::onToggleGraph);
    viewMenu->addSeparator();

    auto addDockToggle = [&](const QString& label, const QString& shortcut, QDockWidget* dock) {
        auto* act = viewMenu->addAction(label);
        act->setShortcut(QKeySequence(shortcut));
        act->setCheckable(true); act->setChecked(false);
        connect(act,  &QAction::toggled,           dock, &QDockWidget::setVisible);
        connect(dock, &QDockWidget::visibilityChanged, act, &QAction::setChecked);
    };
    addDockToggle("Show Filter Panel",  "Ctrl+F", m_filterDock);
    addDockToggle("Show Trigger Panel", "Ctrl+T", m_triggerDock);

    auto* toolsMenu  = menuBar()->addMenu("&Outils");
    auto* rulesAct   = toolsMenu->addAction("Installer les règles udev…");
    auto* permsAct   = toolsMenu->addAction("Configurer les droits CAN (une seule fois)…");
    connect(rulesAct,  &QAction::triggered, this, &MainWindow::onInstallUdevRules);
    connect(permsAct,  &QAction::triggered, this, &MainWindow::onGrantCanPermissions);
}

void MainWindow::setupCapture(const QString& iface) {
    if (m_capture) {
        m_capture->stop();
        m_capture->wait();
        delete m_capture;
    }
    m_capture = new CanCapture(iface, this);

    connect(m_capture, &CanCapture::frameReceived, m_monitor, &MonitorPanel::onFrameReceived);
    connect(m_capture, &CanCapture::frameReceived, m_graph,   &SignalGraphPanel::onFrameReceived);
    connect(m_capture, &CanCapture::frameReceived, m_stats,   &StatsPanel::onFrameReceived);
    connect(m_capture, &CanCapture::statsUpdated,  this,      &MainWindow::onStatsUpdated);
    connect(m_capture, &CanCapture::errorOccurred, this,      &MainWindow::onCaptureError);
    connect(m_capture, &CanCapture::triggerFired,  this,      &MainWindow::onTriggerFired);
    connect(m_triggerPanel, &TriggerPanel::triggerConfigChanged, m_capture, &CanCapture::setTrigger);
    connect(m_triggerPanel, &TriggerPanel::triggerConfigChanged,
            this, [this](TriggerConfig cfg) { m_lastTriggerCfg = cfg; });

    m_capture->start();
}

void MainWindow::setupToolBar() {
    auto* tb = addToolBar("Interface");
    tb->setMovable(false);
    tb->addWidget(new QLabel("  Interface: ", this));

    m_ifaceCombo = new QComboBox(this);
    m_ifaceCombo->setMinimumWidth(120);
    m_knownIfaces = IfaceDetector::scanCanIfaces();
    m_ifaceCombo->addItems(m_knownIfaces);
    int idx = m_ifaceCombo->findText(m_iface);
    if (idx >= 0) m_ifaceCombo->setCurrentIndex(idx);
    tb->addWidget(m_ifaceCombo);
    auto* refreshBtn = new QPushButton("↺", this);
    refreshBtn->setFixedWidth(28); refreshBtn->setToolTip("Refresh interface list");
    tb->addWidget(refreshBtn); tb->addSeparator();

    tb->addWidget(new QLabel("Bitrate: ", this));
    m_bitrateCombo = new QComboBox(this);
    for (auto [label, val] : {std::pair{"125 kbit/s",125000},{"250 kbit/s",250000},
                                         {"500 kbit/s",500000},{"1000 kbit/s",1000000}})
        m_bitrateCombo->addItem(label, val);
    m_bitrateCombo->setCurrentIndex(2);
    tb->addWidget(m_bitrateCombo); tb->addSeparator();

    m_connStatusLabel = new QLabel("●", this);
    m_connStatusLabel->setStyleSheet("color: #cc3333; font-size: 14px;");
    m_connStatusLabel->setToolTip("Aucun trafic reçu");
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

    auto* recordBtn  = new QPushButton("Record",   this);
    auto* stopRecBtn = new QPushButton("Stop Rec", this);
    recordBtn->setToolTip("Start recording to .log file");
    stopRecBtn->setToolTip("Stop recording");
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
    m_bitrate = m_bitrateCombo->itemData(index).toInt();
    if (!canIfaceIsVirtual(m_iface) && !m_iface.startsWith("slcan:")) {
        if (canIfaceApply(m_iface, m_bitrate)) setupCapture(m_iface);
        else statusBar()->showMessage("Erreur : impossible de configurer " + m_iface, 5000);
    }
    m_bitrateCombo->setEnabled(!canIfaceIsVirtual(m_iface));
}

void MainWindow::onIfaceChanged(const QString& iface) {
    if (iface == m_iface) return;
    m_userPicked = (iface != "vcan0");
    m_iface = iface;
    m_statusLabel->setText("Interface: " + m_iface + "  |  0 fps");
    setConnStatus(false);
    m_bitrateCombo->setEnabled(!canIfaceIsVirtual(iface));

    if (iface.startsWith("slcan:")) {
        statusBar()->showMessage("Configuration slcan sur " + iface.mid(6) + "…", 0);
        auto [ok, slIface] = canSlcanSetup(iface.mid(6), m_bitrate);
        if (ok) { m_iface = slIface; setupCapture(m_iface); }
        else    { statusBar()->showMessage("Erreur : impossible de configurer " + iface.mid(6), 8000); }
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

void MainWindow::onStatsUpdated(uint64_t fps) {
    m_statusLabel->setText("Interface: " + m_iface + "  |  " + QString::number(fps) + " fps");
    if (fps > 0) { m_statusTimer->stop(); setConnStatus(true); }
    else if (!m_statusTimer->isActive()) { m_statusTimer->start(); }
}

void MainWindow::onStatusTimeout()               { setConnStatus(false); }
void MainWindow::onCaptureError(QString message) { statusBar()->showMessage("Capture error: " + message, 8000); }
void MainWindow::onExit()                        { close(); }

} // namespace socketspy::gui
