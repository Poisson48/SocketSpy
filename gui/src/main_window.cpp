#include "main_window.h"
#include "can_capture.h"
#include "monitor_panel.h"
#include "transmit_panel.h"
#include "signal_graph.h"
#include "filter_panel.h"

// Include DBC parser after Qt headers but with signals macro suppressed.
#pragma push_macro("signals")
#undef signals
#include "dbc_parser.h"
#pragma pop_macro("signals")

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QFile>
#include <QTextStream>
#include <QDir>
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

    m_tabs     = new QTabWidget(this);
    setCentralWidget(m_tabs);

    m_monitor  = new MonitorPanel(this);
    m_transmit = new TransmitPanel(this);
    m_graph    = new SignalGraphPanel(this);

    m_tabs->addTab(m_monitor,  "Monitor");
    m_tabs->addTab(m_transmit, "Transmit");
    m_tabs->addTab(m_graph,    "Signal Graph");

    m_statusLabel = new QLabel("Interface: " + m_iface + "  |  0 fps", this);
    statusBar()->addPermanentWidget(m_statusLabel);

    connect(m_monitor, &MonitorPanel::signalDoubleClicked,
            m_graph,   &SignalGraphPanel::addSignal);

    m_filterPanel = new FilterPanel(this);
    m_filterDock = new QDockWidget("Filter", this);
    m_filterDock->setWidget(m_filterPanel);
    m_filterDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::RightDockWidgetArea, m_filterDock);
    m_filterDock->hide();

    connect(m_filterPanel, &FilterPanel::filterChanged,
            m_monitor,     &MonitorPanel::onFilterChanged);
}

void MainWindow::setupMenuBar() {
    auto* fileMenu = menuBar()->addMenu("&File");

    auto* openDbc = fileMenu->addAction("&Open DBC...");
    connect(openDbc, &QAction::triggered, this, &MainWindow::onOpenDbc);

    fileMenu->addSeparator();

    auto* exitAct = fileMenu->addAction("E&xit");
    exitAct->setShortcut(QKeySequence::Quit);
    connect(exitAct, &QAction::triggered, this, &MainWindow::onExit);

    auto* viewMenu = menuBar()->addMenu("&View");

    auto* monAct = viewMenu->addAction("Monitor Panel");
    monAct->setCheckable(true);
    monAct->setChecked(true);
    connect(monAct, &QAction::toggled, this, &MainWindow::onToggleMonitor);

    auto* txAct = viewMenu->addAction("Transmit Panel");
    txAct->setCheckable(true);
    txAct->setChecked(true);
    connect(txAct, &QAction::toggled, this, &MainWindow::onToggleTransmit);

    auto* graphAct = viewMenu->addAction("Signal Graph");
    graphAct->setCheckable(true);
    graphAct->setChecked(true);
    connect(graphAct, &QAction::toggled, this, &MainWindow::onToggleGraph);

    viewMenu->addSeparator();

    auto* filterAct = viewMenu->addAction("Show Filter Panel");
    filterAct->setShortcut(QKeySequence("Ctrl+F"));
    filterAct->setCheckable(true);
    filterAct->setChecked(false);
    connect(filterAct, &QAction::toggled, m_filterDock, &QDockWidget::setVisible);
    connect(m_filterDock, &QDockWidget::visibilityChanged, filterAct, &QAction::setChecked);
}

void MainWindow::setupCapture(const QString& iface) {
    if (m_capture) {
        m_capture->stop();
        m_capture->wait();
        delete m_capture;
    }
    m_capture = new CanCapture(iface, this);

    connect(m_capture, &CanCapture::frameReceived,
            m_monitor, &MonitorPanel::onFrameReceived);
    connect(m_capture, &CanCapture::frameReceived,
            m_graph,   &SignalGraphPanel::onFrameReceived);
    connect(m_capture, &CanCapture::statsUpdated,
            this,      &MainWindow::onStatsUpdated);
    connect(m_capture, &CanCapture::errorOccurred,
            this,      &MainWindow::onCaptureError);

    m_capture->start();
}

void MainWindow::onOpenDbc() {
    QString path = QFileDialog::getOpenFileName(
        this, "Open DBC File", {}, "DBC Files (*.dbc);;All Files (*)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error",
            "Cannot open file: " + file.errorString());
        return;
    }
    QString content = QTextStream(&file).readAll();

    auto result = parse_dbc(content.toStdString());
    if (!result) {
        QMessageBox::critical(this, "DBC Parse Error",
            QString::fromStdString(
                std::string(parse_error_string(result.error()))));
        return;
    }
    *m_dbc = *result;
    m_monitor->onDbcLoaded(*m_dbc);
    m_graph->onDbcLoaded(*m_dbc);
    statusBar()->showMessage(
        "DBC loaded: "
        + QString::number(m_dbc->messages.size()) + " messages", 5000);
}

void MainWindow::onExit() { close(); }

// Scan /sys/class/net/ for SocketCAN interfaces (type == 280 = ARPHRD_CAN).
// Always includes vcan0 if present; falls back to showing all if none found.
QStringList MainWindow::scanCanIfaces() {
    QStringList result;
    QDir netDir("/sys/class/net");
    const auto entries = netDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& name : entries) {
        QFile typeFile("/sys/class/net/" + name + "/type");
        if (!typeFile.open(QIODevice::ReadOnly)) continue;
        if (typeFile.readAll().trimmed() == "280")
            result << name;
    }
    if (result.isEmpty()) result << "vcan0";
    return result;
}

void MainWindow::setupToolBar() {
    auto* tb = addToolBar("Interface");
    tb->setMovable(false);

    tb->addWidget(new QLabel("  Interface: ", this));

    m_ifaceCombo = new QComboBox(this);
    m_ifaceCombo->setMinimumWidth(120);
    m_ifaceCombo->setToolTip("Select CAN interface");
    const QStringList ifaces = scanCanIfaces();
    m_ifaceCombo->addItems(ifaces);
    // Pre-select current iface if present
    int idx = m_ifaceCombo->findText(m_iface);
    if (idx >= 0) m_ifaceCombo->setCurrentIndex(idx);
    tb->addWidget(m_ifaceCombo);

    auto* refreshBtn = new QPushButton("↺", this);
    refreshBtn->setFixedWidth(28);
    refreshBtn->setToolTip("Refresh interface list");
    tb->addWidget(refreshBtn);
    tb->addSeparator();

    connect(m_ifaceCombo, &QComboBox::currentTextChanged,
            this, &MainWindow::onIfaceChanged);
    connect(refreshBtn, &QPushButton::clicked,
            this, &MainWindow::onRefreshIfaces);
}

void MainWindow::onIfaceChanged(const QString& iface) {
    if (iface == m_iface) return;
    m_iface = iface;
    m_statusLabel->setText("Interface: " + m_iface + "  |  0 fps");
    setupCapture(m_iface);
}

void MainWindow::onRefreshIfaces() {
    const QString current = m_ifaceCombo->currentText();
    m_ifaceCombo->blockSignals(true);
    m_ifaceCombo->clear();
    m_ifaceCombo->addItems(scanCanIfaces());
    int idx = m_ifaceCombo->findText(current);
    m_ifaceCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    m_ifaceCombo->blockSignals(false);
    // Trigger reconnect if the previously selected iface disappeared
    if (m_ifaceCombo->currentText() != current)
        onIfaceChanged(m_ifaceCombo->currentText());
}

void MainWindow::onStatsUpdated(uint64_t fps) {
    m_statusLabel->setText(
        "Interface: " + m_iface + "  |  "
        + QString::number(fps) + " fps");
}

void MainWindow::onCaptureError(QString message) {
    statusBar()->showMessage("Capture error: " + message, 8000);
}

void MainWindow::onToggleMonitor(bool visible) {
    int idx = m_tabs->indexOf(m_monitor);
    if (visible && idx == -1)
        m_tabs->insertTab(0, m_monitor, "Monitor");
    else if (!visible && idx != -1)
        m_tabs->removeTab(idx);
}

void MainWindow::onToggleTransmit(bool visible) {
    int idx = m_tabs->indexOf(m_transmit);
    if (visible && idx == -1)
        m_tabs->addTab(m_transmit, "Transmit");
    else if (!visible && idx != -1)
        m_tabs->removeTab(idx);
}

void MainWindow::onToggleGraph(bool visible) {
    int idx = m_tabs->indexOf(m_graph);
    if (visible && idx == -1)
        m_tabs->addTab(m_graph, "Signal Graph");
    else if (!visible && idx != -1)
        m_tabs->removeTab(idx);
}

} // namespace socketspy::gui
