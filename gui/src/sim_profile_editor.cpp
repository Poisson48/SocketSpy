#include "sim_profile_editor.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QFrame>
#include <QHeaderView>
#include <QScrollArea>
#include <QDialogButtonBox>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <nlohmann/json.hpp>

// Helper: WaveformType <-> int index
static int waveformToIndex(socketspy::gui::WaveformType w) {
    switch (w) {
    case socketspy::gui::WaveformType::Sine:   return 1;
    case socketspy::gui::WaveformType::Ramp:   return 2;
    case socketspy::gui::WaveformType::Square: return 3;
    case socketspy::gui::WaveformType::Random: return 4;
    default: return 0;
    }
}
static socketspy::gui::WaveformType indexToWaveform(int idx) {
    switch (idx) {
    case 1: return socketspy::gui::WaveformType::Sine;
    case 2: return socketspy::gui::WaveformType::Ramp;
    case 3: return socketspy::gui::WaveformType::Square;
    case 4: return socketspy::gui::WaveformType::Random;
    default: return socketspy::gui::WaveformType::None;
    }
}

using json = nlohmann::json;

namespace socketspy::gui {

static QString msgLabel(uint32_t id, int period) {
    return "0x" + QString::number(id, 16).toUpper().rightJustified(3, QChar('0'))
         + QString(" @ %1ms").arg(period);
}

SimProfileEditor::SimProfileEditor(QWidget* parent) : QDialog(parent) {
    setupUi();
}

void SimProfileEditor::setupUi() {
    setWindowTitle("Éditeur de profil simulateur");
    resize(640, 620);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    // Profile name
    auto* nameRow = new QHBoxLayout;
    nameRow->addWidget(new QLabel("Nom du profil :", this));
    m_profileName = new QLineEdit("Mon Profil", this);
    nameRow->addWidget(m_profileName, 1);
    root->addLayout(nameRow);

    // Action buttons
    auto* btnRow = new QHBoxLayout;
    m_addNodeBtn = new QPushButton("+ Nœud", this);
    m_addMsgBtn  = new QPushButton("+ Message", this);
    m_addSigBtn  = new QPushButton("+ Signal", this);
    m_removeBtn  = new QPushButton("Supprimer", this);
    m_addMsgBtn->setEnabled(false);
    m_addSigBtn->setEnabled(false);
    m_removeBtn->setEnabled(false);
    btnRow->addWidget(m_addNodeBtn);
    btnRow->addWidget(m_addMsgBtn);
    btnRow->addWidget(m_addSigBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_removeBtn);
    root->addLayout(btnRow);

    // Tree
    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(1);
    m_tree->setHeaderHidden(true);
    root->addWidget(m_tree, 1);

    // Separator
    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    root->addWidget(sep);

    // Stacked form
    m_stack = new QStackedWidget(this);

    // Page 0: placeholder
    auto* emptyPage = new QLabel("Sélectionnez un nœud, un message ou un signal pour l'éditer.", this);
    emptyPage->setAlignment(Qt::AlignCenter);
    // Use theme muted color (#7c8fa6) instead of hardcoded #888
    emptyPage->setStyleSheet("color: #7c8fa6;");
    m_stack->addWidget(emptyPage);

    // Page 1: Node
    auto* nodePage = new QWidget(this);
    auto* nodeForm = new QFormLayout(nodePage);
    m_nodeNameEdit = new QLineEdit(nodePage);
    nodeForm->addRow("Nom :", m_nodeNameEdit);
    m_stack->addWidget(nodePage);

    // Page 2: Message
    auto* msgPage = new QWidget(this);
    auto* msgForm = new QFormLayout(msgPage);
    m_msgIdEdit = new QLineEdit(msgPage);
    m_msgIdEdit->setPlaceholderText("e.g. 0x100");
    m_msgPeriodSpin = new QSpinBox(msgPage);
    m_msgPeriodSpin->setRange(1, 10000);
    m_msgPeriodSpin->setSuffix(" ms");
    m_msgPeriodSpin->setValue(100);
    m_msgDlcSpin = new QSpinBox(msgPage);
    m_msgDlcSpin->setRange(1, 8);
    m_msgDlcSpin->setValue(8);
    msgForm->addRow("ID (hex) :",  m_msgIdEdit);
    msgForm->addRow("Période :",   m_msgPeriodSpin);
    msgForm->addRow("DLC :",       m_msgDlcSpin);
    m_stack->addWidget(msgPage);

    // Page 3: Signal (wrapped in a QScrollArea – sigPage has no parent, scroll area owns it)
    auto* sigPage = new QWidget();
    auto* sigForm = new QFormLayout(sigPage);
    m_sigNameEdit     = new QLineEdit(sigPage);
    m_sigStartBitSpin = new QSpinBox(sigPage);
    m_sigStartBitSpin->setRange(0, 63);
    m_sigLengthSpin   = new QSpinBox(sigPage);
    m_sigLengthSpin->setRange(1, 64);
    m_sigLengthSpin->setValue(8);
    m_sigFactorSpin = new QDoubleSpinBox(sigPage);
    m_sigFactorSpin->setRange(-1e9, 1e9);
    m_sigFactorSpin->setDecimals(6);
    m_sigFactorSpin->setValue(1.0);
    m_sigOffsetSpin = new QDoubleSpinBox(sigPage);
    m_sigOffsetSpin->setRange(-1e9, 1e9);
    m_sigOffsetSpin->setDecimals(6);
    m_sigMinSpin = new QDoubleSpinBox(sigPage);
    m_sigMinSpin->setRange(-1e9, 1e9);
    m_sigMinSpin->setDecimals(4);
    m_sigMaxSpin = new QDoubleSpinBox(sigPage);
    m_sigMaxSpin->setRange(-1e9, 1e9);
    m_sigMaxSpin->setDecimals(4);
    m_sigMaxSpin->setValue(255.0);
    m_sigDefaultSpin = new QDoubleSpinBox(sigPage);
    m_sigDefaultSpin->setRange(-1e9, 1e9);
    m_sigDefaultSpin->setDecimals(4);
    sigForm->addRow("Nom :",           m_sigNameEdit);
    sigForm->addRow("Bit de départ :", m_sigStartBitSpin);
    sigForm->addRow("Longueur (bits):", m_sigLengthSpin);
    sigForm->addRow("Facteur :",      m_sigFactorSpin);
    sigForm->addRow("Offset :",       m_sigOffsetSpin);
    sigForm->addRow("Min :",          m_sigMinSpin);
    sigForm->addRow("Max :",          m_sigMaxSpin);
    sigForm->addRow("Défaut :",       m_sigDefaultSpin);

    // Waveform section
    auto* waveBox = new QGroupBox("Forme d'onde (génération automatique)", sigPage);
    auto* waveLayout = new QFormLayout(waveBox);
    waveLayout->setContentsMargins(4, 8, 4, 4);
    waveLayout->setSpacing(4);

    auto* waveHelp = new QLabel(
        "Génère automatiquement un scénario cyclique. "
        "Remplace les points manuels ci-dessous si actif.", waveBox);
    waveHelp->setWordWrap(true);
    waveHelp->setStyleSheet("color: #7c8fa6; font-size: 10px;");
    waveLayout->addRow(waveHelp);

    m_waveformCombo = new QComboBox(waveBox);
    m_waveformCombo->addItem("Aucune (statique / manuel)", 0);
    m_waveformCombo->addItem("Sinusoïde",                  1);
    m_waveformCombo->addItem("Rampe (dent de scie)",       2);
    m_waveformCombo->addItem("Carré",                      3);
    m_waveformCombo->addItem("Aléatoire",                  4);
    waveLayout->addRow("Type :", m_waveformCombo);

    m_numPointsSpin = new QSpinBox(waveBox);
    m_numPointsSpin->setRange(2, 10000);
    m_numPointsSpin->setValue(100);
    m_numPointsSpin->setToolTip("Nombre de points par cycle de la forme d'onde");
    waveLayout->addRow("Points/cycle :", m_numPointsSpin);

    m_stepMsSpin = new QSpinBox(waveBox);
    m_stepMsSpin->setRange(1, 60000);
    m_stepMsSpin->setValue(50);
    m_stepMsSpin->setSuffix(" ms");
    m_stepMsSpin->setToolTip("Intervalle entre deux pas consécutifs (fréquence de mise à jour)");
    waveLayout->addRow("Pas (update) :", m_stepMsSpin);

    sigForm->addRow(waveBox);

    // Scenario section
    auto* scenBox = new QGroupBox("Scenario (animation automatique)", sigPage);
    auto* scenLayout = new QVBoxLayout(scenBox);
    scenLayout->setContentsMargins(4, 4, 4, 4);
    scenLayout->setSpacing(4);

    auto* scenHelp = new QLabel("Points interpolés linéairement en boucle. Laissez vide pour valeur manuelle.", scenBox);
    scenHelp->setWordWrap(true);
    scenHelp->setStyleSheet("color: gray; font-size: 10px;");
    scenLayout->addWidget(scenHelp);

    m_scenarioTable = new QTableWidget(0, 2, scenBox);
    m_scenarioTable->setHorizontalHeaderLabels({"Temps (ms)", "Valeur"});
    m_scenarioTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_scenarioTable->setMinimumHeight(100);
    m_scenarioTable->setMaximumHeight(160);
    scenLayout->addWidget(m_scenarioTable);

    auto* scenBtnRow = new QHBoxLayout;
    m_scenarioAddBtn    = new QPushButton("+ Point", scenBox);
    m_scenarioRemoveBtn = new QPushButton("Supprimer", scenBox);
    scenBtnRow->addWidget(m_scenarioAddBtn);
    scenBtnRow->addWidget(m_scenarioRemoveBtn);
    scenBtnRow->addStretch();
    scenLayout->addLayout(scenBtnRow);

    sigForm->addRow(scenBox);

    // Wrap in a scroll area so the form is usable even at small dialog sizes
    auto* sigScroll = new QScrollArea(this);
    sigScroll->setWidgetResizable(true);
    sigScroll->setWidget(sigPage);
    sigScroll->setFrameShape(QFrame::NoFrame);
    m_stack->addWidget(sigScroll);

    root->addWidget(m_stack);

    auto* dlgBtns = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    root->addWidget(dlgBtns);

    // ── connections ──────────────────────────────────────────────────────────
    connect(m_addNodeBtn, &QPushButton::clicked, this, &SimProfileEditor::onAddNode);
    connect(m_addMsgBtn,  &QPushButton::clicked, this, &SimProfileEditor::onAddMessage);
    connect(m_addSigBtn,  &QPushButton::clicked, this, &SimProfileEditor::onAddSignal);
    connect(m_removeBtn,  &QPushButton::clicked, this, &SimProfileEditor::onRemove);
    connect(m_tree, &QTreeWidget::currentItemChanged,
            this, [this](QTreeWidgetItem*, QTreeWidgetItem*) { onSelectionChanged(); });
    connect(dlgBtns, &QDialogButtonBox::accepted, this, &SimProfileEditor::onAccept);
    connect(dlgBtns, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Live sync: form widgets → tree item data
    connect(m_nodeNameEdit, &QLineEdit::textChanged, this, [this]() {
        if (m_blockSync || !m_current || m_current->type() != Node) return;
        m_current->setText(0, m_nodeNameEdit->text());
    });

    auto syncMsg = [this]() {
        if (m_blockSync || !m_current || m_current->type() != Message) return;
        bool ok;
        uint32_t id = m_msgIdEdit->text().trimmed().toUInt(&ok, 0);
        if (!ok) id = 0;
        int period = m_msgPeriodSpin->value();
        m_current->setData(0, IdRole,     id);
        m_current->setData(0, PeriodRole, period);
        m_current->setData(0, DlcRole,    m_msgDlcSpin->value());
        m_current->setText(0, msgLabel(id, period));
    };
    connect(m_msgIdEdit,    &QLineEdit::textChanged,
            this, syncMsg);
    connect(m_msgPeriodSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, syncMsg);
    connect(m_msgDlcSpin,    QOverload<int>::of(&QSpinBox::valueChanged), this, syncMsg);

    auto syncSig = [this]() {
        if (m_blockSync || !m_current || m_current->type() != Signal) return;
        m_current->setText(0, m_sigNameEdit->text());
        m_current->setData(0, StartBitRole, m_sigStartBitSpin->value());
        m_current->setData(0, LengthRole,   m_sigLengthSpin->value());
        m_current->setData(0, FactorRole,   m_sigFactorSpin->value());
        m_current->setData(0, OffsetRole,   m_sigOffsetSpin->value());
        m_current->setData(0, MinRole,      m_sigMinSpin->value());
        m_current->setData(0, MaxRole,      m_sigMaxSpin->value());
        m_current->setData(0, DefaultRole,  m_sigDefaultSpin->value());
        m_current->setData(0, WaveformRole,  m_waveformCombo->currentIndex());
        m_current->setData(0, NumPointsRole, m_numPointsSpin->value());
        m_current->setData(0, StepMsRole,    m_stepMsSpin->value());
    };
    connect(m_sigNameEdit, &QLineEdit::textChanged, this, syncSig);
    connect(m_sigStartBitSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, syncSig);
    connect(m_sigLengthSpin,   QOverload<int>::of(&QSpinBox::valueChanged), this, syncSig);
    connect(m_sigFactorSpin,   QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, syncSig);
    connect(m_sigOffsetSpin,   QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, syncSig);
    connect(m_sigMinSpin,      QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, syncSig);
    connect(m_sigMaxSpin,      QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, syncSig);
    connect(m_sigDefaultSpin,  QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, syncSig);
    connect(m_waveformCombo,  QOverload<int>::of(&QComboBox::currentIndexChanged), this, syncSig);
    connect(m_numPointsSpin,  QOverload<int>::of(&QSpinBox::valueChanged), this, syncSig);
    connect(m_stepMsSpin,     QOverload<int>::of(&QSpinBox::valueChanged), this, syncSig);

    connect(m_scenarioAddBtn,    &QPushButton::clicked, this, &SimProfileEditor::onScenarioAdd);
    connect(m_scenarioRemoveBtn, &QPushButton::clicked, this, &SimProfileEditor::onScenarioRemove);
    connect(m_scenarioTable, &QTableWidget::cellChanged,
            this, &SimProfileEditor::onScenarioCellChanged);
}

// ── tree builders ─────────────────────────────────────────────────────────────

QTreeWidgetItem* SimProfileEditor::addNodeItem(const QString& name) {
    auto* item = new QTreeWidgetItem(m_tree, Node);
    item->setText(0, name);
    item->setExpanded(true);
    return item;
}

QTreeWidgetItem* SimProfileEditor::addMessageItem(QTreeWidgetItem* parent, uint32_t id, int period, uint8_t dlc) {
    auto* item = new QTreeWidgetItem(parent, Message);
    item->setText(0, msgLabel(id, period));
    item->setData(0, IdRole,     id);
    item->setData(0, PeriodRole, period);
    item->setData(0, DlcRole,    dlc);
    item->setExpanded(true);
    return item;
}

QTreeWidgetItem* SimProfileEditor::addSignalItem(QTreeWidgetItem* parent, const SimSignal& s) {
    auto* item = new QTreeWidgetItem(parent, Signal);
    item->setText(0, s.name);
    item->setData(0, StartBitRole, s.start_bit);
    item->setData(0, LengthRole,   s.length);
    item->setData(0, FactorRole,   s.factor);
    item->setData(0, OffsetRole,   s.offset);
    item->setData(0, MinRole,      s.min);
    item->setData(0, MaxRole,      s.max);
    item->setData(0, DefaultRole,  s.current_value);
    item->setData(0, WaveformRole,   waveformToIndex(s.waveform));
    item->setData(0, NumPointsRole,  s.num_points);
    item->setData(0, StepMsRole,     s.step_ms);

    QVariantList pts;
    for (const auto& p : s.scenario) {
        QVariantMap m;
        m["t_ms"]  = (qlonglong)p.t_ms;
        m["value"] = p.value;
        pts.append(m);
    }
    item->setData(0, ScenarioRole, pts);
    return item;
}

void SimProfileEditor::loadScenarioTable(QTreeWidgetItem* item) {
    m_scenarioBlockSync = true;
    m_scenarioTable->setRowCount(0);
    QVariantList pts = item->data(0, ScenarioRole).toList();
    for (const auto& pt : pts) {
        QVariantMap m = pt.toMap();
        int row = m_scenarioTable->rowCount();
        m_scenarioTable->insertRow(row);
        m_scenarioTable->setItem(row, 0, new QTableWidgetItem(QString::number(m["t_ms"].toLongLong())));
        m_scenarioTable->setItem(row, 1, new QTableWidgetItem(QString::number(m["value"].toDouble())));
    }
    m_scenarioBlockSync = false;
}

void SimProfileEditor::saveScenarioTable() {
    if (!m_current || m_current->type() != Signal) return;
    QVariantList pts;
    for (int r = 0; r < m_scenarioTable->rowCount(); ++r) {
        auto* tItem = m_scenarioTable->item(r, 0);
        auto* vItem = m_scenarioTable->item(r, 1);
        if (!tItem || !vItem) continue;
        bool ok1, ok2;
        qlonglong t = tItem->text().toLongLong(&ok1);
        double    v = vItem->text().toDouble(&ok2);
        if (!ok1 || !ok2) continue;
        QVariantMap m;
        m["t_ms"]  = t;
        m["value"] = v;
        pts.append(m);
    }
    m_current->setData(0, ScenarioRole, pts);
}

void SimProfileEditor::onScenarioAdd() {
    if (!m_current || m_current->type() != Signal) return;
    int row = m_scenarioTable->rowCount();
    m_scenarioTable->insertRow(row);
    qlonglong t_ms = (row > 0 && m_scenarioTable->item(row - 1, 0))
        ? m_scenarioTable->item(row - 1, 0)->text().toLongLong() + 1000
        : 0LL;
    m_scenarioTable->setItem(row, 0, new QTableWidgetItem(QString::number(t_ms)));
    m_scenarioTable->setItem(row, 1, new QTableWidgetItem("0"));
    saveScenarioTable();
}

void SimProfileEditor::onScenarioRemove() {
    if (!m_current || m_current->type() != Signal) return;
    int row = m_scenarioTable->currentRow();
    if (row < 0) return;
    m_scenarioTable->removeRow(row);
    saveScenarioTable();
}

void SimProfileEditor::onScenarioCellChanged(int /*row*/, int /*col*/) {
    if (m_scenarioBlockSync) return;
    saveScenarioTable();
}

// ── slots ─────────────────────────────────────────────────────────────────────

void SimProfileEditor::onAddNode() {
    auto* item = addNodeItem("NouveauNœud");
    m_tree->setCurrentItem(item);
}

void SimProfileEditor::onAddMessage() {
    QTreeWidgetItem* sel = m_tree->currentItem();
    if (!sel) return;
    QTreeWidgetItem* nodeItem = nullptr;
    if      (sel->type() == Node)    nodeItem = sel;
    else if (sel->type() == Message) nodeItem = sel->parent();
    else if (sel->type() == Signal && sel->parent())
                                     nodeItem = sel->parent()->parent();
    if (!nodeItem) return;
    auto* item = addMessageItem(nodeItem, 0x100, 100, 8);
    m_tree->setCurrentItem(item);
}

void SimProfileEditor::onAddSignal() {
    QTreeWidgetItem* sel = m_tree->currentItem();
    if (!sel) return;
    QTreeWidgetItem* msgItem = nullptr;
    if      (sel->type() == Message) msgItem = sel;
    else if (sel->type() == Signal)  msgItem = sel->parent();
    if (!msgItem) return;
    SimSignal s;
    s.name = "Signal"; s.start_bit = 0; s.length = 8;
    s.factor = 1.0; s.offset = 0.0; s.min = 0.0; s.max = 255.0; s.current_value = 0.0;
    s.scenario = {};
    auto* item = addSignalItem(msgItem, s);
    m_tree->setCurrentItem(item);
}

void SimProfileEditor::onRemove() {
    QTreeWidgetItem* sel = m_tree->currentItem();
    if (!sel) return;
    m_current = nullptr;
    delete sel;
    m_stack->setCurrentIndex(0);
    m_removeBtn->setEnabled(false);
    m_addMsgBtn->setEnabled(false);
    m_addSigBtn->setEnabled(false);
}

void SimProfileEditor::onSelectionChanged() {
    m_current = m_tree->currentItem();
    if (!m_current) {
        m_stack->setCurrentIndex(0);
        m_addMsgBtn->setEnabled(false);
        m_addSigBtn->setEnabled(false);
        m_removeBtn->setEnabled(false);
        return;
    }

    m_removeBtn->setEnabled(true);
    int t = m_current->type();
    m_addMsgBtn->setEnabled(t == Node || t == Message);
    m_addSigBtn->setEnabled(t == Message || t == Signal);

    m_blockSync = true;
    if (t == Node) {
        m_nodeNameEdit->setText(m_current->text(0));
        m_stack->setCurrentIndex(1);
    } else if (t == Message) {
        m_msgIdEdit->setText(QString("0x%1")
            .arg(m_current->data(0, IdRole).toUInt(), 0, 16).toUpper());
        m_msgPeriodSpin->setValue(m_current->data(0, PeriodRole).toInt());
        m_msgDlcSpin->setValue(m_current->data(0, DlcRole).toInt());
        m_stack->setCurrentIndex(2);
    } else if (t == Signal) {
        m_sigNameEdit->setText(m_current->text(0));
        m_sigStartBitSpin->setValue(m_current->data(0, StartBitRole).toInt());
        m_sigLengthSpin->setValue(m_current->data(0, LengthRole).toInt());
        m_sigFactorSpin->setValue(m_current->data(0, FactorRole).toDouble());
        m_sigOffsetSpin->setValue(m_current->data(0, OffsetRole).toDouble());
        m_sigMinSpin->setValue(m_current->data(0, MinRole).toDouble());
        m_sigMaxSpin->setValue(m_current->data(0, MaxRole).toDouble());
        m_sigDefaultSpin->setValue(m_current->data(0, DefaultRole).toDouble());
        // Waveform fields (default to 0/100/50 if not set)
        int waveIdx = m_current->data(0, WaveformRole).isValid()
                      ? m_current->data(0, WaveformRole).toInt() : 0;
        int numPts  = m_current->data(0, NumPointsRole).isValid()
                      ? m_current->data(0, NumPointsRole).toInt() : 100;
        int stepMs  = m_current->data(0, StepMsRole).isValid()
                      ? m_current->data(0, StepMsRole).toInt() : 50;
        m_waveformCombo->setCurrentIndex(waveIdx);
        m_numPointsSpin->setValue(numPts);
        m_stepMsSpin->setValue(stepMs);
        loadScenarioTable(m_current);
        m_stack->setCurrentIndex(3);
    }
    m_blockSync = false;
}

// ── build / save ──────────────────────────────────────────────────────────────

SimProfile SimProfileEditor::buildProfile() const {
    SimProfile p;
    p.name = m_profileName->text().trimmed();
    if (p.name.isEmpty()) p.name = "Custom";

    for (int ni = 0; ni < m_tree->topLevelItemCount(); ++ni) {
        QTreeWidgetItem* ni_item = m_tree->topLevelItem(ni);
        SimNode node;
        node.name = ni_item->text(0);
        for (int mi = 0; mi < ni_item->childCount(); ++mi) {
            QTreeWidgetItem* mi_item = ni_item->child(mi);
            SimMessage msg;
            msg.id        = mi_item->data(0, IdRole).toUInt();
            msg.period_ms = mi_item->data(0, PeriodRole).toInt();
            msg.dlc       = static_cast<uint8_t>(mi_item->data(0, DlcRole).toInt());
            for (int si = 0; si < mi_item->childCount(); ++si) {
                QTreeWidgetItem* si_item = mi_item->child(si);
                SimSignal sig;
                sig.name          = si_item->text(0);
                sig.start_bit     = si_item->data(0, StartBitRole).toInt();
                sig.length        = si_item->data(0, LengthRole).toInt();
                sig.factor        = si_item->data(0, FactorRole).toDouble();
                sig.offset        = si_item->data(0, OffsetRole).toDouble();
                sig.min           = si_item->data(0, MinRole).toDouble();
                sig.max           = si_item->data(0, MaxRole).toDouble();
                sig.current_value = si_item->data(0, DefaultRole).toDouble();
                sig.waveform   = indexToWaveform(
                    si_item->data(0, WaveformRole).isValid()
                    ? si_item->data(0, WaveformRole).toInt() : 0);
                sig.num_points = si_item->data(0, NumPointsRole).isValid()
                    ? si_item->data(0, NumPointsRole).toInt() : 100;
                sig.step_ms    = si_item->data(0, StepMsRole).isValid()
                    ? si_item->data(0, StepMsRole).toInt() : 50;
                for (const auto& pt : si_item->data(0, ScenarioRole).toList()) {
                    QVariantMap m = pt.toMap();
                    SimScenarioPoint p;
                    p.t_ms  = m["t_ms"].toLongLong();
                    p.value = m["value"].toDouble();
                    sig.scenario.push_back(p);
                }
                msg.sigs.push_back(sig);
            }
            node.messages.push_back(msg);
        }
        p.nodes.push_back(node);
    }
    return p;
}

bool SimProfileEditor::saveToFile(const SimProfile& p) {
    json root;
    root["name"]        = p.name.toStdString();
    root["description"] = "Custom profile";
    json nodes_arr = json::array();
    for (const auto& node : p.nodes) {
        json nj;
        nj["name"] = node.name.toStdString();
        json msgs_arr = json::array();
        for (const auto& msg : node.messages) {
            json mj;
            mj["id"]        = QString("0x%1").arg(msg.id, 0, 16).toUpper().toStdString();
            mj["period_ms"] = msg.period_ms;
            mj["dlc"]       = msg.dlc;
            json sigs_arr = json::array();
            for (const auto& sig : msg.sigs) {
                json sj;
                sj["name"]       = sig.name.toStdString();
                sj["start_bit"]  = sig.start_bit;
                sj["length"]     = sig.length;
                sj["factor"]     = sig.factor;
                sj["offset"]     = sig.offset;
                sj["min"]        = sig.min;
                sj["max"]        = sig.max;
                sj["default"]    = sig.current_value;
                sj["waveform"]   = waveformToIndex(sig.waveform);
                sj["num_points"] = sig.num_points;
                sj["step_ms"]    = sig.step_ms;
                if (!sig.scenario.empty()) {
                    json sa = json::array();
                    for (const auto& p : sig.scenario)
                        sa.push_back({{"t_ms", p.t_ms}, {"value", p.value}});
                    sj["scenario"] = sa;
                }
                sigs_arr.push_back(sj);
            }
            mj["signals"] = sigs_arr;
            msgs_arr.push_back(mj);
        }
        nj["messages"] = msgs_arr;
        nodes_arr.push_back(nj);
    }
    root["nodes"] = nodes_arr;

    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/profiles";
    QDir().mkpath(dir);

    QString filename = p.name;
    for (auto& c : filename)
        if (!c.isLetterOrNumber() && c != '_' && c != '-') c = '_';
    m_savedPath = dir + "/" + filename + ".json";

    QFile f(m_savedPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, "Error", "Cannot write: " + m_savedPath);
        m_savedPath.clear();
        return false;
    }
    std::string s = root.dump(2);
    f.write(s.c_str(), static_cast<qint64>(s.size()));
    return true;
}

void SimProfileEditor::onAccept() {
    SimProfile p = buildProfile();
    if (p.name.isEmpty()) {
        QMessageBox::warning(this, "Profil incomplet", "Le profil doit avoir un nom.");
        return;
    }
    if (p.nodes.empty()) {
        QMessageBox::warning(this, "Profil incomplet",
            "Le profil doit contenir au moins un nœud avec un message.");
        return;
    }
    bool hasMessage = false;
    for (const auto& n : p.nodes) if (!n.messages.empty()) { hasMessage = true; break; }
    if (!hasMessage) {
        QMessageBox::warning(this, "Profil incomplet",
            "Chaque nœud doit avoir au moins un message.");
        return;
    }
    if (saveToFile(p))
        accept();
}

SimProfile SimProfileEditor::result() const {
    return buildProfile();
}

} // namespace socketspy::gui
