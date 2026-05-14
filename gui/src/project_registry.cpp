#include "project_registry.h"

#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

namespace socketspy::gui {

ProjectRegistry::ProjectRegistry() {
    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(configDir);
    m_filePath = configDir + "/projects.json";
    load();
}

void ProjectRegistry::add(const QString& path) {
    // move to front if already known, otherwise prepend
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].path == path) {
            m_entries.removeAt(i);
            break;
        }
    }
    ProjectEntry e;
    e.name       = QFileInfo(path).completeBaseName();
    e.path       = path;
    e.lastOpened = QDateTime::currentDateTime();
    m_entries.prepend(e);

    // keep the list bounded
    while (m_entries.size() > 50)
        m_entries.removeLast();

    save();
}

void ProjectRegistry::remove(const QString& path) {
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].path == path) {
            m_entries.removeAt(i);
            break;
        }
    }
    save();
}

void ProjectRegistry::load() {
    QFile f(m_filePath);
    if (!f.open(QIODevice::ReadOnly)) return;

    QJsonArray arr = QJsonDocument::fromJson(f.readAll()).array();
    for (const QJsonValue& v : arr) {
        QJsonObject o = v.toObject();
        ProjectEntry e;
        e.path       = o["path"].toString();
        e.name       = o["name"].toString(QFileInfo(e.path).completeBaseName());
        e.lastOpened = QDateTime::fromString(o["last_opened"].toString(), Qt::ISODate);
        if (!e.path.isEmpty())
            m_entries.append(e);
    }
}

void ProjectRegistry::save() const {
    QJsonArray arr;
    for (const ProjectEntry& e : m_entries) {
        QJsonObject o;
        o["path"]        = e.path;
        o["name"]        = e.name;
        o["last_opened"] = e.lastOpened.toString(Qt::ISODate);
        arr.append(o);
    }
    QFile f(m_filePath);
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(arr).toJson());
}

} // namespace socketspy::gui
