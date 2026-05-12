#include "can_iface_config.h"
#include <QProcess>
#include <QFile>

namespace socketspy::gui {

static const QString kHelper = "/usr/local/bin/socketspy-can-setup";

bool canIfaceIsVirtual(const QString& iface) {
    return iface.startsWith("vcan") || iface.startsWith("slcan:");
}

int canIfaceReadBitrate(const QString& iface) {
    QFile f("/sys/class/net/" + iface + "/can_bittiming/bitrate");
    if (!f.open(QIODevice::ReadOnly)) return 0;
    return f.readAll().trimmed().toInt();
}

// Tries the NOPASSWD helper first (no prompt), then pkexec as fallback (one prompt).
static bool runPrivileged(const QString& shellCmd, const QStringList& helperArgs) {
    if (QFile::exists(kHelper)) {
        QProcess p;
        p.start("sudo", QStringList{"-n", kHelper} + helperArgs);
        p.waitForFinished(8000);
        if (p.exitCode() == 0) return true;
    }
    QProcess p;
    p.start("pkexec", {"sh", "-c", shellCmd});
    p.waitForFinished(8000);
    return p.exitCode() == 0;
}

bool canIfaceApply(const QString& iface, int bitrate_bps) {
    if (canIfaceIsVirtual(iface)) return true;
    return runPrivileged(
        QString("ip link set %1 down; ip link set %1 up type can bitrate %2")
            .arg(iface).arg(bitrate_bps),
        {"apply", iface, QString::number(bitrate_bps)});
}

static int bitrateToSlcanSpeed(int bps) {
    if (bps <= 10000)  return 0;
    if (bps <= 20000)  return 1;
    if (bps <= 50000)  return 2;
    if (bps <= 100000) return 3;
    if (bps <= 125000) return 4;
    if (bps <= 250000) return 5;
    if (bps <= 500000) return 6;
    if (bps <= 800000) return 7;
    return 8;
}

std::pair<bool, QString> canSlcanSetup(const QString& devPath, int bitrate_bps) {
    QProcess::execute("pkill", {"slcand"});
    const QString speed = QString::number(bitrateToSlcanSpeed(bitrate_bps));
    bool ok = runPrivileged(
        QString("slcand -o -c -s%1 %2 && ip link set slcan0 up").arg(speed, devPath),
        {"slcan", devPath, speed});
    return ok ? std::pair{true, QString("slcan0")} : std::pair{false, QString{}};
}

} // namespace socketspy::gui
