// simulator_panel_waveform.cpp — WaveformConfigDialog + configuration des
// signaux (waveform) du SimulatorPanel. Extrait de simulator_panel.cpp.
#include "simulator_panel.h"
#include "sim_profile.h"
#include "sim_profile_editor.h"
#include "uds_ecu_sim.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QSpinBox>
#include <QGroupBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QFont>
#include <QTabWidget>
#include <QListWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QCheckBox>
#include <QTabWidget>
#include <QLineEdit>

namespace socketspy::gui {

// ---------------------------------------------------------------------------
// WaveformConfigDialog implementation

static int waveformToIndex(WaveformType w) {
    switch (w) {
    case WaveformType::Sine:   return 1;
    case WaveformType::Ramp:   return 2;
    case WaveformType::Square: return 3;
    case WaveformType::Random: return 4;
    default:                   return 0;
    }
}

static WaveformType indexToWaveform(int i) {
    switch (i) {
    case 1:  return WaveformType::Sine;
    case 2:  return WaveformType::Ramp;
    case 3:  return WaveformType::Square;
    case 4:  return WaveformType::Random;
    default: return WaveformType::None;
    }
}

WaveformConfigDialog::WaveformConfigDialog(const SimSignal& sig, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QString("Forme d'onde — %1").arg(sig.name));
    setMinimumWidth(320);

    auto* root = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    root->addLayout(form);

    auto* nameLabel = new QLabel(sig.name, this);
    nameLabel->setStyleSheet("font-weight:bold;");
    form->addRow("Signal :", nameLabel);

    m_waveCombo = new QComboBox(this);
    m_waveCombo->addItem("Aucune (statique / manuel)", 0);
    m_waveCombo->addItem("Sinusoïde",                  1);
    m_waveCombo->addItem("Rampe (dent de scie)",       2);
    m_waveCombo->addItem("Carré",                      3);
    m_waveCombo->addItem("Aléatoire",                  4);
    m_waveCombo->setCurrentIndex(waveformToIndex(sig.waveform));
    form->addRow("Type :", m_waveCombo);

    m_minSpin = new QDoubleSpinBox(this);
    m_minSpin->setRange(-1e9, 1e9); m_minSpin->setDecimals(4); m_minSpin->setValue(sig.min);
    form->addRow("Min :", m_minSpin);

    m_maxSpin = new QDoubleSpinBox(this);
    m_maxSpin->setRange(-1e9, 1e9); m_maxSpin->setDecimals(4); m_maxSpin->setValue(sig.max);
    form->addRow("Max :", m_maxSpin);

    m_ptsSpin = new QSpinBox(this);
    m_ptsSpin->setRange(2, 10000); m_ptsSpin->setValue(sig.num_points);
    m_ptsSpin->setToolTip("Nombre de points par cycle de la forme d'onde");
    form->addRow("Points/cycle :", m_ptsSpin);

    m_stepSpin = new QSpinBox(this);
    m_stepSpin->setRange(1, 60000); m_stepSpin->setValue(sig.step_ms);
    m_stepSpin->setSuffix(" ms");
    m_stepSpin->setToolTip("Intervalle entre deux pas consécutifs");
    form->addRow("Pas (update) :", m_stepSpin);

    m_infoLabel = new QLabel(this);
    m_infoLabel->setStyleSheet("color: #7c8fa6; font-size: 10px;");
    form->addRow(m_infoLabel);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(btns);
    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(m_ptsSpin,  QOverload<int>::of(&QSpinBox::valueChanged), this, &WaveformConfigDialog::updateCycleInfo);
    connect(m_stepSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &WaveformConfigDialog::updateCycleInfo);
    updateCycleInfo();
}

void WaveformConfigDialog::updateCycleInfo() {
    const int total = m_ptsSpin->value() * m_stepSpin->value();
    m_infoLabel->setText(QString("Durée du cycle : %1 ms (%2 s)")
        .arg(total).arg(total / 1000.0, 0, 'f', 2));
}

WaveformType WaveformConfigDialog::waveform()  const { return indexToWaveform(m_waveCombo->currentIndex()); }
double       WaveformConfigDialog::min()       const { return m_minSpin->value(); }
double       WaveformConfigDialog::max()       const { return m_maxSpin->value(); }
int          WaveformConfigDialog::numPoints() const { return m_ptsSpin->value(); }
int          WaveformConfigDialog::stepMs()    const { return m_stepSpin->value(); }

// ---------------------------------------------------------------------------

void SimulatorPanel::openWaveformConfig(int ni, int mi, int si) {
    const SimSignal* sig = m_simulator->signalAt(ni, mi, si);
    SimSignal fallback;
    if (!sig) {
        if (ni < (int)m_currentProfile.nodes.size() &&
            mi < (int)m_currentProfile.nodes[ni].messages.size() &&
            si < (int)m_currentProfile.nodes[ni].messages[mi].sigs.size())
        {
            fallback = m_currentProfile.nodes[ni].messages[mi].sigs[si];
            sig = &fallback;
        }
        if (!sig) return;
    }

    WaveformConfigDialog dlg(*sig, this);
    if (dlg.exec() != QDialog::Accepted) return;

    const WaveformType newWave = dlg.waveform();
    const double       newMin  = dlg.min();
    const double       newMax  = dlg.max();
    const int          newPts  = dlg.numPoints();
    const int          newStep = dlg.stepMs();

    m_simulator->setSignalWaveform(ni, mi, si, newWave, newMin, newMax, newPts, newStep);

    if (ni < (int)m_currentProfile.nodes.size() &&
        mi < (int)m_currentProfile.nodes[ni].messages.size() &&
        si < (int)m_currentProfile.nodes[ni].messages[mi].sigs.size())
    {
        auto& s      = m_currentProfile.nodes[ni].messages[mi].sigs[si];
        s.waveform   = newWave;
        s.min        = newMin;
        s.max        = newMax;
        s.num_points = newPts;
        s.step_ms    = newStep;
    }

    // Persist waveform changes for custom (non-built-in) profiles
    const int curIdx = m_profileCombo->currentIndex();
    if (curIdx >= 0) {
        const QString profilePath = m_profileCombo->itemData(curIdx).toString();
        if (!profilePath.startsWith(':'))
            save_sim_profile(m_currentProfile, profilePath);
    }

    populateTree();
}

} // namespace socketspy::gui
