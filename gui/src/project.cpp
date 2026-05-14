#include "project.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace socketspy::gui {

static QJsonObject filterToJson(const FrameFilter& f) {
    return {{"enabled",f.enabled},{"id_min",(int)f.id_min},{"id_max",(int)f.id_max},
            {"id_mask",(int)f.id_mask},{"dlc_min",f.dlc_min},{"dlc_max",f.dlc_max},
            {"show_std",f.show_std},{"show_ext",f.show_ext},
            {"show_fd",f.show_fd},{"show_err",f.show_err}};
}

static FrameFilter filterFromJson(const QJsonObject& o) {
    FrameFilter f;
    f.enabled  = o["enabled"].toBool();
    f.id_min   = (uint32_t)o["id_min"].toInt(0);
    f.id_max   = (uint32_t)o["id_max"].toInt(0x1FFFFFFF);
    f.id_mask  = (uint32_t)o["id_mask"].toInt(0xFFFFFFFF);
    f.dlc_min  = o["dlc_min"].toInt(0);
    f.dlc_max  = o["dlc_max"].toInt(15);
    f.show_std = o["show_std"].toBool(true);
    f.show_ext = o["show_ext"].toBool(true);
    f.show_fd  = o["show_fd"].toBool(true);
    f.show_err = o["show_err"].toBool(false);
    return f;
}

static QJsonObject triggerToJson(const TriggerConfig& t) {
    QJsonArray data, mask;
    for (int i = 0; i < t.data_len; ++i) { data.append(t.data[i]); mask.append(t.data_mask[i]); }
    return {{"enabled",t.enabled},{"id",(int)t.id},{"id_mask",(int)t.id_mask},
            {"action",(int)t.action},{"data_len",t.data_len},
            {"data",data},{"data_mask",mask}};
}

static TriggerConfig triggerFromJson(const QJsonObject& o) {
    TriggerConfig t;
    t.enabled  = o["enabled"].toBool();
    t.id       = (uint32_t)o["id"].toInt();
    t.id_mask  = (uint32_t)o["id_mask"].toInt(0xFFFFFFFF);
    t.data_len = (uint8_t)o["data_len"].toInt();
    t.action   = (TriggerConfig::Action)o["action"].toInt();
    const auto data  = o["data"].toArray();
    const auto dmask = o["data_mask"].toArray();
    for (int i = 0; i < (int)t.data_len && i < 8; ++i) {
        t.data[i]      = (uint8_t)data[i].toInt();
        t.data_mask[i] = (uint8_t)dmask[i].toInt();
    }
    return t;
}

bool projectSave(const ProjectData& p, const QString& path, QString& error) {
    QJsonArray sigs;
    for (const auto& s : p.graphSignals)
        sigs.append(QJsonObject{{"name",s.name},{"label",s.label},
                                {"msg_id",(int)s.msgId},
                                {"is_raw",s.isRaw},{"byte_idx",s.rawByteIdx}});

    QJsonObject aliases;
    for (auto it = p.signalAliases.begin(); it != p.signalAliases.end(); ++it)
        aliases[it.key()] = it.value();

    QJsonObject root;
    root["version"]        = 1;
    root["iface"]          = p.iface;
    root["bitrate"]        = p.bitrate;
    root["dbc_path"]       = p.dbcPath;
    root["log_path"]       = p.logPath;
    root["graph_signals"]  = sigs;
    root["signal_aliases"] = aliases;
    root["filter"]         = filterToJson(p.filter);
    root["trigger"]        = triggerToJson(p.trigger);

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) { error = f.errorString(); return false; }
    f.write(QJsonDocument(root).toJson());
    return true;
}

bool projectLoad(ProjectData& p, const QString& path, QString& error) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { error = f.errorString(); return false; }
    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (doc.isNull()) { error = err.errorString(); return false; }

    const auto root = doc.object();
    p.iface   = root["iface"].toString("vcan0");
    p.bitrate = root["bitrate"].toInt(500000);
    p.dbcPath = root["dbc_path"].toString();
    p.logPath = root["log_path"].toString();

    p.graphSignals.clear();
    for (const auto& sv : root["graph_signals"].toArray()) {
        const auto so = sv.toObject();
        p.graphSignals.append({so["name"].toString(),
                               so["label"].toString(),
                               (uint32_t)so["msg_id"].toInt(),
                               so["is_raw"].toBool(),
                               so["byte_idx"].toInt()});
    }

    p.signalAliases.clear();
    const auto aliasObj = root["signal_aliases"].toObject();
    for (auto it = aliasObj.begin(); it != aliasObj.end(); ++it)
        p.signalAliases[it.key()] = it.value().toString();

    p.filter  = filterFromJson(root["filter"].toObject());
    p.trigger = triggerFromJson(root["trigger"].toObject());
    return true;
}

} // namespace socketspy::gui
