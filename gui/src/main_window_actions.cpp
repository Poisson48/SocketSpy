#include "main_window.h"
#include "monitor_panel.h"
#include "transmit_panel.h"
#include "signal_graph.h"
#include "stats_panel.h"
#include "can_capture.h"
#include "trigger_config.h"

#pragma push_macro("signals")
#undef signals
#include "dbc_parser.h"
#pragma pop_macro("signals")

#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QStatusBar>

using namespace socketspy::dbc;

namespace socketspy::gui {

void MainWindow::onTriggerFired() {
    switch (m_lastTriggerCfg.action) {
        case TriggerConfig::Action::StartRecord: onStartRecording(); break;
        case TriggerConfig::Action::StopRecord:  onStopRecording();  break;
        case TriggerConfig::Action::Bookmark:
            statusBar()->showMessage("Trigger fired — bookmark set", 5000); break;
    }
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
    m_stats->onDbcLoaded(*m_dbc);
    statusBar()->showMessage(
        "DBC loaded: "
        + QString::number(m_dbc->messages.size()) + " messages", 5000);
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

void MainWindow::onStartRecording() {
    QString path = QFileDialog::getSaveFileName(
        this, "Save CAN Log", {}, "CAN Log Files (*.log);;All Files (*)");
    if (path.isEmpty()) return;
    m_recorder.open(path);
    if (!m_recorder.isOpen()) return;
    connect(m_capture, &CanCapture::frameReceived,
            this, [this](socketspy::core::CanFrame frame) {
                m_recorder.write(frame, m_iface);
            });
}

void MainWindow::onStopRecording() {
    m_recorder.close();
    disconnect(m_capture, &CanCapture::frameReceived, this, nullptr);
    connect(m_capture, &CanCapture::frameReceived,
            m_monitor, &MonitorPanel::onFrameReceived);
    connect(m_capture, &CanCapture::frameReceived,
            m_graph,   &SignalGraphPanel::onFrameReceived);
}

} // namespace socketspy::gui
