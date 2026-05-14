#include "sim_profile_editor.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QFrame>
#include <QDialogButtonBox>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace socketspy::gui {

static QString msgLabel(uint32_t id, int period) {
    return QString("0x%1 @ %2ms").arg(id, 0, 16, QChar('0')).toUpper().arg(period);
}

SimProfileEditor::SimProfileEditor(QWidget* parent) : QDialog(parent) {
    setupUi();
}

void SimProfileEditor::setupUi() {
    setWindowTitle("New Simulator Profile");
    resize(620, 540);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    // Profile name
    auto* nameRow = new QHBoxLayout;
    nameRow->addWidget(new QLabel("Profile name:", this));
    m_profileName = new QLineEdit("My Profile", this);
    nameRow->addWidget(m_profileName, 1);
    root->addLayout(nameRow);

    // Action buttons
    auto* btnRow = new QHBoxLayout;
    m_addNodeBtn = new QPushButton("+ Node", this);
    m_addMsgBtn  = new QPushButton("+ Message", this);
    m_addSigBtn  = new QPushButton("+ Signal", this);
    m_removeBtn  = new QPushButton("Remove", this);
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
    auto* emptyPage = new QLabel("Select a node, message or signal to edit.", this);
    emptyPage->setAlignment(Qt::AlignCenter);
    m_stack->addWidget(emptyPage);

    // Page 1: Node
    auto* nodePage = new QWidget(this);
    auto* nodeForm = new QFormLayout(nodePage);
    m_nodeNameEdit = new QLineEdit(nodePage);
    nodeForm->addRow("Name:", m_nodeNameEdit);
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
    msgForm->addRow("ID (hex):", m_msgIdEdit);
    msgForm->addRow("Period:", m_msgPeriodSpin);
    msgForm->addRow("DLC:", m_msgDlcSpin);
    m_stack->addWidget(msgPage);

    // Page 3: Signal
    auto* sigPage = new QWidget(this);
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
    sigForm->addRow("Name:",         m_sigNameEdit);
    sigForm->addRow("Start bit:",    m_sigStartBitSpin);
    sigForm->addRow("Length (bits):", m_sigLengthSpin);
    sigForm->addRow("Factor:",       m_sigFactorSpin);
    sigForm->addRow("Offset:",       m_sigOffsetSpin);
    sigForm->addRow("Min:",          m_sigMinSpin);
    sigForm->addRow("Max:",          m_sigMaxSpin);
    sigForm->addRow("Default:",      m_sigDefaultSpin);
    m_stack->addWidget(sigPage);

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
    };
    connect(m_sigNameEdit, &QLineEdit::textChanged, this, syncSig);
    connect(m_sigStartBitSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, syncSig);
    connect(m_sigLengthSpin,   QOverload<int>::of(&QSpinBox::valueChanged), this, syncSig);
    connect(m_sigFactorSpin,   QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, syncSig);
    connect(m_sigOffsetSpin,   QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, syncSig);
    connect(m_sigMinSpin,      QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, syncSig);
    connect(m_sigMaxSpin,      QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, syncSig);
    connect(m_sigDefaultSpin,  QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, syncSig);
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
    return item;
}

// ── slots ─────────────────────────────────────────────────────────────────────

void SimProfileEditor::onAddNode() {
    auto* item = addNodeItem("NewNode");
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
                sj["name"]      = sig.name.toStdString();
                sj["start_bit"] = sig.start_bit;
                sj["length"]    = sig.length;
                sj["factor"]    = sig.factor;
                sj["offset"]    = sig.offset;
                sj["min"]       = sig.min;
                sj["max"]       = sig.max;
                sj["default"]   = sig.current_value;
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
    if (p.name.isEmpty() || p.nodes.empty()) {
        QMessageBox::warning(this, "Incomplete",
            "Profile needs a name and at least one node with a message.");
        return;
    }
    if (saveToFile(p))
        accept();
}

SimProfile SimProfileEditor::result() const {
    return buildProfile();
}

} // namespace socketspy::gui
