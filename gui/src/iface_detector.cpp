#include "iface_detector.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace socketspy::gui {

namespace {

struct SlcanVidPid { const char* vid; const char* pid; };

constexpr SlcanVidPid k_knownAdapters[] = {
    {"0c72", nullptr},
    {"0bfd", nullptr},
    {"0403", "6001"},
};

bool isKnownSlcanAdapter(const QString& vid, const QString& pid) {
    for (const auto& entry : k_knownAdapters) {
        if (vid != QLatin1String(entry.vid)) continue;
        if (entry.pid == nullptr) return true;
        if (pid == QLatin1String(entry.pid)) return true;
    }
    return false;
}

QString readSysAttr(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QString::fromLatin1(f.readAll()).trimmed().toLower();
}

} // namespace

QStringList IfaceDetector::scanCanIfaces() {
    QStringList result;
    QDir netDir("/sys/class/net");
    const auto entries = netDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& name : entries) {
        QFile typeFile("/sys/class/net/" + name + "/type");
        if (!typeFile.open(QIODevice::ReadOnly)) continue;
        if (typeFile.readAll().trimmed() == "280")
            result << name;
    }
    const QStringList slcan = scanSlcanDevices();
    result.append(slcan);
    if (result.isEmpty()) result << "vcan0";
    return result;
}

QStringList IfaceDetector::scanSlcanDevices() {
    QStringList result;
    QDir devDir("/dev");

    const QStringList patterns = {"ttyUSB*", "ttyACM*"};
    for (const QString& pattern : patterns) {
        const auto entries = devDir.entryList({pattern}, QDir::System);
        for (const QString& dev : entries) {
            const QString devLink = "/sys/class/tty/" + dev + "/device";
            const QString usbDev = QFileInfo(devLink).canonicalFilePath() + "/..";
            const QString usbReal = QFileInfo(usbDev).canonicalFilePath();
            if (usbReal.isEmpty()) continue;
            const QString vid = readSysAttr(usbReal + "/idVendor");
            const QString pid = readSysAttr(usbReal + "/idProduct");
            if (vid.isEmpty()) continue;
            if (isKnownSlcanAdapter(vid, pid))
                result << "slcan:/dev/" + dev;
        }
    }
    return result;
}

} // namespace socketspy::gui
