#pragma once
#include <QDialog>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include "project_registry.h"

class QVBoxLayout;

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
    void checkForUpdatesRequested();
    void fixCanPermissionsRequested();
    void fixUdevRulesRequested();

private slots:
    void onItemDoubleClicked(QListWidgetItem* item);
    void onOpenSelectedProject();

protected:
    void showEvent(QShowEvent* e) override;

private:
    void buildUi();
    QWidget* buildLeftPanel();
    QWidget* buildRightPanel();
    void buildPermissionsSection(QVBoxLayout* parent, QWidget* container);
    void refreshPermissions();

    static QLabel* makeLink(const QString& icon, const QString& label,
                            const QString& url, QWidget* parent);

    struct PermRow {
        QLabel*      icon{nullptr};
        QLabel*      text{nullptr};
        QPushButton* fix{nullptr};
    };

    ProjectRegistry& m_registry;
    QListWidget*     m_recentList{nullptr};
    PermRow          m_serialRow;
    PermRow          m_sudoRow;
    PermRow          m_udevRow;
};

} // namespace socketspy::gui
