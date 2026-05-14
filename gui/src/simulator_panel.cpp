#include "simulator_panel.h"
#include "sim_profile.h"
#include "sim_profile_editor.h"
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

namespace socketspy::gui {

static const QStringList kBuiltinPaths = {
    ":/vehicles/renault_megane2.json",
    ":/vehicles/generic_car.json",
    ":/vehicles/electric_scooter.json",
};

// Format a CAN ID as "0x0C6 @ 10ms" without uppercasing prefix/suffix
static QString fmtMsgLabel(uint32_t id, int period_ms) {
    return "0x" + QString::number(id, 16).toUpper().rightJustified(3, QChar('0'))
         + QString(" @ %1ms").arg(period_ms);
}

SimulatorPanel::SimulatorPanel(QWidget* parent) : QWidget(parent) {
    m_simulator = new CanSimulator(this);
    setupUi();
    populateProfiles();
}

void SimulatorPanel::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(4);

    // ── Top bar: profile selector + actions ──────────────────────────────────
    auto* topBar = new QHBoxLayout;
    topBar->setSpacing(4);
    topBar->addWidget(new QLabel("Profil :", this));
    m_profileCombo = new QComboBox(this);
    m_profileCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    topBar->addWidget(m_profileCombo, 1);

    m_newBtn    = new QPushButton("Nouveau…", this);
    m_deleteBtn = new QPushButton("Supprimer", this);
    m_deleteBtn->setEnabled(false);
    m_deleteBtn->setToolTip("Supprime ce profil (profils intégrés non supprimables)");
    topBar->addWidget(m_newBtn);
    topBar->addWidget(m_deleteBtn);

    m_resetBtn = new QPushButton("⏮", this);
    m_resetBtn->setFixedWidth(30);
    m_resetBtn->setEnabled(false);
    m_resetBtn->setToolTip("Remettre le scénario au début");
    topBar->addWidget(m_resetBtn);

    m_startBtn = new QPushButton("▶  Démarrer", this);
    m_startBtn->setCheckable(true);
    m_startBtn->setMinimumWidth(110);
    topBar->addWidget(m_startBtn);
    root->addLayout(topBar);

    // ── Profile description ──────────────────────────────────────────────────
    m_descLabel = new QLabel(this);
    m_descLabel->setStyleSheet("color: #888; font-style: italic; padding: 0 2px;");
    m_descLabel->setWordWrap(true);
    m_descLabel->hide();
    root->addWidget(m_descLabel);

    // ── Scenario progress bar ────────────────────────────────────────────────
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 1000);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(5);
    m_progressBar->setToolTip("Position dans la boucle du scénario");
    m_progressBar->hide();
    root->addWidget(m_progressBar);

    // ── Signal tree ──────────────────────────────────────────────────────────
    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(4);
    m_tree->setHeaderLabels({"Signal", "Valeur", "Plage", "Forme d'onde"});
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_tree->header()->resizeSection(1, 110);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_tree->header()->resizeSection(3, 90);
    m_tree->setAlternatingRowColors(false);
    m_tree->setIndentation(16);
    root->addWidget(m_tree, 1);

    // ── Connections ──────────────────────────────────────────────────────────
    connect(m_startBtn,    &QPushButton::toggled,  this, &SimulatorPanel::onStartStop);
    connect(m_profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SimulatorPanel::onProfileChanged);
    connect(m_newBtn,    &QPushButton::clicked, this, &SimulatorPanel::onNewProfile);
    connect(m_deleteBtn, &QPushButton::clicked, this, &SimulatorPanel::onDeleteProfile);
    connect(m_resetBtn,  &QPushButton::clicked, this, [this]() {
        m_simulator->resetElapsed();
        m_progressBar->setValue(0);
    });
    connect(m_simulator, &CanSimulator::frameGenerated,
            this, &SimulatorPanel::frameGenerated);
    connect(m_simulator, &CanSimulator::animationTick,
            this, &SimulatorPanel::onAnimationTick);
}

void SimulatorPanel::populateProfiles() {
    const QString currentPath = m_profileCombo->count() > 0
        ? m_profileCombo->currentData().toString() : QString{};

    m_profileCombo->blockSignals(true);
    m_profileCombo->clear();

    // Built-in profiles
    for (const QString& path : kBuiltinPaths) {
        SimProfile p = load_sim_profile(path);
        if (!p.name.isEmpty())
            m_profileCombo->addItem("⚙ " + p.name, path);
    }

    // Custom profiles (user-created)
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                        + "/profiles";
    const auto customFiles = QDir(dir).entryInfoList({"*.json"}, QDir::Files, QDir::Name);
    if (!customFiles.isEmpty()) {
        m_profileCombo->insertSeparator(m_profileCombo->count());
        for (const QFileInfo& fi : customFiles) {
            SimProfile p = load_sim_profile(fi.absoluteFilePath());
            if (!p.name.isEmpty())
                m_profileCombo->addItem("✎ " + p.name, fi.absoluteFilePath());
        }
    }

    m_profileCombo->blockSignals(false);

    int idx = m_profileCombo->findData(currentPath);
    if (idx >= 0)
        m_profileCombo->setCurrentIndex(idx);
    else if (m_profileCombo->count() > 0)
        onProfileChanged(0);

    updateDeleteButton();
}

void SimulatorPanel::updateDeleteButton() {
    const int idx = m_profileCombo->currentIndex();
    if (idx < 0) { m_deleteBtn->setEnabled(false); return; }
    const QString path = m_profileCombo->itemData(idx).toString();
    m_deleteBtn->setEnabled(!path.startsWith(':') && !m_simulator->isRunning());
}

void SimulatorPanel::updateRunningState(bool running) {
    m_startBtn->setText(running ? "■  Arrêter" : "▶  Démarrer");
    m_resetBtn->setEnabled(running);
    m_progressBar->setVisible(running && m_simulator->scenarioDuration() > 0);
    if (!running) m_progressBar->setValue(0);
    updateDeleteButton();
}

void SimulatorPanel::onProfileChanged(int index) {
    if (index < 0 || index >= m_profileCombo->count()) return;
    if (m_simulator->isRunning()) {
        m_simulator->stop();
        // Block signals to avoid double-triggering onStartStop via toggled()
        m_startBtn->blockSignals(true);
        m_startBtn->setChecked(false);
        m_startBtn->blockSignals(false);
        updateRunningState(false);
    }
    const QString path = m_profileCombo->itemData(index).toString();
    m_currentProfile = load_sim_profile(path);
    m_simulator->load(m_currentProfile);

    m_descLabel->setText(m_currentProfile.description);
    m_descLabel->setVisible(!m_currentProfile.description.isEmpty());

    populateTree();
    updateDeleteButton();
}

void SimulatorPanel::populateTree() {
    m_signalWidgets.clear();  // Clear before tree (avoids dangling spin pointers during clear)
    m_tree->clear();

    // Fonts for hierarchy levels
    QFont nodeFont = m_tree->font();
    nodeFont.setBold(true);
    nodeFont.setPointSize(nodeFont.pointSize());

    QFont msgFont = m_tree->font();
    msgFont.setItalic(true);

    // Accent color for animated signals
    static const QColor kAnimColor(80, 180, 255);
    static const QColor kNodeColor(210, 210, 210);
    static const QColor kMsgColor(160, 160, 160);

    for (int ni = 0; ni < (int)m_currentProfile.nodes.size(); ++ni) {
        const auto& node = m_currentProfile.nodes[ni];
        auto* nodeItem = new QTreeWidgetItem(m_tree, {node.name});
        nodeItem->setFont(0, nodeFont);
        nodeItem->setForeground(0, kNodeColor);
        nodeItem->setExpanded(true);

        for (int mi = 0; mi < (int)node.messages.size(); ++mi) {
            const auto& msg = node.messages[mi];
            auto* msgItem = new QTreeWidgetItem(nodeItem, {fmtMsgLabel(msg.id, msg.period_ms)});
            msgItem->setFont(0, msgFont);
            msgItem->setForeground(0, kMsgColor);
            msgItem->setExpanded(true);

            for (int si = 0; si < (int)msg.sigs.size(); ++si) {
                const auto& sig = msg.sigs[si];
                bool animated = !sig.scenario.empty();

                QString range = animated
                    ? QString("[%1 – %2]  ▶ auto").arg(sig.min).arg(sig.max)
                    : QString("[%1 – %2]").arg(sig.min).arg(sig.max);

                auto* sigItem = new QTreeWidgetItem(msgItem, {sig.name, "", range});
                if (animated) {
                    sigItem->setForeground(0, kAnimColor);
                    sigItem->setForeground(2, kAnimColor);
                    sigItem->setToolTip(0, QString("Signal animé automatiquement (%1 points)")
                                            .arg(sig.scenario.size()));
                } else {
                    sigItem->setToolTip(0, "Valeur manuelle – modifiable pendant l'émission");
                }

                auto* spin = new QDoubleSpinBox(m_tree);
                spin->setRange(sig.min, sig.max);
                spin->setValue(sig.current_value);
                spin->setSingleStep((sig.max - sig.min) / 100.0);
                spin->setDecimals(2);
                spin->setReadOnly(animated);
                spin->setButtonSymbols(animated ? QAbstractSpinBox::NoButtons
                                                 : QAbstractSpinBox::UpDownArrows);
                if (animated)
                    spin->setStyleSheet("color: #50b4ff; background: transparent; border: none;");
                m_tree->setItemWidget(sigItem, 1, spin);

                if (!animated) {
                    connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                        this, [this, ni, mi, si](double v) {
                            m_simulator->setSignalValue(ni, mi, si, v);
                        });
                }

                // Waveform configure button (always present for signals)
                auto* waveBtn = new QPushButton("⚙ Onde", m_tree);
                waveBtn->setToolTip("Configurer la forme d'onde pour ce signal");
                waveBtn->setFixedHeight(20);
                waveBtn->setStyleSheet(animated
                    ? "font-size:10px; padding:0 4px; color:#50b4ff;"
                    : "font-size:10px; padding:0 4px;");
                connect(waveBtn, &QPushButton::clicked, this,
                    [this, ni, mi, si]() { openWaveformConfig(ni, mi, si); });
                m_tree->setItemWidget(sigItem, 3, waveBtn);

                m_signalWidgets.push_back({ni, mi, si, spin, animated, waveBtn});
            }
        }
    }
}

void SimulatorPanel::onAnimationTick() {
    // Update animated spinboxes with live values
    for (const auto& sw : m_signalWidgets) {
        if (!sw.animated) continue;
        double v = m_simulator->signalValue(sw.ni, sw.mi, sw.si);
        sw.spin->blockSignals(true);
        sw.spin->setValue(v);
        sw.spin->blockSignals(false);
    }

    // Update scenario progress bar
    int64_t dur = m_simulator->scenarioDuration();
    if (dur > 0)
        m_progressBar->setValue((int)((m_simulator->elapsedMs() % dur) * 1000 / dur));
}

void SimulatorPanel::onStartStop() {
    const bool running = m_startBtn->isChecked();
    if (running)
        m_simulator->start();
    else
        m_simulator->stop();
    updateRunningState(running);
}

void SimulatorPanel::onNewProfile() {
    SimProfileEditor editor(this);
    if (editor.exec() != QDialog::Accepted) return;
    populateProfiles();
    const int idx = m_profileCombo->findData(editor.savedPath());
    if (idx >= 0) m_profileCombo->setCurrentIndex(idx);
}

void SimulatorPanel::onDeleteProfile() {
    const int idx = m_profileCombo->currentIndex();
    if (idx < 0) return;
    const QString path = m_profileCombo->itemData(idx).toString();
    if (path.startsWith(':')) return;

    const QString name = m_currentProfile.name;
    const auto btn = QMessageBox::question(this, "Supprimer le profil",
        "Supprimer le profil \"" + name + "\" ?",
        QMessageBox::Yes | QMessageBox::No);
    if (btn != QMessageBox::Yes) return;

    QFile::remove(path);
    populateProfiles();
}

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
    m_infoLabel->setStyleSheet("color: gray; font-size: 10px;");
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

    populateTree();
}

} // namespace socketspy::gui
