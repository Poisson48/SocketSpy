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

static bool runIpLink(const QString& iface, int bitrate, bool use_pkexec) {
    auto run = [&](const QStringList& args) -> bool {
        QProcess p;
        if (use_pkexec) {
            p.start("pkexec", QStringList{"ip"} + args);
        } else {
            p.start("ip", args);
        }
        p.waitForFinished(5000);
        return p.exitCode() == 0;
    };

    run({"link", "set", iface, "down"});
    return run({"link", "set", iface, "up", "type", "can",
                "bitrate", QString::number(bitrate)});
}

bool canIfaceApply(const QString& iface, int bitrate_bps) {
    if (canIfaceIsVirtual(iface)) return true;

    if (runIpLink(iface, bitrate_bps, false)) return true;
    // Permission denied — try again with pkexec (shows a GUI auth dialog)
    return runIpLink(iface, bitrate_bps, true);
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

    const QString speedArg = "-s" + QString::number(bitrateToSlcanSpeed(bitrate_bps));
    auto trySlcand = [&](bool usePkexec) -> bool {
        QProcess p;
        QStringList args = {"-o", "-c", speedArg, devPath};
        if (usePkexec) p.start("pkexec", QStringList{"slcand"} + args);
        else           p.start("slcand", args);
        p.waitForFinished(5000);
        return p.exitCode() == 0;
    };
    if (!trySlcand(false) && !trySlcand(true)) return {false, {}};

    auto tryIfUp = [&](bool usePkexec) -> bool {
        QProcess p;
        QStringList args = {"link", "set", "slcan0", "up"};
        if (usePkexec) p.start("pkexec", QStringList{"ip"} + args);
        else           p.start("ip", args);
        p.waitForFinished(3000);
        return p.exitCode() == 0;
    };
    if (!tryIfUp(false) && !tryIfUp(true)) return {false, {}};

    return {true, "slcan0"};
}

} // namespace socketspy::gui
