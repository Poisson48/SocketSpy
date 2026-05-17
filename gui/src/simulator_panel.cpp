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
    // Consistent outer margin 8px, inner spacing 6px
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    // ── Top bar: profile selector + actions ──────────────────────────────────
    auto* topBar = new QHBoxLayout;
    topBar->setSpacing(6);
    topBar->addWidget(new QLabel("Profil :", this));
    m_profileCombo = new QComboBox(this);
    m_profileCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    topBar->addWidget(m_profileCombo, 1);

    m_newBtn    = new QPushButton("Nouveau…", this);
    m_editBtn   = new QPushButton("Éditer…", this);
    m_editBtn->setEnabled(false);
    m_editBtn->setToolTip("Modifier ce profil custom");
    m_deleteBtn = new QPushButton("Supprimer", this);
    m_deleteBtn->setEnabled(false);
    m_deleteBtn->setToolTip("Supprime ce profil (profils intégrés non supprimables)");
    topBar->addWidget(m_newBtn);
    topBar->addWidget(m_editBtn);
    topBar->addWidget(m_deleteBtn);

    m_resetBtn = new QPushButton("↺", this);
    m_resetBtn->setObjectName("resetBtn");
    m_resetBtn->setFixedWidth(30);
    m_resetBtn->setEnabled(false);
    m_resetBtn->setToolTip("Remettre le scénario au début");
    topBar->addWidget(m_resetBtn);

    m_startBtn = new QPushButton("▶  Démarrer", this);
    m_startBtn->setCheckable(true);
    m_startBtn->setMinimumWidth(110);
    topBar->addWidget(m_startBtn);
    root->addLayout(topBar);

    // ── Tab widget: Signaux / ECUs UDS ───────────────────────────────────────
    auto* tabs = new QTabWidget(this);

    // --- Tab 0: Signaux ---
    auto* signalTab = new QWidget;
    auto* signalLayout = new QVBoxLayout(signalTab);
    signalLayout->setContentsMargins(0, 4, 0, 0);
    signalLayout->setSpacing(4);

    // ── Profile description ──────────────────────────────────────────────────
    m_descLabel = new QLabel(signalTab);
    // Use theme muted color (#7c8fa6) instead of hardcoded #888
    m_descLabel->setStyleSheet("color: #7c8fa6; font-style: italic; padding: 0 2px;");
    m_descLabel->setWordWrap(true);
    m_descLabel->hide();
    signalLayout->addWidget(m_descLabel);

    // ── Scenario progress bar ────────────────────────────────────────────────
    m_progressBar = new QProgressBar(signalTab);
    m_progressBar->setRange(0, 1000);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(5);
    m_progressBar->setToolTip("Position dans la boucle du scénario");
    m_progressBar->hide();
    signalLayout->addWidget(m_progressBar);

    // ── Signal tree ──────────────────────────────────────────────────────────
    m_tree = new QTreeWidget(signalTab);
    m_tree->setColumnCount(4);
    m_tree->setHeaderLabels({"Signal", "Valeur", "Plage", "Forme d'onde"});
    m_tree->header()->setMinimumSectionSize(60);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_tree->header()->resizeSection(0, 150);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_tree->header()->resizeSection(1, 110);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_tree->header()->resizeSection(3, 80);
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSortIndicatorShown(false);
    m_tree->setAlternatingRowColors(false);
    m_tree->setIndentation(16);
    signalLayout->addWidget(m_tree, 1);

    tabs->addTab(signalTab, "Signaux");
    tabs->addTab(setupEcuTab(), "ECUs UDS");
    root->addWidget(tabs, 1);

    // ── Connections ──────────────────────────────────────────────────────────
    connect(m_startBtn,    &QPushButton::toggled,  this, &SimulatorPanel::onStartStop);
    connect(m_profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SimulatorPanel::onProfileChanged);
    connect(m_newBtn,    &QPushButton::clicked, this, &SimulatorPanel::onNewProfile);
    connect(m_editBtn,   &QPushButton::clicked, this, &SimulatorPanel::onEditProfile);
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
    if (idx < 0) {
        m_deleteBtn->setEnabled(false);
        m_editBtn->setVisible(false);
        return;
    }
    const QString path = m_profileCombo->itemData(idx).toString();
    const bool isCustom = !path.startsWith(':');
    m_deleteBtn->setEnabled(isCustom && !m_simulator->isRunning());
    m_editBtn->setVisible(isCustom);
    m_editBtn->setEnabled(isCustom && !m_simulator->isRunning());
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

    // Accent color for animated signals — theme cyan #06b6d4
    static const QColor kAnimColor(6, 182, 212);
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
                spin->setMinimumWidth(115);
                spin->setReadOnly(animated);
                spin->setButtonSymbols(animated ? QAbstractSpinBox::NoButtons
                                                 : QAbstractSpinBox::UpDownArrows);
                if (animated)
                    // Use theme cyan (#06b6d4) — read-only animated display, no border
                    spin->setStyleSheet("color: #06b6d4; background: transparent; border: none; padding-right: 6px;");
                else
                    spin->setStyleSheet("padding-right: 6px;");

                // Ensure the row is tall enough to fully display the embedded spinbox.
                // Qt sizes item rows from the item's own sizeHint (text/icon based) and
                // ignores the height of widgets set via setItemWidget — without this the
                // spinbox (and waveBtn) get vertically clipped to roughly half their height.
                const int rowH = qMax(spin->sizeHint().height(), 26) + 4;
                sigItem->setSizeHint(0, QSize(0, rowH));

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
                waveBtn->setProperty("secondary", true);
                waveBtn->setStyleSheet(animated
                    // Use theme cyan (#06b6d4) for animated waveform button accent
                    ? "font-size:10px; padding:0 4px; color:#06b6d4;"
                    : "font-size:10px; padding:0 4px; background:transparent; border:1px solid #3d5270; color:#7c8fa6;");
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

void SimulatorPanel::onEditProfile() {
    const int idx = m_profileCombo->currentIndex();
    if (idx < 0) return;
    const QString path = m_profileCombo->itemData(idx).toString();
    if (path.startsWith(':')) return;  // built-in profiles are not editable

    SimProfileEditor editor(this);
    editor.loadProfile(m_currentProfile);
    if (editor.exec() != QDialog::Accepted) return;

    // Re-save to the same path and reload
    const SimProfile updated = editor.result();
    save_sim_profile(updated, path);
    populateProfiles();
    const int newIdx = m_profileCombo->findData(path);
    if (newIdx >= 0) m_profileCombo->setCurrentIndex(newIdx);
}

void SimulatorPanel::setProfileByName(const QString& name) {
    if (name.isEmpty()) return;
    // The combo stores display text with a leading icon prefix (e.g. "⚙ Renault…"),
    // so try an exact match first, then a suffix match.
    for (int i = 0; i < m_profileCombo->count(); ++i) {
        if (m_profileCombo->itemText(i) == name) {
            m_profileCombo->setCurrentIndex(i);
            return;
        }
    }
    // Fallback: match by the part after the icon prefix + space
    for (int i = 0; i < m_profileCombo->count(); ++i) {
        const QString text = m_profileCombo->itemText(i);
        const int sp = text.indexOf(' ');
        if (sp >= 0 && text.mid(sp + 1) == name) {
            m_profileCombo->setCurrentIndex(i);
            return;
        }
    }
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

// ---------------------------------------------------------------------------
// UdsEcuEditDialog — inline dialog for editing ECU configuration

class UdsEcuEditDialog : public QDialog {
    Q_OBJECT
public:
    explicit UdsEcuEditDialog(QWidget* parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle("Configuration ECU UDS");
        setMinimumWidth(480);

        auto* root = new QVBoxLayout(this);
        auto* tabs = new QTabWidget(this);
        root->addWidget(tabs);

        // --- Tab Général ---
        auto* generalTab = new QWidget;
        auto* form = new QFormLayout(generalTab);
        m_nameEdit    = new QLineEdit(generalTab);
        m_rxIdEdit    = new QLineEdit("7E0", generalTab);
        m_txIdEdit    = new QLineEdit("7E8", generalTab);
        m_funcIdEdit  = new QLineEdit("7DF", generalTab);
        m_keyEdit     = new QLineEdit("C0FFEE", generalTab);
        m_enabledCheck = new QCheckBox("Activé", generalTab);
        m_enabledCheck->setChecked(true);
        form->addRow("Nom :", m_nameEdit);
        form->addRow("RX ID (hex) :", m_rxIdEdit);
        form->addRow("TX ID (hex) :", m_txIdEdit);
        form->addRow("Func ID (hex) :", m_funcIdEdit);
        form->addRow("Seed Key (hex) :", m_keyEdit);
        form->addRow(m_enabledCheck);
        tabs->addTab(generalTab, "Général");

        // --- Tab DIDs ---
        auto* didTab = new QWidget;
        auto* didLayout = new QVBoxLayout(didTab);
        m_didTable = new QTableWidget(0, 3, didTab);
        m_didTable->setHorizontalHeaderLabels({"ID (hex)", "Nom", "Valeur (hex)"});
        m_didTable->horizontalHeader()->setStretchLastSection(true);
        m_didTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        auto* didBtnRow = new QHBoxLayout;
        m_addDidBtn    = new QPushButton("+ Ajouter", didTab);
        m_removeDidBtn = new QPushButton("Supprimer", didTab);
        m_removeDidBtn->setEnabled(false);
        didBtnRow->addWidget(m_addDidBtn);
        didBtnRow->addWidget(m_removeDidBtn);
        didBtnRow->addStretch();
        didLayout->addLayout(didBtnRow);
        didLayout->addWidget(m_didTable, 1);
        tabs->addTab(didTab, "DIDs");

        // --- Tab DTCs ---
        auto* dtcTab = new QWidget;
        auto* dtcLayout = new QVBoxLayout(dtcTab);
        m_dtcTable = new QTableWidget(0, 2, dtcTab);
        m_dtcTable->setHorizontalHeaderLabels({"Code DTC (hex)", "Status (hex)"});
        m_dtcTable->horizontalHeader()->setStretchLastSection(true);
        m_dtcTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        auto* dtcBtnRow = new QHBoxLayout;
        m_addDtcBtn    = new QPushButton("+ Ajouter", dtcTab);
        m_removeDtcBtn = new QPushButton("Supprimer", dtcTab);
        m_removeDtcBtn->setEnabled(false);
        dtcBtnRow->addWidget(m_addDtcBtn);
        dtcBtnRow->addWidget(m_removeDtcBtn);
        dtcBtnRow->addStretch();
        dtcLayout->addLayout(dtcBtnRow);
        dtcLayout->addWidget(m_dtcTable, 1);
        tabs->addTab(dtcTab, "DTCs");

        auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        root->addWidget(btns);
        connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

        // Connect buttons
        connect(m_addDidBtn, &QPushButton::clicked, this, [this]() {
            int row = m_didTable->rowCount();
            m_didTable->insertRow(row);
            m_didTable->setItem(row, 0, new QTableWidgetItem("F190"));
            m_didTable->setItem(row, 1, new QTableWidgetItem("DID"));
            m_didTable->setItem(row, 2, new QTableWidgetItem("00"));
        });
        connect(m_removeDidBtn, &QPushButton::clicked, this, [this]() {
            const auto sel = m_didTable->selectedItems();
            if (!sel.isEmpty())
                m_didTable->removeRow(m_didTable->row(sel.first()));
        });
        connect(m_didTable, &QTableWidget::itemSelectionChanged, this, [this]() {
            m_removeDidBtn->setEnabled(!m_didTable->selectedItems().isEmpty());
        });

        connect(m_addDtcBtn, &QPushButton::clicked, this, [this]() {
            int row = m_dtcTable->rowCount();
            m_dtcTable->insertRow(row);
            m_dtcTable->setItem(row, 0, new QTableWidgetItem("012345"));
            m_dtcTable->setItem(row, 1, new QTableWidgetItem("09"));
        });
        connect(m_removeDtcBtn, &QPushButton::clicked, this, [this]() {
            const auto sel = m_dtcTable->selectedItems();
            if (!sel.isEmpty())
                m_dtcTable->removeRow(m_dtcTable->row(sel.first()));
        });
        connect(m_dtcTable, &QTableWidget::itemSelectionChanged, this, [this]() {
            m_removeDtcBtn->setEnabled(!m_dtcTable->selectedItems().isEmpty());
        });

        // Pre-fill defaults
        setConfig(UdsEcuSim::Config{});
    }

    void setConfig(const UdsEcuSim::Config& cfg)
    {
        m_nameEdit->setText(cfg.name);
        m_rxIdEdit->setText(QString::number(cfg.rxId, 16).toUpper());
        m_txIdEdit->setText(QString::number(cfg.txId, 16).toUpper());
        m_funcIdEdit->setText(QString::number(cfg.funcId, 16).toUpper());
        m_keyEdit->setText(QString::number(cfg.seedKey, 16).toUpper());
        m_enabledCheck->setChecked(cfg.enabled);

        m_didTable->setRowCount(0);
        for (const auto& did : cfg.dids) {
            int row = m_didTable->rowCount();
            m_didTable->insertRow(row);
            m_didTable->setItem(row, 0, new QTableWidgetItem(QString::number(did.id, 16).toUpper()));
            m_didTable->setItem(row, 1, new QTableWidgetItem(did.name));
            m_didTable->setItem(row, 2, new QTableWidgetItem(QString::fromLatin1(did.value.toHex()).toUpper()));
        }
        if (cfg.dids.isEmpty()) {
            // Pre-fill common DIDs
            struct { uint16_t id; const char* name; const char* hex; } defaults[] = {
                {0xF190, "VIN",                       "574442303132333435363738394142434400"},
                {0xF18C, "ECU Serial Number",         "01234567"},
                {0xF1A0, "ECU Part Number",            "AABBCCDD"},
                {0xF197, "System Supplier ECU HW Num","11223344"},
                {0xF101, "Active Diag Session",       "01"},
            };
            for (const auto& d : defaults) {
                int row = m_didTable->rowCount();
                m_didTable->insertRow(row);
                m_didTable->setItem(row, 0, new QTableWidgetItem(QString::number(d.id, 16).toUpper()));
                m_didTable->setItem(row, 1, new QTableWidgetItem(d.name));
                m_didTable->setItem(row, 2, new QTableWidgetItem(d.hex));
            }
        }

        m_dtcTable->setRowCount(0);
        for (const auto& dtc : cfg.dtcs) {
            int row = m_dtcTable->rowCount();
            m_dtcTable->insertRow(row);
            m_dtcTable->setItem(row, 0, new QTableWidgetItem(QString::number(dtc.code, 16).toUpper()));
            m_dtcTable->setItem(row, 1, new QTableWidgetItem(QString::number(dtc.status, 16).toUpper()));
        }
        if (cfg.dtcs.isEmpty()) {
            int row = m_dtcTable->rowCount();
            m_dtcTable->insertRow(row);
            m_dtcTable->setItem(row, 0, new QTableWidgetItem("012345"));
            m_dtcTable->setItem(row, 1, new QTableWidgetItem("09"));
        }
    }

    UdsEcuSim::Config config() const
    {
        UdsEcuSim::Config cfg;
        cfg.name    = m_nameEdit->text().trimmed();
        if (cfg.name.isEmpty()) cfg.name = "ECU";
        cfg.rxId    = m_rxIdEdit->text().toUInt(nullptr, 16);
        cfg.txId    = m_txIdEdit->text().toUInt(nullptr, 16);
        cfg.funcId  = m_funcIdEdit->text().toUInt(nullptr, 16);
        cfg.seedKey = m_keyEdit->text().toUInt(nullptr, 16);
        cfg.enabled = m_enabledCheck->isChecked();

        for (int row = 0; row < m_didTable->rowCount(); ++row) {
            UdsEcuSim::Did did;
            auto* idItem  = m_didTable->item(row, 0);
            auto* nmItem  = m_didTable->item(row, 1);
            auto* valItem = m_didTable->item(row, 2);
            if (!idItem) continue;
            did.id    = idItem->text().toUShort(nullptr, 16);
            did.name  = nmItem  ? nmItem->text()  : QString();
            did.value = valItem ? QByteArray::fromHex(valItem->text().toLatin1()) : QByteArray();
            cfg.dids.append(did);
        }

        for (int row = 0; row < m_dtcTable->rowCount(); ++row) {
            UdsEcuSim::Dtc dtc;
            auto* codeItem   = m_dtcTable->item(row, 0);
            auto* statusItem = m_dtcTable->item(row, 1);
            if (!codeItem) continue;
            dtc.code   = codeItem->text().toUInt(nullptr, 16);
            dtc.status = statusItem ? (uint8_t)statusItem->text().toUInt(nullptr, 16) : 0x09;
            cfg.dtcs.append(dtc);
        }
        return cfg;
    }

private:
    QLineEdit*    m_nameEdit{nullptr};
    QLineEdit*    m_rxIdEdit{nullptr};
    QLineEdit*    m_txIdEdit{nullptr};
    QLineEdit*    m_funcIdEdit{nullptr};
    QLineEdit*    m_keyEdit{nullptr};
    QCheckBox*    m_enabledCheck{nullptr};
    QTableWidget* m_didTable{nullptr};
    QTableWidget* m_dtcTable{nullptr};
    QPushButton*  m_addDidBtn{nullptr};
    QPushButton*  m_removeDidBtn{nullptr};
    QPushButton*  m_addDtcBtn{nullptr};
    QPushButton*  m_removeDtcBtn{nullptr};
};

// ---------------------------------------------------------------------------
// SimulatorPanel ECU tab implementation

QWidget* SimulatorPanel::setupEcuTab()
{
    auto* tab = new QWidget;
    auto* vl  = new QVBoxLayout(tab);
    vl->setContentsMargins(4, 4, 4, 4);
    vl->setSpacing(4);

    // Button bar
    auto* btnRow  = new QHBoxLayout;
    m_addEcuBtn    = new QPushButton("+ Ajouter ECU", tab);
    m_editEcuBtn   = new QPushButton("Éditer", tab);
    m_editEcuBtn->setEnabled(false);
    m_removeEcuBtn = new QPushButton("Supprimer", tab);
    m_removeEcuBtn->setEnabled(false);
    btnRow->addWidget(m_addEcuBtn);
    btnRow->addWidget(m_editEcuBtn);
    btnRow->addWidget(m_removeEcuBtn);
    btnRow->addStretch();
    vl->addLayout(btnRow);

    // ECU list
    m_ecuList = new QListWidget(tab);
    m_ecuList->setFont(QFont("Monospace", 9));
    vl->addWidget(m_ecuList, 1);

    // Info label
    auto* info = new QLabel(
        "Double-clic pour éditer. Réponses UDS renvoyées via frameGenerated.", tab);
    info->setStyleSheet("color:#7c8fa6; font-size:10px; padding:4px;");
    vl->addWidget(info);

    // Connections
    connect(m_addEcuBtn,    &QPushButton::clicked, this, &SimulatorPanel::onAddEcu);
    connect(m_editEcuBtn,   &QPushButton::clicked, this, &SimulatorPanel::onEditEcu);
    connect(m_removeEcuBtn, &QPushButton::clicked, this, &SimulatorPanel::onRemoveEcu);
    connect(m_ecuList, &QListWidget::itemDoubleClicked, this, &SimulatorPanel::onEditEcu);
    connect(m_ecuList, &QListWidget::currentRowChanged, this, [this](int row) {
        bool valid = row >= 0;
        m_editEcuBtn->setEnabled(valid);
        m_removeEcuBtn->setEnabled(valid);
    });

    return tab;
}

void SimulatorPanel::refreshEcuList()
{
    m_ecuList->clear();
    for (const auto* ecu : m_ecuSims) {
        const auto& cfg = ecu->config();
        QString bullet = cfg.enabled ? "● " : "○ ";
        QString text = QString("%1%2   0x%3 → 0x%4")
            .arg(bullet)
            .arg(cfg.name)
            .arg(cfg.rxId, 3, 16, QChar('0'))
            .arg(cfg.txId, 3, 16, QChar('0'));
        auto* item = new QListWidgetItem(text);
        item->setForeground(cfg.enabled ? QColor("#22c55e") : QColor("#7c8fa6"));
        m_ecuList->addItem(item);
    }
}

void SimulatorPanel::onAddEcu()
{
    UdsEcuEditDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;
    auto* ecu = new UdsEcuSim(dlg.config(), this);
    connect(ecu, &UdsEcuSim::frameToSend, this, &SimulatorPanel::frameGenerated);
    m_ecuSims.append(ecu);
    refreshEcuList();
}

void SimulatorPanel::onEditEcu()
{
    int row = m_ecuList->currentRow();
    if (row < 0 || row >= m_ecuSims.size()) return;
    UdsEcuEditDialog dlg(this);
    dlg.setConfig(m_ecuSims[row]->config());
    if (dlg.exec() != QDialog::Accepted) return;
    m_ecuSims[row]->setConfig(dlg.config());
    refreshEcuList();
}

void SimulatorPanel::onRemoveEcu()
{
    int row = m_ecuList->currentRow();
    if (row < 0 || row >= m_ecuSims.size()) return;
    auto* ecu = m_ecuSims.takeAt(row);
    ecu->deleteLater();
    refreshEcuList();
}

void SimulatorPanel::onFrameReceived(const socketspy::core::CanFrame& frame)
{
    for (auto* ecu : m_ecuSims)
        ecu->onFrameReceived(frame);
}

} // namespace socketspy::gui

#include "simulator_panel.moc"
