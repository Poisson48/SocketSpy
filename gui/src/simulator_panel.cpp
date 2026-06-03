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

} // namespace socketspy::gui
