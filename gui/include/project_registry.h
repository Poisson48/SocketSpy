#pragma once
#include <QString>
#include <QList>
#include <QDateTime>

namespace socketspy::gui {

struct ProjectEntry {
    QString  name;        // basename without extension
    QString  path;        // absolute path to .spyproj
    QDateTime lastOpened;
};

class ProjectRegistry {
public:
    ProjectRegistry();

    void              add(const QString& path);
    void              remove(const QString& path);
    QList<ProjectEntry> entries() const { return m_entries; }

private:
    void load();
    void save() const;

    QList<ProjectEntry> m_entries;
    QString             m_filePath;
};

} // namespace socketspy::gui
