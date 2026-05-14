#include "main_window.h"
#include "monitor_panel.h"
#include "transmit_panel.h"
#include "signal_graph.h"
#include "stats_panel.h"
#include "can_capture.h"
#include "trigger_config.h"
#include "filter_panel.h"
#include "trigger_panel.h"
#include "project.h"
#include "project_browser.h"

#pragma push_macro("signals")
#undef signals
#include "dbc_parser.h"
#pragma pop_macro("signals")

#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QTemporaryFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStatusBar>

using namespace socketspy::dbc;

namespace socketspy::gui {

void MainWindow::onTriggerFired() {
    switch (m_lastTriggerCfg.action) {
        case TriggerConfig::Action::StartRecord: onStartRecording(); break;
        case TriggerConfig::Action::StopRecord:  onStopRecording();  break;
        case TriggerConfig::Action::Bookmark:
            statusBar()->showMessage(tr("Trigger fired — bookmark set"), 5000); break;
    }
}

void MainWindow::onOpenDbc() {
    QString path = QFileDialog::getOpenFileName(
        this, tr("Open DBC File"), {}, tr("DBC Files (*.dbc);;All Files (*)"));
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Error"),
            tr("Cannot open file: %1").arg(file.errorString()));
        return;
    }
    QString content = QTextStream(&file).readAll();

    auto result = parse_dbc(content.toStdString());
    if (!result) {
        QMessageBox::critical(this, tr("DBC Parse Error"),
            QString::fromStdString(
                std::string(parse_error_string(result.error()))));
        return;
    }
    *m_dbc = *result;
    m_dbcPath = path;
    m_monitor->onDbcLoaded(*m_dbc);
    m_graph->onDbcLoaded(*m_dbc);
    m_stats->onDbcLoaded(*m_dbc);
    statusBar()->showMessage("DBC: " + QString::number(m_dbc->messages.size()) + " messages", 5000);
}

void MainWindow::onToggleMonitor(bool v) {
    int i = m_tabs->indexOf(m_monitor);
    if (v && i==-1) m_tabs->insertTab(0, m_monitor, QString::fromUtf8("⊞  ") + tr("Monitor"));
    else if (!v && i!=-1) m_tabs->removeTab(i);
}
void MainWindow::onToggleTransmit(bool v) {
    int i = m_tabs->indexOf(m_transmit);
    if (v && i==-1) m_tabs->addTab(m_transmit, QString::fromUtf8("↑  ") + tr("Transmit"));
    else if (!v && i!=-1) m_tabs->removeTab(i);
}
void MainWindow::onToggleGraph(bool v) {
    int i = m_tabs->indexOf(m_graph);
    if (v && i==-1) m_tabs->addTab(m_graph, QString::fromUtf8("∿  ") + tr("Graph"));
    else if (!v && i!=-1) m_tabs->removeTab(i);
}

void MainWindow::onStartRecording() {
    QString path = QFileDialog::getSaveFileName(
        this, tr("Save CAN Log"), {}, tr("CAN Log Files (*.log);;All Files (*)"));
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

ProjectData MainWindow::collectProject() const {
    ProjectData p;
    p.iface         = m_iface;
    p.bitrate       = m_bitrate;
    p.dbcPath       = m_dbcPath;
    p.graphSignals  = m_graph->trackedSignals();
    p.signalAliases = m_signalAliases;
    p.filter        = m_filterPanel->currentFilter();
    p.trigger       = m_triggerPanel->currentConfig();
    return p;
}

void MainWindow::applyProject(const ProjectData& p) {
    m_bitrate = p.bitrate;
    int bIdx = m_bitrateCombo->findData(p.bitrate);
    if (bIdx >= 0) m_bitrateCombo->setCurrentIndex(bIdx);
    if (!p.iface.isEmpty()) m_ifaceCombo->setCurrentText(p.iface);
    m_filterPanel->applyFilter(p.filter);
    m_triggerPanel->applyConfig(p.trigger);
    m_signalAliases = p.signalAliases;
    m_monitor->setAliases(m_signalAliases);
    m_graph->restoreSignals(p.graphSignals, p.signalAliases);
    if (!p.dbcPath.isEmpty()) {
        QFile f(p.dbcPath);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            auto res = socketspy::dbc::parse_dbc(QTextStream(&f).readAll().toStdString());
            if (res) { *m_dbc = *res; m_dbcPath = p.dbcPath;
                m_monitor->onDbcLoaded(*m_dbc); m_graph->onDbcLoaded(*m_dbc); m_stats->onDbcLoaded(*m_dbc); }
        }
    }
}

void MainWindow::onNewProject() {
    m_projectPath.clear();
    m_signalAliases.clear();
    m_monitor->setAliases({});
    m_graph->restoreSignals({});
    m_filterPanel->applyFilter({});
    m_triggerPanel->applyConfig({});
    setWindowTitle("SocketSpy");
}

void MainWindow::onOpenProject() {
    onShowProjectBrowser();
}

void MainWindow::onShowProjectBrowser() {
    ProjectBrowserDialog dlg(m_projectRegistry, this);
    if (dlg.exec() != QDialog::Accepted) return;

    if (dlg.wantsNew()) { onNewProject(); return; }

    const QString path = dlg.selectedPath();
    if (path.isEmpty()) return;

    ProjectData p;
    QString err;
    if (!projectLoad(p, path, err)) {
        QMessageBox::critical(this, tr("Error"), tr("Cannot open project: %1").arg(err));
        return;
    }
    m_projectPath = path;
    m_projectRegistry.add(path);
    applyProject(p);
    setWindowTitle("SocketSpy \xe2\x80\x94 " + QFileInfo(path).baseName());
}

void MainWindow::onSaveProject() {
    if (m_projectPath.isEmpty()) { onSaveProjectAs(); return; }
    QString err;
    if (!projectSave(collectProject(), m_projectPath, err))
        QMessageBox::critical(this, tr("Error"), tr("Cannot save: %1").arg(err));
}

void MainWindow::onSaveProjectAs() {
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save project"), {}, tr("SocketSpy Projects (*.spyproj);;All Files (*)"));
    if (path.isEmpty()) return;
    m_projectPath = path.endsWith(".spyproj") ? path : path + ".spyproj";
    onSaveProject();
    m_projectRegistry.add(m_projectPath);
    setWindowTitle("SocketSpy \xe2\x80\x94 " + QFileInfo(m_projectPath).baseName());
}

void MainWindow::setConnStatus(bool active) {
    if (active) {
        m_connStatusLabel->setText(QString::fromUtf8("● LIVE"));
        m_connStatusLabel->setStyleSheet("color: #22c55e; font-size: 11px; font-weight:700; letter-spacing:1px;");
        m_connStatusLabel->setToolTip(tr("Interface active"));
    } else {
        m_connStatusLabel->setText(QString::fromUtf8("●  \xe2\x80\x93 \xe2\x80\x93"));
        m_connStatusLabel->setStyleSheet("color: #4b5563; font-size: 11px; font-weight:700; letter-spacing:1px;");
        m_connStatusLabel->setToolTip(tr("No traffic received"));
    }
}

void MainWindow::onGrantCanPermissions() {
    const QString user = QProcessEnvironment::systemEnvironment().value("USER");
    if (user.isEmpty()) {
        QMessageBox::critical(this, tr("Error"), tr("Cannot determine the username."));
        return;
    }

    static const char* kScript =
        "#!/bin/sh\n"
        "case \"$1\" in\n"
        "  apply) ip link set \"$2\" down; ip link set \"$2\" up type can bitrate \"$3\" ;;\n"
        "  slcan) pkill slcand 2>/dev/null; slcand -o -c \"-s$3\" \"$2\"; ip link set slcan0 up ;;\n"
        "  *) exit 1 ;;\n"
        "esac\n";

    QTemporaryFile tmpScript, tmpSudoers;
    tmpScript.setAutoRemove(false);
    tmpSudoers.setAutoRemove(false);
    if (!tmpScript.open() || !tmpSudoers.open()) {
        QMessageBox::critical(this, tr("Error"), tr("Cannot create temporary files."));
        return;
    }
    tmpScript.write(kScript);
    tmpScript.close();
    tmpSudoers.write((user + " ALL=(root) NOPASSWD: /usr/local/bin/socketspy-can-setup\n").toUtf8());
    tmpSudoers.close();

    const QString cmd = QString(
        "install -m 755 %1 /usr/local/bin/socketspy-can-setup && "
        "install -m 440 %2 /etc/sudoers.d/socketspy-can && "
        "visudo -c -f /etc/sudoers.d/socketspy-can")
        .arg(tmpScript.fileName(), tmpSudoers.fileName());

    QProcess p;
    p.start("pkexec", {"sh", "-c", cmd});
    p.waitForFinished(15000);
    QFile::remove(tmpScript.fileName());
    QFile::remove(tmpSudoers.fileName());

    if (p.exitCode() == 0)
        QMessageBox::information(this, tr("CAN permissions configured"),
            tr("Success! CAN interfaces can now be configured without a password."));
    else
        QMessageBox::critical(this, tr("Error"),
            "Cannot configure permissions.\nCheck that pkexec, sudo, and visudo are installed.");
}

void MainWindow::onInstallUdevRules() {
    static const char* kRules = R"(# SocketSpy — udev rules for USB CAN adapters
SUBSYSTEM=="usb", ATTRS{idVendor}=="1d50", ATTRS{idProduct}=="606f", \
    RUN+="/sbin/modprobe -b gs_usb"
SUBSYSTEM=="usb", ATTRS{idVendor}=="1d50", ATTRS{idProduct}=="5070", \
    RUN+="/sbin/modprobe -b gs_usb"
SUBSYSTEM=="usb", ATTRS{idVendor}=="0c72", \
    RUN+="/sbin/modprobe -b peak_usb"
SUBSYSTEM=="usb", ATTRS{idVendor}=="0bfd", \
    RUN+="/sbin/modprobe -b kvaser_usb"
SUBSYSTEM=="usb", ATTRS{idVendor}=="08d8", \
    RUN+="/sbin/modprobe -b ems_usb"
SUBSYSTEM=="net", ACTION=="add", KERNEL=="can*", \
    RUN+="/bin/ip link set %k up type can bitrate 500000"
SUBSYSTEM=="net", KERNEL=="can*", GROUP="plugdev", MODE="0660"
)";

    QTemporaryFile tmp;
    tmp.setAutoRemove(false);
    if (!tmp.open()) {
        QMessageBox::critical(this, tr("Error"), tr("Cannot create a temporary file."));
        return;
    }
    tmp.write(kRules);
    tmp.close();

    auto run = [&](const QStringList& args) -> bool {
        QProcess p;
        p.start("pkexec", args);
        p.waitForFinished(10000);
        return p.exitCode() == 0;
    };

    bool ok = run({"cp", tmp.fileName(), "/etc/udev/rules.d/99-socketspy-can.rules"})
           && run({"udevadm", "control", "--reload"})
           && run({"udevadm", "trigger"});

    QFile::remove(tmp.fileName());

    if (ok)
        QMessageBox::information(this, tr("Success"),
            tr("udev rules installed.") + "\n" +
            "Reconnect your CAN USB adapter — it will be detected automatically.");
    else
        QMessageBox::critical(this, tr("Error"),
            "Cannot install udev rules.\nCheck that pkexec is installed.");
}

} // namespace socketspy::gui
