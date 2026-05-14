#pragma once
#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QDoubleSpinBox>
#include <vector>
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
    void onNewProfile();
    void onDeleteProfile();
    void onAnimationTick();

private:
    struct SignalWidget {
        int ni, mi, si;
        QDoubleSpinBox* spin;
        bool animated;
    };

    void setupUi();
    void populateProfiles();
    void populateTree();
    void connectSignalWidgets();
    void updateDeleteButton();

    QComboBox*   m_profileCombo{nullptr};
    QPushButton* m_startBtn{nullptr};
    QPushButton* m_newBtn{nullptr};
    QPushButton* m_deleteBtn{nullptr};
    QTreeWidget* m_tree{nullptr};

    CanSimulator*            m_simulator{nullptr};
    SimProfile               m_currentProfile;
    std::vector<SignalWidget> m_signalWidgets;
};

} // namespace socketspy::gui
