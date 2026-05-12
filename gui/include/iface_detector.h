#pragma once
#include <QStringList>

namespace socketspy::gui {

class IfaceDetector {
public:
    static QStringList scanCanIfaces();
    static QStringList scanSlcanDevices();
};

} // namespace socketspy::gui
