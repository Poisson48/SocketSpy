#pragma once
#include <QDialog>
#include <QLabel>
#include <QListWidget>
#include "project_registry.h"

namespace socketspy::gui {

class WelcomeScreen : public QDialog {
    Q_OBJECT

public:
    explicit WelcomeScreen(ProjectRegistry& registry, QWidget* parent = nullptr);

    // Returns false if the user checked "ne plus afficher"
    static bool shouldShow();

    void refreshRecentProjects();

signals:
    void newProjectRequested();
    void openProjectRequested(const QString& path);
    void openDbcRequested();
    void quickConnectRequested();
    void showSimulatorRequested();
    void showMonitorRequested();

private slots:
    void onItemDoubleClicked(QListWidgetItem* item);
    void onOpenSelectedProject();

private:
    void buildUi();
    QWidget* buildLeftPanel();
    QWidget* buildRightPanel();

    static QLabel* makeLink(const QString& icon, const QString& label,
                            const QString& url, QWidget* parent);

    ProjectRegistry& m_registry;
    QListWidget*     m_recentList{nullptr};
};

} // namespace socketspy::gui
