#pragma once
#include <QWidget>
#include <QDialog>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QTreeWidget>
#include <QListWidget>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QTableWidget>
#include <QLineEdit>
#include <vector>
#include "can_simulator.h"
#include "sim_profile.h"
#include "cancore.h"
#include "uds_ecu_sim.h"

namespace socketspy::gui {

// ---------------------------------------------------------------------------
// Non-modal dialog for configuring a signal's waveform parameters.
class WaveformConfigDialog : public QDialog {
    Q_OBJECT
public:
    explicit WaveformConfigDialog(const SimSignal& sig, QWidget* parent = nullptr);

    WaveformType waveform()   const;
    double       min()        const;
    double       max()        const;
    int          numPoints()  const;
    int          stepMs()     const;

private slots:
    void updateCycleInfo();

private:
    QComboBox*      m_waveCombo{nullptr};
    QDoubleSpinBox* m_minSpin{nullptr};
    QDoubleSpinBox* m_maxSpin{nullptr};
    QSpinBox*       m_ptsSpin{nullptr};
    QSpinBox*       m_stepSpin{nullptr};
    QLabel*         m_infoLabel{nullptr};
};

// ---------------------------------------------------------------------------

class SimulatorPanel : public QWidget {
    Q_OBJECT

public:
    explicit SimulatorPanel(QWidget* parent = nullptr);

    // Returns the display text of the currently selected profile combo entry.
    QString currentProfileName() const { return m_profileCombo ? m_profileCombo->currentText() : QString(); }

    // Selects the profile whose combo text matches name (no-op if not found).
    void setProfileByName(const QString& name);

signals:
    void frameGenerated(socketspy::core::CanFrame frame);

public slots:
    void onFrameReceived(const socketspy::core::CanFrame& frame);

private slots:
    void onStartStop();
    void onProfileChanged(int index);
    void onNewProfile();
    void onEditProfile();
    void onDeleteProfile();
    void onAnimationTick();
    void onAddEcu();
    void onEditEcu();
    void onRemoveEcu();

private:
    struct SignalWidget {
        int ni, mi, si;
        QDoubleSpinBox* spin;
        bool animated;
        QPushButton* waveBtn{nullptr}; // "configure waveform" button
    };

    void setupUi();
    void populateProfiles();
    void populateTree();
    void updateDeleteButton();
    void updateRunningState(bool running);
    void openWaveformConfig(int ni, int mi, int si);
    QWidget* setupEcuTab();
    void refreshEcuList();

    QComboBox*    m_profileCombo{nullptr};
    QPushButton*  m_startBtn{nullptr};
    QPushButton*  m_resetBtn{nullptr};
    QPushButton*  m_newBtn{nullptr};
    QPushButton*  m_editBtn{nullptr};
    QPushButton*  m_deleteBtn{nullptr};
    QLabel*       m_descLabel{nullptr};
    QProgressBar* m_progressBar{nullptr};
    QTreeWidget*  m_tree{nullptr};

    // ECU tab
    QListWidget* m_ecuList{nullptr};
    QPushButton* m_addEcuBtn{nullptr};
    QPushButton* m_removeEcuBtn{nullptr};
    QPushButton* m_editEcuBtn{nullptr};
    QList<UdsEcuSim*> m_ecuSims;

    CanSimulator*             m_simulator{nullptr};
    SimProfile                m_currentProfile;
    std::vector<SignalWidget> m_signalWidgets;
};

} // namespace socketspy::gui
