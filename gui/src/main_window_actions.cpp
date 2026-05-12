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

void MainWindow::setConnStatus(bool active) {
    if (active) {
        m_connStatusLabel->setStyleSheet("color: #33aa33; font-size: 14px;");
        m_connStatusLabel->setToolTip("Interface active");
    } else {
        m_connStatusLabel->setStyleSheet("color: #cc3333; font-size: 14px;");
        m_connStatusLabel->setToolTip("Aucun trafic reçu");
    }
}

void MainWindow::onGrantCanPermissions() {
    const QString user = QProcessEnvironment::systemEnvironment().value("USER");
    if (user.isEmpty()) {
        QMessageBox::critical(this, "Erreur", "Impossible de déterminer le nom d'utilisateur.");
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
        QMessageBox::critical(this, "Erreur", "Impossible de créer les fichiers temporaires.");
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
        QMessageBox::information(this, "Droits CAN configurés",
            "Succès ! Les interfaces CAN peuvent maintenant être configurées\n"
            "sans saisir de mot de passe.");
    else
        QMessageBox::critical(this, "Erreur",
            "Impossible de configurer les droits.\n"
            "Vérifiez que pkexec, sudo et visudo sont installés.");
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
        QMessageBox::critical(this, "Erreur", "Impossible de créer un fichier temporaire.");
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
        QMessageBox::information(this, "Succès",
            "Règles udev installées.\nReconnectez votre adaptateur CAN USB — il sera détecté automatiquement.");
    else
        QMessageBox::critical(this, "Erreur",
            "Impossible d'installer les règles udev.\nVérifiez que pkexec est installé.");
}

} // namespace socketspy::gui
