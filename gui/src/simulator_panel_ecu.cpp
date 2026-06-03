// simulator_panel_ecu.cpp — onglet ECU du SimulatorPanel et dialogue
// d'édition UdsEcuEditDialog (Q_OBJECT local). Extrait de simulator_panel.cpp.
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

#include "simulator_panel_ecu.moc"
