#pragma once
#include <QWidget>
#include <QListWidget>
#include "project_registry.h"

namespace socketspy::gui {

class WelcomeScreen : public QWidget {
    Q_OBJECT

public:
    explicit WelcomeScreen(ProjectRegistry& registry, QWidget* parent = nullptr);

    void refreshRecentProjects();

signals:
    void newProjectRequested();
    void openProjectRequested(const QString& path);   // empty = show browser
    void openDbcRequested();
    void quickConnectRequested();        // "Connecter vcan0"
    void showSimulatorRequested();       // navigate to Simulator tab
    void showMonitorRequested();         // navigate to Monitor tab

private slots:
    void onItemDoubleClicked(QListWidgetItem* item);
    void onOpenSelectedProject();

private:
    void buildUi();
    QWidget* buildHeader();
    QWidget* buildRecentSection();
    QWidget* buildQuickActionsSection();
    QWidget* buildQuickStartSection();

    ProjectRegistry& m_registry;
    QListWidget*     m_recentList{nullptr};
};

} // namespace socketspy::gui
