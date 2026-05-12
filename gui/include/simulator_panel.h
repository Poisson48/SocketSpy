#pragma once
#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QTreeWidget>
#include "can_simulator.h"
#include "cancore.h"

namespace socketspy::gui {

class SimulatorPanel : public QWidget {
    Q_OBJECT

public:
    explicit SimulatorPanel(QWidget* parent = nullptr);

signals:
    void frameGenerated(socketspy::core::CanFrame frame);

private slots:
    void onStartStop();
    void onProfileChanged(int index);

private:
    void setupUi();
    void populateProfiles();
    void populateTree();
    void connectSignalWidgets();

    QComboBox*   m_profileCombo{nullptr};
    QPushButton* m_startBtn{nullptr};
    QTreeWidget* m_tree{nullptr};

    CanSimulator* m_simulator{nullptr};
    SimProfile    m_currentProfile;
};

} // namespace socketspy::gui
