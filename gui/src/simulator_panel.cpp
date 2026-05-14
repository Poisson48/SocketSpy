#include "simulator_panel.h"
#include "sim_profile.h"
#include "sim_profile_editor.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QHeaderView>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QMessageBox>

namespace socketspy::gui {

static const QStringList kBuiltinPaths = {
    ":/vehicles/generic_car.json",
    ":/vehicles/electric_scooter.json",
};

SimulatorPanel::SimulatorPanel(QWidget* parent) : QWidget(parent) {
    m_simulator = new CanSimulator(this);
    setupUi();
    populateProfiles();
}

void SimulatorPanel::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(6);

    auto* topBar = new QHBoxLayout;
    topBar->addWidget(new QLabel("Profile:", this));
    m_profileCombo = new QComboBox(this);
    topBar->addWidget(m_profileCombo, 1);

    m_newBtn    = new QPushButton("New…", this);
    m_deleteBtn = new QPushButton("Delete", this);
    m_deleteBtn->setEnabled(false);
    topBar->addWidget(m_newBtn);
    topBar->addWidget(m_deleteBtn);

    m_startBtn = new QPushButton("Start", this);
    m_startBtn->setCheckable(true);
    topBar->addWidget(m_startBtn);
    root->addLayout(topBar);

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(3);
    m_tree->setHeaderLabels({"Signal", "Value", "Range"});
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_tree->header()->resizeSection(1, 120);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    root->addWidget(m_tree, 1);

    connect(m_startBtn, &QPushButton::toggled, this, &SimulatorPanel::onStartStop);
    connect(m_profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SimulatorPanel::onProfileChanged);
    connect(m_newBtn,    &QPushButton::clicked, this, &SimulatorPanel::onNewProfile);
    connect(m_deleteBtn, &QPushButton::clicked, this, &SimulatorPanel::onDeleteProfile);
    connect(m_simulator, &CanSimulator::frameGenerated,
            this, &SimulatorPanel::frameGenerated);
}

void SimulatorPanel::populateProfiles() {
    const QString currentPath = m_profileCombo->count() > 0
        ? m_profileCombo->currentData().toString() : QString{};

    m_profileCombo->blockSignals(true);
    m_profileCombo->clear();

    for (const QString& path : kBuiltinPaths) {
        SimProfile p = load_sim_profile(path);
        if (!p.name.isEmpty())
            m_profileCombo->addItem(p.name, path);
    }

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                        + "/profiles";
    for (const QFileInfo& fi : QDir(dir).entryInfoList({"*.json"}, QDir::Files, QDir::Name)) {
        SimProfile p = load_sim_profile(fi.absoluteFilePath());
        if (!p.name.isEmpty())
            m_profileCombo->addItem(p.name, fi.absoluteFilePath());
    }

    m_profileCombo->blockSignals(false);

    // Restore previous selection if still present
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

void SimulatorPanel::onProfileChanged(int index) {
    if (index < 0 || index >= m_profileCombo->count()) return;
    if (m_simulator->isRunning()) {
        m_simulator->stop();
        m_startBtn->setChecked(false);
        m_startBtn->setText("Start");
    }
    const QString path = m_profileCombo->itemData(index).toString();
    m_currentProfile = load_sim_profile(path);
    m_simulator->load(m_currentProfile);
    populateTree();
    updateDeleteButton();
}

void SimulatorPanel::populateTree() {
    m_tree->clear();
    for (int ni = 0; ni < (int)m_currentProfile.nodes.size(); ++ni) {
        const auto& node = m_currentProfile.nodes[ni];
        auto* nodeItem = new QTreeWidgetItem(m_tree, {node.name});
        nodeItem->setExpanded(true);
        for (int mi = 0; mi < (int)node.messages.size(); ++mi) {
            const auto& msg = node.messages[mi];
            QString msgLabel = QString("0x%1 @ %2ms")
                .arg(msg.id, 0, 16, QChar('0')).toUpper()
                .arg(msg.period_ms);
            auto* msgItem = new QTreeWidgetItem(nodeItem, {msgLabel});
            msgItem->setExpanded(true);
            for (int si = 0; si < (int)msg.sigs.size(); ++si) {
                const auto& sig = msg.sigs[si];
                QString range = QString("[%1 – %2]")
                    .arg(sig.min).arg(sig.max);
                auto* sigItem = new QTreeWidgetItem(msgItem,
                    {sig.name, "", range});
                auto* spin = new QDoubleSpinBox(m_tree);
                spin->setRange(sig.min, sig.max);
                spin->setValue(sig.current_value);
                spin->setSingleStep((sig.max - sig.min) / 100.0);
                spin->setDecimals(2);
                m_tree->setItemWidget(sigItem, 1, spin);
                connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, [this, ni, mi, si](double v) {
                        m_simulator->setSignalValue(ni, mi, si, v);
                    });
            }
        }
    }
}

void SimulatorPanel::onStartStop() {
    if (m_startBtn->isChecked()) {
        m_simulator->start();
        m_startBtn->setText("Stop");
        m_deleteBtn->setEnabled(false);
    } else {
        m_simulator->stop();
        m_startBtn->setText("Start");
        updateDeleteButton();
    }
}

void SimulatorPanel::onNewProfile() {
    SimProfileEditor editor(this);
    if (editor.exec() != QDialog::Accepted) return;
    populateProfiles();
    // Select the newly created profile
    const int idx = m_profileCombo->findData(editor.savedPath());
    if (idx >= 0) m_profileCombo->setCurrentIndex(idx);
}

void SimulatorPanel::onDeleteProfile() {
    const int idx = m_profileCombo->currentIndex();
    if (idx < 0) return;
    const QString path = m_profileCombo->itemData(idx).toString();
    if (path.startsWith(':')) return;

    const QString name = m_profileCombo->itemText(idx);
    const auto btn = QMessageBox::question(this, "Delete profile",
        "Delete profile \"" + name + "\"?",
        QMessageBox::Yes | QMessageBox::No);
    if (btn != QMessageBox::Yes) return;

    QFile::remove(path);
    populateProfiles();
}

void SimulatorPanel::connectSignalWidgets() {}

} // namespace socketspy::gui
