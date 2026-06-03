#include "main_window.h"
#include "update_dialog.h"
#include "sidebar_nav.h"
#include "welcome_screen.h"
#include "permission_checker.h"
#include "monitor_panel.h"
#include "simulator_panel.h"
#include "transmit_panel.h"
#include "signal_graph.h"
#include "stats_panel.h"
#include "can_capture.h"
#include "trigger_config.h"
#include "filter_panel.h"
#include "trigger_panel.h"
#include "project.h"
#include "project_browser.h"
#include "dbc_builder_panel.h"
#include "protocol_panel.h"
#include "signal_validator_panel.h"
#include "verify_signal_panel.h"
#include "blf_writer.h"
#include "mdf4_writer.h"
#include "asc_io.h"
#include "trc_io.h"
#include "pcap_io.h"
#include "replay_panel.h"
#include "iface_detector.h"
#include "can_capture.h"

// Permanently undef Qt's `signals` macro so we can access dbc::Message::signals.
#include "dbc_compat.h"
#include "dbc_writer.h"
#include "gui_palette.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QTemporaryFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStatusBar>
#include <QInputDialog>
#include <QTableWidget>

using namespace socketspy::dbc;

namespace socketspy::gui {

void MainWindow::onExportMonitorCsv() {
    // Delegate to the Monitor panel's own CSV export action
    m_monitor->onExportCsv();
}

void MainWindow::onSaveDbc() {
    const auto& db = m_dbcBuilder->database();
    if (db.messages.empty()) {
        statusBar()->showMessage(tr("DBC Builder: no messages defined"), 4000);
        return;
    }
    QString path = QFileDialog::getSaveFileName(
        this, tr("Save DBC"), {}, tr("DBC Files (*.dbc);;All Files (*)"));
    if (path.isEmpty()) return;
    if (!path.endsWith(".dbc")) path += ".dbc";
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Error"), f.errorString());
        return;
    }
    QTextStream(&f) << QString::fromStdString(socketspy::dbc::write_dbc(db));
    statusBar()->showMessage(tr("DBC saved: ") + path, 5000);
}

void MainWindow::onTriggerFired() {
    switch (m_lastTriggerCfg.action) {
        case TriggerConfig::Action::StartRecord: onStartRecording(); break;
        case TriggerConfig::Action::StopRecord:  onStopRecording();  break;
        case TriggerConfig::Action::Bookmark:
            statusBar()->showMessage(tr("Trigger fired — bookmark set"), 5000); break;
    }
}

void MainWindow::loadDbcFromPath(const QString& path) {
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
    m_dbcBuilder->loadDbc(*m_dbc);
    m_protocolPanel->onDbcLoaded(*m_dbc);
    m_signalValidator->onDbcLoaded(*m_dbc);
    m_verifySignal->onDbcLoaded(*m_dbc);
    statusBar()->showMessage("DBC: " + QString::number(m_dbc->messages.size()) + " messages", 5000);
}

void MainWindow::onOpenDbc() {
    QString path = QFileDialog::getOpenFileName(
        this, tr("Open DBC File"), {}, tr("DBC Files (*.dbc);;All Files (*)"));
    if (path.isEmpty()) return;
    loadDbcFromPath(path);
}

void MainWindow::onToggleMonitor(bool v)  { m_sidebar->setPanelVisible(m_monitor,  v); }
void MainWindow::onToggleTransmit(bool v) { m_sidebar->setPanelVisible(m_transmit, v); }
void MainWindow::onToggleGraph(bool v)    { m_sidebar->setPanelVisible(m_graph,    v); }

void MainWindow::onStartRecording() {
    QString path = QFileDialog::getSaveFileName(
        this, tr("Save CAN Log"), m_defaultLogPath, tr("CAN Log Files (*.log);;All Files (*)"));
    if (path.isEmpty()) return;
    m_recorder.open(path);
    if (!m_recorder.isOpen()) return;
    m_recorder.setFilter(m_filterPanel->currentFilter());
    connect(m_capture, &CanCapture::frameReceived,
            this, [this](socketspy::core::CanFrame frame) {
                m_recorder.write(frame, m_iface);
            });
    m_recLabel->setText(QString::fromUtf8("\xe2\x97\x8f  REC"));
    m_recLabel->setStyleSheet("color: #ef4444; font-weight: 700;");
    m_recLabel->show();
}

void MainWindow::onStopRecording() {
    m_recorder.close();
    // Disconnect only the recorder lambda (connected to `this` in onStartRecording).
    // m_monitor and m_graph remain connected from setupCapture() and must not be
    // reconnected here — doing so would duplicate frames. Use UniqueConnection as
    // a safety net in case this function is ever called in an unexpected order.
    disconnect(m_capture, &CanCapture::frameReceived, this, nullptr);
    connect(m_capture, &CanCapture::frameReceived,
            m_monitor, &MonitorPanel::onFrameReceived,
            Qt::UniqueConnection);
    connect(m_capture, &CanCapture::frameReceived,
            m_graph,   &SignalGraphPanel::onFrameReceived,
            Qt::UniqueConnection);
    m_recLabel->hide();
    m_recLabel->clear();
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
    p.simulatorProfile = m_simulator->currentProfileName();
    {
        const auto mf    = m_monitor->currentMonitorFilter();
        p.monitorFilter.changedOnly  = mf.changedOnly;
        p.monitorFilter.dlc          = mf.dlc;
        p.monitorFilter.useTimestamp = mf.useTimestamp;
        p.monitorFilter.tsMin        = mf.tsMin;
        p.monitorFilter.tsMax        = mf.tsMax;
    }
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
                m_monitor->onDbcLoaded(*m_dbc); m_graph->onDbcLoaded(*m_dbc); m_stats->onDbcLoaded(*m_dbc);
                m_dbcBuilder->loadDbc(*m_dbc); m_protocolPanel->onDbcLoaded(*m_dbc);
                m_signalValidator->onDbcLoaded(*m_dbc); m_verifySignal->onDbcLoaded(*m_dbc); }
        }
    }
    if (!p.logPath.isEmpty())
        m_defaultLogPath = p.logPath;
    if (!p.simulatorProfile.isEmpty())
        m_simulator->setProfileByName(p.simulatorProfile);
    {
        MonitorFilter mf;
        mf.changedOnly  = p.monitorFilter.changedOnly;
        mf.dlc          = p.monitorFilter.dlc;
        mf.useTimestamp = p.monitorFilter.useTimestamp;
        mf.tsMin        = p.monitorFilter.tsMin;
        mf.tsMax        = p.monitorFilter.tsMax;
        m_monitor->applyMonitorFilter(mf);
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
    m_welcomeScreen->refreshRecentProjects();
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
    m_welcomeScreen->refreshRecentProjects();
    setWindowTitle("SocketSpy \xe2\x80\x94 " + QFileInfo(m_projectPath).baseName());
}

void MainWindow::onWelcomeOpenProject(const QString& path) {
    if (path.isEmpty()) {
        // no specific path → open the project browser dialog
        onShowProjectBrowser();
        return;
    }
    ProjectData p;
    QString err;
    if (!projectLoad(p, path, err)) {
        QMessageBox::critical(this, tr("Error"), tr("Cannot open project: %1").arg(err));
        return;
    }
    m_projectPath = path;
    m_projectRegistry.add(path);
    m_welcomeScreen->refreshRecentProjects();
    applyProject(p);
    setWindowTitle("SocketSpy \xe2\x80\x94 " + QFileInfo(path).baseName());
    // Navigate to Monitor after loading
    m_sidebar->showPanel(m_monitor);
}

void MainWindow::onWelcomeQuickConnect() {
    // Select vcan0 (or the first available interface) and navigate to Monitor
    const QString target = m_knownIfaces.contains("vcan0") ? "vcan0"
                         : (!m_knownIfaces.isEmpty() ? m_knownIfaces.first() : QString());
    if (!target.isEmpty())
        m_ifaceCombo->setCurrentText(target);
    m_sidebar->showPanel(m_monitor);
}

void MainWindow::setConnStatus(bool active) {
    if (active) {
        m_connStatusLabel->setText(QString::fromUtf8("● LIVE"));
        m_connStatusLabel->setStyleSheet(
            QString("color: %1; font-size: 11px; font-weight:700; letter-spacing:1px;")
                .arg(Palette::kLiveGreen));
        m_connStatusLabel->setToolTip(tr("Interface active"));
    } else {
        m_connStatusLabel->setText(QString::fromUtf8("●  \xe2\x80\x93 \xe2\x80\x93"));
        m_connStatusLabel->setStyleSheet(
            QString("color: %1; font-size: 11px; font-weight:700; letter-spacing:1px;")
                .arg(Palette::kDeadGray));
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

    if (ok) {
        // Add user to plugdev so udev rules (GROUP="plugdev") apply immediately
        const QString user = QProcessEnvironment::systemEnvironment().value("USER");
        if (!user.isEmpty()
                && PermissionChecker::groupExists("plugdev")
                && !PermissionChecker::userInGroup("plugdev")) {
            run({"usermod", "-aG", "plugdev", user});
        }
        QMessageBox::information(this, tr("Success"),
            tr("udev rules installed.") + "\n" +
            "Reconnect your CAN USB adapter — it will be detected automatically.");
    } else {
        QMessageBox::critical(this, tr("Error"),
            "Cannot install udev rules.\nCheck that pkexec is installed.");
    }
}

// ---------------------------------------------------------------------------
// Helpers: collect all visible frames from the monitor table
// We re-parse the table text since MonitorPanel owns the data model.
// For a cleaner architecture a getFrames() API could be added later.

static std::vector<socketspy::core::CanFrame> collectMonitorFrames(MonitorPanel* monitor) {
    std::vector<socketspy::core::CanFrame> frames;
    QTableWidget* table = monitor->findChild<QTableWidget*>();
    if (!table) return frames;
    for (int r = 0; r < table->rowCount(); ++r) {
        if (table->isRowHidden(r)) continue;
        auto* tsItem   = table->item(r, 0);
        auto* idItem   = table->item(r, 1);
        auto* dlcItem  = table->item(r, 2);
        auto* dataItem = table->item(r, 3);
        if (!tsItem || !idItem || !dlcItem || !dataItem) continue;

        socketspy::core::CanFrame f{};
        f.timestamp_us = tsItem->text().toULongLong();
        f.id           = idItem->text().trimmed().startsWith("* ")
                       ? idItem->text().mid(2).trimmed().toUInt(nullptr, 16)
                       : idItem->text().trimmed().toUInt(nullptr, 16);
        f.dlc          = static_cast<uint8_t>(dlcItem->text().toUInt());
        // Parse hex bytes
        QString hex = dataItem->text().remove(' ');
        for (int i = 0; i < static_cast<int>(hex.length() / 2) && i < 64; ++i)
            f.data[i] = static_cast<uint8_t>(hex.mid(i * 2, 2).toUInt(nullptr, 16));
        frames.push_back(f);
    }
    return frames;
}

void MainWindow::onExportBlf() {
    auto frames = collectMonitorFrames(m_monitor);
    if (frames.empty()) {
        QMessageBox::information(this, tr("Export BLF"), tr("No visible data to export."));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export BLF"), {}, tr("BLF Files (*.blf);;All Files (*)"));
    if (path.isEmpty()) return;
    const QString outPath = path.endsWith(".blf") ? path : path + ".blf";
    if (BlfWriter::write(outPath, frames))
        statusBar()->showMessage(tr("BLF exported: %1 frames → %2").arg(frames.size()).arg(outPath), 5000);
    else
        QMessageBox::critical(this, tr("Export BLF"), tr("Failed to write BLF file."));
}

void MainWindow::onExportMdf4() {
    auto frames = collectMonitorFrames(m_monitor);
    if (frames.empty()) {
        QMessageBox::information(this, tr("Export MDF4"), tr("No visible data to export."));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export MDF4"), {}, tr("MDF4 Files (*.mf4);;All Files (*)"));
    if (path.isEmpty()) return;
    const QString outPath = path.endsWith(".mf4") ? path : path + ".mf4";
    if (Mdf4Writer::write(outPath, frames))
        statusBar()->showMessage(tr("MDF4 exported: %1 frames → %2").arg(frames.size()).arg(outPath), 5000);
    else
        QMessageBox::critical(this, tr("Export MDF4"), tr("Failed to write MDF4 file."));
}

void MainWindow::onAddBus() {
    QStringList ifaces = IfaceDetector::scanCanIfaces();
    ifaces.removeAll(m_iface);  // exclude already active interface
    if (ifaces.isEmpty()) {
        QMessageBox::information(this, tr("Add Bus"),
            tr("No additional CAN interfaces found."));
        return;
    }
    bool ok = false;
    const QString iface = QInputDialog::getItem(
        this, tr("Add Second Bus"), tr("Select interface:"), ifaces, 0, false, &ok);
    if (!ok || iface.isEmpty()) return;

    if (m_capture2) {
        m_capture2->stop();
        m_capture2->wait();
        delete m_capture2;
        m_capture2 = nullptr;
    }

    m_capture2 = new CanCapture(iface, this);
    const QString busLabel = iface;
    connect(m_capture2, &CanCapture::frameReceived,
            this, [this, busLabel](socketspy::core::CanFrame frame) {
                m_monitor->onFrameReceivedOnBus(frame, busLabel);
            });
    m_capture2->start();
    statusBar()->showMessage(tr("Second bus added: %1").arg(iface), 5000);
}

void MainWindow::onRemoveBus() {
    if (!m_capture2) {
        statusBar()->showMessage(tr("No second bus active"), 3000);
        return;
    }
    m_capture2->stop();
    m_capture2->wait();
    delete m_capture2;
    m_capture2 = nullptr;
    statusBar()->showMessage(tr("Second bus removed"), 3000);
}

void MainWindow::onExportAsc() {
    auto frames = collectMonitorFrames(m_monitor);
    if (frames.empty()) {
        QMessageBox::information(this, tr("Export ASC"), tr("No visible data to export."));
        return;
    }
    QString path = QFileDialog::getSaveFileName(
        this, tr("Export ASC"), {}, tr("ASC Files (*.asc);;All Files (*)"));
    if (path.isEmpty()) return;
    if (!path.endsWith(".asc")) path += ".asc";
    QList<socketspy::core::CanFrame> list(frames.begin(), frames.end());
    if (AscWriter::write(list, path))
        statusBar()->showMessage(
            tr("ASC exported: %1 frames → %2").arg(list.size()).arg(path), 5000);
    else
        QMessageBox::critical(this, tr("Export ASC"), tr("Failed to write ASC file."));
}

void MainWindow::onImportAsc() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Import ASC"), {}, tr("ASC Files (*.asc);;All Files (*)"));
    if (path.isEmpty()) return;
    const auto frames = AscReader::read(path);
    if (frames.isEmpty()) {
        QMessageBox::information(this, tr("Import ASC"), tr("No frames found in file."));
        return;
    }
    QVector<QPair<double, socketspy::core::CanFrame>> replayFrames;
    QVector<QString> ifaces;
    replayFrames.reserve(frames.size());
    ifaces.reserve(frames.size());
    for (const auto& fr : frames) {
        replayFrames.append({static_cast<double>(fr.timestamp_us) / 1'000'000.0, fr});
        ifaces.append("1");
    }
    m_replay->loadFrames(replayFrames, ifaces, QFileInfo(path).fileName());
    m_sidebar->showPanel(m_replay);
    statusBar()->showMessage(
        tr("ASC imported: %1 frames from %2").arg(frames.size()).arg(path), 5000);
}

void MainWindow::onExportTrc() {
    auto stdFrames = collectMonitorFrames(m_monitor);
    if (stdFrames.empty()) {
        QMessageBox::information(this, tr("Export TRC"), tr("No visible data to export."));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export TRC"), {}, tr("PEAK Trace Files (*.trc);;All Files (*)"));
    if (path.isEmpty()) return;
    const QString outPath = path.endsWith(".trc") ? path : path + ".trc";
    QList<socketspy::core::CanFrame> frames;
    frames.reserve(static_cast<int>(stdFrames.size()));
    for (const auto& f : stdFrames) frames.append(f);
    if (TrcWriter::write(frames, outPath))
        statusBar()->showMessage(
            tr("TRC exported: %1 frames → %2").arg(frames.size()).arg(outPath), 5000);
    else
        QMessageBox::critical(this, tr("Export TRC"), tr("Failed to write TRC file."));
}

void MainWindow::onImportTrc() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Import TRC"), {}, tr("PEAK Trace Files (*.trc);;All Files (*)"));
    if (path.isEmpty()) return;
    const QList<socketspy::core::CanFrame> frames = TrcReader::read(path);
    if (frames.isEmpty()) {
        QMessageBox::information(this, tr("Import TRC"), tr("No frames found in file."));
        return;
    }
    for (const auto& f : frames)
        m_monitor->onFrameReceived(f);
    statusBar()->showMessage(
        tr("TRC imported: %1 frames from %2").arg(frames.size()).arg(path), 5000);
}

void MainWindow::onExportPcap() {
    auto frames = collectMonitorFrames(m_monitor);
    if (frames.empty()) {
        QMessageBox::information(this, tr("Export PCAP"), tr("No visible data to export."));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export PCAP"), {}, tr("PCAP Files (*.pcap);;All Files (*)"));
    if (path.isEmpty()) return;
    const QString outPath = path.endsWith(".pcap") ? path : path + ".pcap";
    if (PcapWriter::write(outPath, frames))
        statusBar()->showMessage(
            tr("PCAP exported: %1 frames → %2").arg(frames.size()).arg(outPath), 5000);
    else
        QMessageBox::critical(this, tr("Export PCAP"), tr("Failed to write PCAP file."));
}

void MainWindow::onImportPcap() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Import PCAP"), {}, tr("PCAP Files (*.pcap);;All Files (*)"));
    if (path.isEmpty()) return;
    const QList<socketspy::core::CanFrame> frames = PcapReader::read(path);
    if (frames.isEmpty()) {
        QMessageBox::warning(this, tr("Import PCAP"),
            tr("No CAN frames found (check link type is LINKTYPE_CAN_SOCKETCAN)."));
        return;
    }
    for (const auto& frame : frames)
        m_monitor->onFrameReceived(frame);
    statusBar()->showMessage(
        tr("PCAP imported: %1 frames from %2").arg(frames.size()).arg(path), 5000);
}

void MainWindow::onImportCsv() {
    QString path = QFileDialog::getOpenFileName(
        this, tr("Import CSV"), {}, tr("CSV files (*.csv)"));
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Import CSV"), f.errorString());
        return;
    }
    QVector<QPair<double, socketspy::core::CanFrame>> frames;
    QTextStream in(&f);
    if (!in.atEnd()) in.readLine(); // skip header
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        QStringList cols = line.split(',');
        if (cols.size() < 3) continue;
        socketspy::core::CanFrame fr{};
        fr.timestamp_us = cols[0].toULongLong();
        bool ok = false;
        fr.id = cols[1].trimmed().toUInt(&ok, 16);
        if (!ok) continue;
        fr.dlc = static_cast<uint8_t>(cols[2].trimmed().toUInt());
        if (cols.size() >= 4) {
            const QStringList bytes = cols[3].trimmed().split(' ', Qt::SkipEmptyParts);
            int nb = qMin(bytes.size(), 64);
            for (int i = 0; i < nb; ++i)
                fr.data[i] = static_cast<uint8_t>(bytes[i].toUInt(nullptr, 16));
        }
        frames.append({static_cast<double>(fr.timestamp_us) / 1'000'000.0, fr});
    }
    if (frames.isEmpty()) {
        QMessageBox::information(this, tr("Import CSV"), tr("No valid frames found in file."));
        return;
    }
    QVector<QString> ifaces(frames.size());
    m_replay->loadFrames(frames, ifaces, QFileInfo(path).fileName());
    m_sidebar->showPanel(m_replay);
    statusBar()->showMessage(tr("CSV imported: %1 frames").arg(frames.size()), 5000);
}

void MainWindow::onCheckForUpdates() {
    auto* dlg = new UpdateDialog(&m_updater, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
    dlg->startCheck();
}

} // namespace socketspy::gui
