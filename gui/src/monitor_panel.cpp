// dbc_helper.h must be included before any Qt headers to avoid the
// `signals` macro collision with socketspy::dbc::Message::signals.
#include "dbc_helper.h"
#include "monitor_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QMenu>
#include <QCursor>
#include <QString>
#include <QLabel>
#include <QBrush>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <algorithm>

using namespace socketspy::core;
using namespace socketspy::dbc;

namespace socketspy::gui {

// ===========================================================================
// MonitorFilterDialog
// ===========================================================================

MonitorFilterDialog::MonitorFilterDialog(QWidget* parent)
    : QDialog(parent, Qt::Tool)
{
    setWindowTitle("Monitor Filters");
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);

    // ----- "Show only changes" group -----
    auto* changesGroup = new QGroupBox("Changes", this);
    m_changedOnly = new QCheckBox("Show only changes", changesGroup);
    m_changedOnly->setToolTip(
        "Track mode: hide IDs whose data never changes\n"
        "Log mode: skip duplicate data for the same ID");
    auto* changesLayout = new QVBoxLayout(changesGroup);
    changesLayout->addWidget(m_changedOnly);

    // ----- DLC filter group -----
    auto* dlcGroup = new QGroupBox("DLC filter", this);
    m_dlc = new QSpinBox(dlcGroup);
    m_dlc->setRange(0, 64);
    m_dlc->setSpecialValueText("Any");
    m_dlc->setToolTip("0 = accept any DLC; 1-64 = exact match");
    auto* dlcLayout = new QFormLayout(dlcGroup);
    dlcLayout->addRow("DLC (0 = any):", m_dlc);

    // ----- Timestamp range group -----
    auto* tsGroup = new QGroupBox("Timestamp range", this);
    m_useTs = new QCheckBox("Enable range filter", tsGroup);
    m_tsMin = new QDoubleSpinBox(tsGroup);
    m_tsMax = new QDoubleSpinBox(tsGroup);
    for (auto* sb : {m_tsMin, m_tsMax}) {
        sb->setRange(0.0, 1e12);
        sb->setDecimals(6);
        sb->setSuffix(" s");
        sb->setSingleStep(0.1);
    }
    m_tsMin->setToolTip("Minimum frame timestamp (seconds)");
    m_tsMax->setToolTip("Maximum frame timestamp (seconds, 0 = no upper bound)");

    auto* tsForm = new QFormLayout;
    tsForm->addRow(m_useTs);
    tsForm->addRow("From:", m_tsMin);
    tsForm->addRow("To:", m_tsMax);
    auto* tsGroupLayout = new QVBoxLayout(tsGroup);
    tsGroupLayout->addLayout(tsForm);

    // ----- Main layout -----
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(changesGroup);
    mainLayout->addWidget(dlcGroup);
    mainLayout->addWidget(tsGroup);
    mainLayout->addStretch();
    setMinimumWidth(280);

    // Connect all controls to onAnyChanged
    connect(m_changedOnly, &QCheckBox::toggled,        this, &MonitorFilterDialog::onAnyChanged);
    connect(m_dlc,         qOverload<int>(&QSpinBox::valueChanged),
                                                        this, &MonitorFilterDialog::onAnyChanged);
    connect(m_useTs,       &QCheckBox::toggled,        this, &MonitorFilterDialog::onAnyChanged);
    connect(m_tsMin,       qOverload<double>(&QDoubleSpinBox::valueChanged),
                                                        this, &MonitorFilterDialog::onAnyChanged);
    connect(m_tsMax,       qOverload<double>(&QDoubleSpinBox::valueChanged),
                                                        this, &MonitorFilterDialog::onAnyChanged);
}

MonitorFilter MonitorFilterDialog::filter() const {
    MonitorFilter f;
    f.changedOnly  = m_changedOnly->isChecked();
    f.dlc          = m_dlc->value();
    f.useTimestamp = m_useTs->isChecked();
    f.tsMin        = m_tsMin->value();
    f.tsMax        = m_tsMax->value();
    return f;
}

void MonitorFilterDialog::setFilter(const MonitorFilter& f) {
    // Block signals while loading to avoid spurious filterChanged emissions.
    QSignalBlocker b1(m_changedOnly), b2(m_dlc),
                   b3(m_useTs), b4(m_tsMin), b5(m_tsMax);
    m_changedOnly->setChecked(f.changedOnly);
    m_dlc->setValue(f.dlc);
    m_useTs->setChecked(f.useTimestamp);
    m_tsMin->setValue(f.tsMin);
    m_tsMax->setValue(f.tsMax);
}

void MonitorFilterDialog::onAnyChanged() {
    emit filterChanged(filter());
}

// ===========================================================================
// MonitorPanel
// ===========================================================================

MonitorPanel::MonitorPanel(QWidget* parent) : QWidget(parent) {
    m_dbc = std::make_unique<DbcDatabase>();
    setupUi();
}

MonitorPanel::~MonitorPanel() = default;
void MonitorPanel::setupUi() {
    m_table = new QTableWidget(0, 5, this);
    m_table->setHorizontalHeaderLabels(
        {"Timestamp (µs)", "ID (hex)", "DLC", "Data (hex)", "Decoded"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->hide();
    // Consistent row height across all tables
    m_table->verticalHeader()->setDefaultSectionSize(26);
    m_table->setAlternatingRowColors(true);

    connect(m_table, &QTableWidget::cellDoubleClicked,
            this,    &MonitorPanel::onCellDoubleClicked);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_table, &QTableWidget::customContextMenuRequested,
            this,    &MonitorPanel::onContextMenu);

    m_clear         = new QPushButton("Clear", this);
    m_clear->setObjectName("clearBtn");
    m_pause         = new QCheckBox("Pause", this);
    m_trackMode     = new QCheckBox("Track by ID", this);
    m_filterBtn     = new QPushButton("Filters…", this);
    m_filterBtn->setObjectName("filterBtn");
    m_filterBtn->setProperty("secondary", true);
    m_exportCsvBtn  = new QPushButton("Export CSV…", this);
    m_exportCsvBtn->setObjectName("filterBtn");   // reuse secondary style
    m_exportCsvBtn->setProperty("secondary", true);
    m_exportCsvBtn->setToolTip("Export visible rows to a CSV file");
    m_autoScrollChk = new QCheckBox("Auto-scroll", this);
    m_autoScrollChk->setChecked(true);
    m_showBusChk = new QCheckBox("Show Bus", this);
    m_showBusChk->setToolTip("Show/hide the Bus column (for multi-bus capture)");

    m_trackMode->setToolTip("One row per CAN ID, updated in place (adds Δt / rate / range columns)");
    m_filterBtn->setToolTip("Open filter configuration panel");
    m_autoScrollChk->setToolTip("Scroll to newest frame automatically");

    // Create the persistent, non-modal filter dialog.
    m_filterDlg = new MonitorFilterDialog(this);
    m_filterDlg->setFilter(m_monFilter);

    connect(m_clear,        &QPushButton::clicked, this, &MonitorPanel::onClear);
    connect(m_trackMode,    &QCheckBox::toggled,   this, &MonitorPanel::onTrackModeToggled);
    connect(m_showBusChk,   &QCheckBox::toggled,   this, [this](bool) {
        onTrackModeToggled(m_trackMode->isChecked());
        onClear();
    });
    connect(m_filterBtn,    &QPushButton::clicked, this, &MonitorPanel::onFiltersButtonClicked);
    connect(m_exportCsvBtn, &QPushButton::clicked, this, &MonitorPanel::onExportCsv);
    connect(m_filterDlg, &MonitorFilterDialog::filterChanged,
            this,        &MonitorPanel::onMonitorFilterChanged);

    m_captureBaselineBtn = new QPushButton("Capture Baseline", this);
    m_captureBaselineBtn->setToolTip("Snapshot current frames as baseline for noise filtering");
    m_clearBaselineBtn   = new QPushButton("Clear Baseline", this);
    m_clearBaselineBtn->setToolTip("Clear the noise baseline");
    m_filterNoiseChk     = new QCheckBox("Filter Noise", this);
    m_filterNoiseChk->setToolTip("Hide frames whose payload matches the captured baseline");

    connect(m_captureBaselineBtn, &QPushButton::clicked, this, [this]() {
        m_baseline.clear();
        for (int row = 0; row < m_table->rowCount(); ++row) {
            if (m_table->isRowHidden(row)) continue;
            auto* idItem   = m_table->item(row, 1);
            auto* dataItem = m_table->item(row, 3);
            if (!idItem || !dataItem) continue;
            QString idText = idItem->text();
            if (idText.startsWith("* ")) idText = idText.mid(2);
            bool ok = false;
            uint32_t id = idText.trimmed().toUInt(&ok, 16);
            if (!ok) continue;
            if (m_trackMode->isChecked()) {
                auto it = m_tracked.find(id);
                if (it != m_tracked.end()) {
                    const auto& d = it->second.lastData;
                    m_baseline[id] = QByteArray(
                        reinterpret_cast<const char*>(d.data()),
                        static_cast<qsizetype>(d.size()));
                }
            } else {
                const QStringList bytes = dataItem->text().split(' ', Qt::SkipEmptyParts);
                QByteArray payload;
                payload.reserve(bytes.size());
                for (const QString& b : bytes)
                    payload.append(static_cast<char>(b.toUInt(nullptr, 16)));
                m_baseline[id] = payload;
            }
        }
    });
    connect(m_clearBaselineBtn, &QPushButton::clicked, this, [this]() {
        m_baseline.clear();
    });

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText("Filter by ID (hex)…");
    m_search->setMaximumWidth(160);
    connect(m_search, &QLineEdit::textChanged, this, &MonitorPanel::onSearchChanged);

    auto* toolbar = new QHBoxLayout;
    toolbar->setSpacing(6);
    toolbar->addWidget(m_clear);
    toolbar->addWidget(m_pause);
    toolbar->addSpacing(12);
    toolbar->addWidget(m_trackMode);
    toolbar->addWidget(m_filterBtn);
    toolbar->addWidget(m_exportCsvBtn);
    toolbar->addWidget(m_autoScrollChk);
    toolbar->addWidget(m_showBusChk);
    toolbar->addSpacing(12);
    toolbar->addWidget(m_captureBaselineBtn);
    toolbar->addWidget(m_clearBaselineBtn);
    toolbar->addWidget(m_filterNoiseChk);
    toolbar->addSpacing(12);
    toolbar->addWidget(new QLabel("ID:", this));
    toolbar->addWidget(m_search);
    toolbar->addStretch();

    auto* layout = new QVBoxLayout(this);
    // Consistent outer margin 8px, inner spacing 6px
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    layout->addLayout(toolbar);
    layout->addWidget(m_table);
}

// ---------------------------------------------------------------------------
// Helper: does a frame pass the monitor-local filter (DLC, timestamp)?
// "changedOnly" is handled per-mode in appendLogRow / applyRowVisibility.


} // namespace socketspy::gui
