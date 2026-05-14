#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include "project_registry.h"

namespace socketspy::gui {

class ProjectBrowserDialog : public QDialog {
    Q_OBJECT

public:
    explicit ProjectBrowserDialog(ProjectRegistry& registry, QWidget* parent = nullptr);

    // path of the project to open, or empty if user chose "New"
    QString selectedPath() const { return m_selectedPath; }
    bool    wantsNew()     const { return m_wantsNew; }

private slots:
    void onSearchChanged(const QString& text);
    void onItemDoubleClicked(QListWidgetItem* item);
    void onOpenSelected();
    void onOpenFile();
    void onRemoveSelected();
    void onNewProject();
    void onSelectionChanged();

private:
    void rebuildList(const QString& filter = {});

    ProjectRegistry& m_registry;
    QLineEdit*       m_search{nullptr};
    QListWidget*     m_list{nullptr};
    QPushButton*     m_btnOpen{nullptr};
    QPushButton*     m_btnRemove{nullptr};

    QString m_selectedPath;
    bool    m_wantsNew{false};
};

} // namespace socketspy::gui
