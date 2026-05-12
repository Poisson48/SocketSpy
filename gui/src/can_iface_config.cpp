#include "can_iface_config.h"
#include <QProcess>
#include <QFile>

namespace socketspy::gui {

bool canIfaceIsVirtual(const QString& iface) {
    return iface.startsWith("vcan") || iface.startsWith("slcan:");
}

int canIfaceReadBitrate(const QString& iface) {
    QFile f("/sys/class/net/" + iface + "/can_bittiming/bitrate");
    if (!f.open(QIODevice::ReadOnly)) return 0;
    return f.readAll().trimmed().toInt();
}

// Runs a shell command, with pkexec elevation if requested.
// Batching both ip-link commands into one sh invocation means a single auth prompt.
static bool runSh(const QString& cmd, bool use_pkexec) {
    QProcess p;
    if (use_pkexec) p.start("pkexec", {"sh", "-c", cmd});
    else            p.start("sh",     {"-c", cmd});
    p.waitForFinished(8000);
    return p.exitCode() == 0;
}

bool canIfaceApply(const QString& iface, int bitrate_bps) {
    if (canIfaceIsVirtual(iface)) return true;
    const QString cmd = QString("ip link set %1 down; ip link set %1 up type can bitrate %2")
                            .arg(iface).arg(bitrate_bps);
    return runSh(cmd, false) || runSh(cmd, true);
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
    const QString cmd = QString("slcand -o -c -s%1 %2 && ip link set slcan0 up")
                            .arg(speed, devPath);
    if (!runSh(cmd, false) && !runSh(cmd, true)) return {false, {}};
    return {true, "slcan0"};
}

} // namespace socketspy::gui
