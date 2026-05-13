#pragma once
#include <QString>
#include <QList>
#include "filter_model.h"
#include "trigger_config.h"

namespace socketspy::gui {

struct GraphSignalConfig {
    QString  name;
    uint32_t msgId{0};
    bool     isRaw{false};
    int      rawByteIdx{0};
};

struct ProjectData {
    QString iface   = "vcan0";
    int     bitrate = 500000;
    QString dbcPath;
    QString logPath;
    QList<GraphSignalConfig> graphSignals;
    FrameFilter   filter;
    TriggerConfig trigger;
};

bool projectSave(const ProjectData& p, const QString& path, QString& error);
bool projectLoad(ProjectData& p, const QString& path, QString& error);

} // namespace socketspy::gui
