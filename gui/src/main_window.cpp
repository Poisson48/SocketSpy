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
#include <QMessageBox>

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
    setWindowTitle(QString("SocketSpy v%1").arg(APP_VERSION));
    resize(1200, 700);

    m_tabs = new QTabWidget(this);
    setCentralWidget(m_tabs);

    m_monitor   = new MonitorPanel(this);
    m_transmit  = new TransmitPanel(this);
    m_graph     = new SignalGraphPanel(this);
    m_stats     = new StatsPanel(this);
    m_replay    = new ReplayPanel(this);
    m_elm327Panel = new Elm327Panel(this);
    m_simulator     = new SimulatorPanel(this);
    m_scriptPanel   = new ScriptingPanel(this);
    m_protocolPanel = new ProtocolPanel(this);

    m_tabs->addTab(m_monitor,       QString::fromUtf8("⊞  ") + tr("Monitor"));
    m_tabs->addTab(m_transmit,      QString::fromUtf8("↑  ") + tr("Transmit"));
    m_tabs->addTab(m_graph,         QString::fromUtf8("∿  ") + tr("Graph"));
    m_tabs->addTab(m_replay,        QString::fromUtf8("▶  ") + tr("Replay"));
    m_tabs->addTab(m_stats,         QString::fromUtf8("⊞  ") + tr("Stats"));
    m_tabs->addTab(m_elm327Panel,   QString::fromUtf8("⚡  ") + tr("OBD2"));
    m_tabs->addTab(m_simulator,     QString::fromUtf8("⚙  ") + tr("Simulator"));
    m_tabs->addTab(m_scriptPanel,   QString::fromUtf8("{}  ") + tr("Scripts"));
    m_tabs->addTab(m_protocolPanel, QString::fromUtf8("⊕  ") + tr("Protocols"));

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

    auto wireFrames = [&](auto* src, auto sig) {
        connect(src, sig, m_monitor,       &MonitorPanel::onFrameReceived);
        connect(src, sig, m_graph,         &SignalGraphPanel::onFrameReceived);
        connect(src, sig, m_stats,         &StatsPanel::onFrameReceived);
        connect(src, sig, m_protocolPanel, &ProtocolPanel::onFrameReceived);
    };
    wireFrames(m_elm327Panel, &Elm327Panel::frameReceived);
    wireFrames(m_simulator,   &SimulatorPanel::frameGenerated);
    connect(m_elm327Panel, &Elm327Panel::frameReceived,
            this, &MainWindow::onAnyFrameReceived, Qt::DirectConnection);
    connect(m_simulator,   &SimulatorPanel::frameGenerated,
            this, &MainWindow::onAnyFrameReceived, Qt::DirectConnection);

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
}

void MainWindow::setupMenuBar() {
    auto* fileMenu = menuBar()->addMenu(tr("&File"));
    auto addF = [&](const QString& lbl, auto slot, QKeySequence k = {}) {
        auto* a = fileMenu->addAction(lbl); if (!k.isEmpty()) a->setShortcut(k);
        connect(a, &QAction::triggered, this, slot);
    };
    addF(tr("&Open DBC…"),         &MainWindow::onOpenDbc);
    fileMenu->addSeparator();
    addF(tr("&New Project"),       &MainWindow::onNewProject,     QKeySequence::New);
    addF(tr("&Open Project…"),     &MainWindow::onOpenProject,    QKeySequence::Open);
    addF(tr("&Save Project"),      &MainWindow::onSaveProject,    QKeySequence::Save);
    addF(tr("Save Project &As…"),  &MainWindow::onSaveProjectAs);
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

    auto* helpMenu = menuBar()->addMenu(tr("&Help"));
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
    connect(m_capture, &CanCapture::frameReceived, this,            &MainWindow::onAnyFrameReceived,
            Qt::DirectConnection);
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
                                         {"500 kbit/s",500000},{"1000 kbit/s",1000000}})
        m_bitrateCombo->addItem(label, val);
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
    m_fpsCount = 0;
    m_smoothFps = 0.0;
    m_statusLabel->setText(m_iface + "  \xc2\xb7  0 fps");
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

void MainWindow::onAnyFrameReceived() {
    ++m_fpsCount;
}

void MainWindow::onFpsTick() {
    constexpr double alpha = 0.3;
    m_smoothFps = alpha * m_fpsCount + (1.0 - alpha) * m_smoothFps;
    m_fpsCount = 0;
    const QString txt = m_iface + "  \xc2\xb7  " + QString::number(qRound(m_smoothFps)) + " fps";
    m_statusLabel->setText(txt);
    if (m_smoothFps > 0.5) { m_statusTimer->stop(); setConnStatus(true); }
    else if (!m_statusTimer->isActive()) { m_statusTimer->start(); }
}

void MainWindow::onStatusTimeout()               { setConnStatus(false); }
void MainWindow::onCaptureError(QString message) { statusBar()->showMessage(tr("Capture error: ") + message, 8000); }
void MainWindow::onExit()                        { close(); }

} // namespace socketspy::gui
