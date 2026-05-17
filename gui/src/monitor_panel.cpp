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

static const QColor kPinBg    {210, 230, 255};  // light blue — pinned row
static const QColor kChangedBg{255, 200, 100};  // amber    — data just changed

static void setCell(QTableWidget* t, int row, int col,
                    const QString& text, const QColor& bg = {})
{
    auto* item = new QTableWidgetItem(text);
    if (bg.isValid())
        item->setBackground(QBrush(bg));
    t->setItem(row, col, item);
}

// Build a space-separated uppercase hex string from a raw byte buffer.
static QString bytesToHex(const uint8_t* data, int len)
{
    QString hex;
    hex.reserve(len * 3);
    for (int i = 0; i < len; ++i) {
        if (i) hex += ' ';
        hex += QString("%1").arg(data[i], 2, 16, QChar('0')).toUpper();
    }
    return hex;
}

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

// Column indices for the log-mode table (5 cols base, 6 with Bus)
static constexpr int kColLogTs      = 0;
static constexpr int kColLogId      = 1;
static constexpr int kColLogDlc     = 2;
static constexpr int kColLogData    = 3;
static constexpr int kColLogDecoded = 4;
static constexpr int kColLogBus     = 5;

// Column indices for the track-mode table (8 cols)
static constexpr int kColTrkTs      = 0;
static constexpr int kColTrkId      = 1;
static constexpr int kColTrkDlc     = 2;
static constexpr int kColTrkData    = 3;
static constexpr int kColTrkDecoded = 4;
static constexpr int kColTrkDelta   = 5;  // Δ since last change (ms)
static constexpr int kColTrkRate    = 6;  // avg frames/sec
static constexpr int kColTrkRange   = 7;  // byte[0] min–max

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

bool MonitorPanel::passesMonitorFilter(const CanFrame& frame) const {
    if (m_monFilter.dlc != 0 && frame.dlc != m_monFilter.dlc)
        return false;

    if (m_monFilter.useTimestamp) {
        double ts = frame.timestamp_us / 1e6;
        if (ts < m_monFilter.tsMin) return false;
        if (m_monFilter.tsMax > 0.0 && ts > m_monFilter.tsMax) return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Frame routing

void MonitorPanel::onFrameReceived(CanFrame frame) {
    onFrameReceivedOnBus(frame, {});
}

void MonitorPanel::onFrameReceivedOnBus(CanFrame frame, QString busName) {
    if (m_pause->isChecked()) return;
    if (!m_filter.accepts(frame)) return;
    if (!passesMonitorFilter(frame)) return;

    if (m_filterNoiseChk->isChecked() && m_baseline.contains(frame.id)) {
        QByteArray payload(reinterpret_cast<const char*>(frame.data), frame.dlc);
        if (m_baseline[frame.id] == payload) return;
    }

    if (m_trackMode->isChecked())
        updateTrackingRow(frame, busName);
    else
        appendLogRow(frame, busName);
}

// ---------------------------------------------------------------------------
// Log mode

void MonitorPanel::appendLogRow(const CanFrame& frame, const QString& busName) {
    if (m_monFilter.changedOnly) {
        std::vector<uint8_t> data(frame.data, frame.data + frame.dlc);
        auto it = m_logLastSeen.find(frame.id);
        if (it != m_logLastSeen.end() &&
            it->second.first == data && it->second.second == frame.dlc)
            return;
        m_logLastSeen[frame.id] = {data, frame.dlc};
    }

    if (m_table->rowCount() >= kMaxRows)
        m_table->removeRow(0);

    int row = m_table->rowCount();
    m_table->insertRow(row);

    setCell(m_table, row, 0, QString::number(frame.timestamp_us));
    setCell(m_table, row, 1,
        QString("%1").arg(frame.id, 8, 16, QChar('0')).toUpper());
    setCell(m_table, row, 2, QString::number(frame.dlc));
    setCell(m_table, row, 3, bytesToHex(frame.data, frame.dlc));
    setCell(m_table, row, 4, decodeFrame(frame));
    if (m_table->columnCount() > kColLogBus)
        setCell(m_table, row, kColLogBus, busName);

    // Apply active search filter to the new row
    const QString filter = m_search->text().trimmed().toLower();
    if (!filter.isEmpty()) {
        auto* idItem = m_table->item(row, 1);
        if (idItem && !idItem->text().toLower().contains(filter))
            m_table->setRowHidden(row, true);
    }

    if (m_autoScrollChk->isChecked())
        m_table->scrollToBottom();
}

// ---------------------------------------------------------------------------
// Tracking mode

void MonitorPanel::updateTrackingRow(const CanFrame& frame, const QString& busName) {
    std::vector<uint8_t> data(frame.data, frame.data + frame.dlc);
    bool isPinned    = m_pinned.count(frame.id) > 0;
    bool dataChanged = false;
    bool isNew       = false;
    int  row         = -1;

    auto it = m_tracked.find(frame.id);
    if (it == m_tracked.end()) {
        // New ID: append a row
        row = m_table->rowCount();
        m_table->insertRow(row);
        TrackEntry entry;
        entry.row        = row;
        entry.lastData   = data;
        entry.lastDlc    = frame.dlc;
        entry.firstTs    = frame.timestamp_us;
        entry.lastTs     = frame.timestamp_us;
        entry.frameCount = 1;
        // Init min/max with first byte values
        for (int b = 0; b < frame.dlc && b < 8; ++b) {
            entry.byteMin[b] = frame.data[b];
            entry.byteMax[b] = frame.data[b];
        }
        m_tracked[frame.id] = std::move(entry);
        isNew = true;
    } else {
        auto& entry  = it->second;
        row          = entry.row;
        dataChanged  = (data != entry.lastData || frame.dlc != entry.lastDlc);
        if (dataChanged) {
            entry.everChanged = true;
            entry.lastData    = data;
            entry.lastDlc     = frame.dlc;
        }
        entry.prevTs = entry.lastTs;
        entry.lastTs = frame.timestamp_us;
        ++entry.frameCount;
        // Update per-byte min/max
        for (int b = 0; b < frame.dlc && b < 8; ++b) {
            entry.byteMin[b] = std::min(entry.byteMin[b], frame.data[b]);
            entry.byteMax[b] = std::max(entry.byteMax[b], frame.data[b]);
        }
    }

    auto& entry = m_tracked[frame.id];

    QColor rowBg  = isPinned ? kPinBg : QColor{};
    QColor dataBg = dataChanged ? kChangedBg : rowBg;

    QString idStr = isPinned
        ? QString("* %1").arg(frame.id, 8, 16, QChar('0')).toUpper()
        : QString("%1").arg(frame.id, 8, 16, QChar('0')).toUpper();

    setCell(m_table, row, kColTrkTs,      QString::number(frame.timestamp_us), rowBg);
    setCell(m_table, row, kColTrkId,      idStr, rowBg);
    setCell(m_table, row, kColTrkDlc,     QString::number(frame.dlc), rowBg);
    setCell(m_table, row, kColTrkData,    bytesToHex(frame.data, frame.dlc), dataBg);
    setCell(m_table, row, kColTrkDecoded, decodeFrame(frame), rowBg);

    // --- Gap A: Δt, rate, byte range columns ---
    // Δt: ms between the two most recent frames for this ID (blank until 2nd frame)
    if (isNew || entry.frameCount <= 1) {
        setCell(m_table, row, kColTrkDelta, "–", rowBg);
        setCell(m_table, row, kColTrkRate,  "–", rowBg);
        setCell(m_table, row, kColTrkRange, "–", rowBg);
    } else {
        // Instantaneous Δt (last two frames)
        double delta_ms = static_cast<double>(entry.lastTs - entry.prevTs) / 1000.0;

        // Average rate over entire observation window
        uint64_t elapsed_us = entry.lastTs - entry.firstTs;
        double elapsed_s    = static_cast<double>(elapsed_us) / 1e6;
        double rate         = (elapsed_s > 0.0)
                            ? (static_cast<double>(entry.frameCount - 1) / elapsed_s)
                            : 0.0;

        setCell(m_table, row, kColTrkDelta,
            QString::number(delta_ms, 'f', 1) + " ms", rowBg);
        setCell(m_table, row, kColTrkRate,
            QString::number(rate, 'f', 1), rowBg);

        // Byte 0 range (most representative byte for quick range check)
        if (frame.dlc > 0) {
            setCell(m_table, row, kColTrkRange,
                QString("B0: %1–%2")
                    .arg(entry.byteMin[0]).arg(entry.byteMax[0]),
                rowBg);
        } else {
            setCell(m_table, row, kColTrkRange, "–", rowBg);
        }
    }

    if (m_table->columnCount() > kColTrkRange + 1)
        setCell(m_table, row, kColTrkRange + 1, busName, rowBg);

    applyRowVisibility(row, frame.id);
}

// Hide/show a tracking row based on "changed only" + search filters.
void MonitorPanel::applyRowVisibility(int row, uint32_t id) {
    bool isPinned = m_pinned.count(id) > 0;

    auto it         = m_tracked.find(id);
    bool everChanged = it != m_tracked.end() && it->second.everChanged;
    bool hiddenByChanged = m_monFilter.changedOnly && !isPinned && !everChanged;

    const QString filter = m_search->text().trimmed().toLower();
    bool hiddenBySearch  = false;
    if (!filter.isEmpty()) {
        auto* item   = m_table->item(row, 1);
        hiddenBySearch = !item || !item->text().toLower().contains(filter);
    }

    m_table->setRowHidden(row, hiddenByChanged || hiddenBySearch);
}

// ---------------------------------------------------------------------------
// Slots

void MonitorPanel::onTrackModeToggled(bool tracking) {
    m_table->setRowCount(0);
    m_tracked.clear();
    m_pinned.clear();
    m_logLastSeen.clear();

    bool showBus = m_showBusChk && m_showBusChk->isChecked();
    if (tracking) {
        m_table->setColumnCount(showBus ? 9 : 8);
        QStringList hdrs = {"Timestamp (µs)", "ID (hex)", "DLC", "Data (hex)", "Decoded",
                            "Δt (ms)", "Rate (f/s)", "B0 min–max"};
        if (showBus) hdrs << "Bus";
        m_table->setHorizontalHeaderLabels(hdrs);
    } else {
        m_table->setColumnCount(showBus ? 6 : 5);
        QStringList hdrs = {"Timestamp (µs)", "ID (hex)", "DLC", "Data (hex)", "Decoded"};
        if (showBus) hdrs << "Bus";
        m_table->setHorizontalHeaderLabels(hdrs);
    }
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

void MonitorPanel::applyMonitorFilter(const MonitorFilter& f) {
    m_filterDlg->setFilter(f);
    onMonitorFilterChanged(f);
}

void MonitorPanel::onMonitorFilterChanged(const MonitorFilter& f) {
    m_monFilter = f;

    // Re-evaluate row visibility immediately in tracking mode.
    if (m_trackMode->isChecked()) {
        for (auto& [id, entry] : m_tracked)
            applyRowVisibility(entry.row, id);
    }
    // In log mode, changedOnly / DLC / timestamp apply to future frames only;
    // the existing rows are not retroactively hidden.
}

void MonitorPanel::onFiltersButtonClicked() {
    if (m_filterDlg->isVisible()) {
        m_filterDlg->hide();
    } else {
        // Position the dialog just below the Filters button.
        QPoint globalPos = m_filterBtn->mapToGlobal(
            QPoint(0, m_filterBtn->height()));
        m_filterDlg->move(globalPos);
        m_filterDlg->show();
        m_filterDlg->raise();
        m_filterDlg->activateWindow();
    }
}

void MonitorPanel::onFilterChanged(const FrameFilter& filter) {
    m_filter = filter;
}

void MonitorPanel::onDbcLoaded(DbcDatabase db) {
    *m_dbc      = std::move(db);
    m_dbcLoaded = true;
}

void MonitorPanel::onClear() {
    m_table->setRowCount(0);
    m_tracked.clear();
    m_pinned.clear();
    m_logLastSeen.clear();
    m_search->clear();
}

void MonitorPanel::onSearchChanged(const QString& text) {
    const QString filter = text.trimmed().toLower();

    if (m_trackMode->isChecked()) {
        for (auto& [id, entry] : m_tracked)
            applyRowVisibility(entry.row, id);
        return;
    }

    for (int row = 0; row < m_table->rowCount(); ++row) {
        auto* item = m_table->item(row, 1);
        bool match = filter.isEmpty() || (item && item->text().toLower().contains(filter));
        m_table->setRowHidden(row, !match);
    }
}

void MonitorPanel::onCellDoubleClicked(int row, int /*col*/) {
    if (!m_dbcLoaded) return;
    auto* idItem = m_table->item(row, 1);
    if (!idItem) return;
    // Strip tracking-mode pin marker "* "
    QString idText = idItem->text();
    if (idText.startsWith("* ")) idText = idText.mid(2);
    bool ok = false;
    uint32_t id = idText.trimmed().toUInt(&ok, 16);
    if (!ok) return;

    auto names = dbc_helper::signal_names_for_msg(*m_dbc, id);
    if (names.empty()) return;

    // Single signal: add directly; multiple signals: show pick menu
    if (names.size() == 1) {
        emit signalDoubleClicked(QString::fromStdString(names[0]), id);
        return;
    }

    QMenu pickMenu(this);
    QList<QAction*> acts;
    for (const auto& n : names)
        acts << pickMenu.addAction(QString::fromStdString(n));

    auto* chosen = pickMenu.exec(QCursor::pos());
    for (int i = 0; i < acts.size(); ++i) {
        if (chosen == acts[i]) {
            emit signalDoubleClicked(QString::fromStdString(names[i]), id);
            return;
        }
    }
}

void MonitorPanel::onContextMenu(const QPoint& pos) {
    const QModelIndex idx = m_table->indexAt(pos);
    if (!idx.isValid()) return;
    auto* idItem = m_table->item(idx.row(), 1);
    if (!idItem) return;

    QString idText = idItem->text();
    if (idText.startsWith("* ")) idText = idText.mid(2);
    bool ok = false;
    uint32_t id = idText.trimmed().toUInt(&ok, 16);
    if (!ok) return;

    QMenu menu(this);
    auto* exportAct = menu.addAction(tr("Export visible rows to CSV…"));
    menu.addSeparator();
    QMenu* graphMenu = menu.addMenu(tr("Add to graph"));
    auto* graphAllAct = graphMenu->addAction(
        tr("All signals of 0x%1").arg(id, 0, 16).toUpper());

    // Per-signal submenu items (when DBC is loaded)
    QList<QAction*> sigActs;
    if (m_dbcLoaded) {
        auto names = dbc_helper::signal_names_for_msg(*m_dbc, id);
        if (!names.empty()) {
            graphMenu->addSeparator();
            for (const auto& name : names)
                sigActs << graphMenu->addAction(QString::fromStdString(name));
        }
    }

    QAction* pinAct = nullptr;
    if (m_trackMode->isChecked()) {
        menu.addSeparator();
        pinAct = m_pinned.count(id)
            ? menu.addAction(QString("Unpin 0x%1").arg(id, 0, 16).toUpper())
            : menu.addAction(QString("Pin 0x%1").arg(id, 0, 16).toUpper());
    }

    auto* chosen = menu.exec(m_table->viewport()->mapToGlobal(pos));

    if (chosen == exportAct) {
        onExportCsv();
        return;
    }

    if (chosen == graphAllAct) {
        emit frameGraphRequested(id);
        return;
    }

    for (int i = 0; i < sigActs.size(); ++i) {
        if (chosen == sigActs[i]) {
            auto names = dbc_helper::signal_names_for_msg(*m_dbc, id);
            if (i < static_cast<int>(names.size()))
                emit signalDoubleClicked(QString::fromStdString(names[i]), id);
            return;
        }
    }

    if (pinAct && chosen == pinAct) {
        int row = idx.row();
        if (m_pinned.count(id)) {
            m_pinned.erase(id);
            // Repaint row with default background and strip pin marker
            for (int col = 0; col < m_table->columnCount(); ++col) {
                auto* item = m_table->item(row, col);
                if (item) item->setBackground(QBrush{});
            }
            if (auto* item = m_table->item(row, 1))
                item->setText(QString("%1").arg(id, 8, 16, QChar('0')).toUpper());
            applyRowVisibility(row, id);
        } else {
            m_pinned.insert(id);
            for (int col = 0; col < m_table->columnCount(); ++col) {
                auto* item = m_table->item(row, col);
                if (item) item->setBackground(QBrush(kPinBg));
            }
            if (auto* item = m_table->item(row, 1))
                item->setText(QString("* %1").arg(id, 8, 16, QChar('0')).toUpper());
            m_table->setRowHidden(row, false);
        }
    }
}

// ---------------------------------------------------------------------------

void MonitorPanel::setAliases(const QHash<QString,QString>& aliases) {
    m_aliases = aliases;
}

// ---------------------------------------------------------------------------
// Gap C — CSV export

void MonitorPanel::onExportCsv() {
    const int rowCount = m_table->rowCount();
    const int colCount = m_table->columnCount();
    if (rowCount == 0) {
        QMessageBox::information(this, tr("Export CSV"), tr("No data to export."));
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export Monitor CSV"), {},
        tr("CSV Files (*.csv);;All Files (*)"));
    if (path.isEmpty()) return;

    QFile file(path.endsWith(".csv") ? path : path + ".csv");
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Export CSV"),
            tr("Cannot write file: %1").arg(file.errorString()));
        return;
    }

    QTextStream out(&file);

    // Write header row from current column labels
    QStringList header;
    for (int c = 0; c < colCount; ++c) {
        auto* h = m_table->horizontalHeaderItem(c);
        header << (h ? h->text() : QString("Col%1").arg(c));
    }
    out << header.join(',') << '\n';

    // Write visible data rows
    int exported = 0;
    for (int r = 0; r < rowCount; ++r) {
        if (m_table->isRowHidden(r)) continue;
        QStringList row;
        for (int c = 0; c < colCount; ++c) {
            auto* item = m_table->item(r, c);
            QString val = item ? item->text() : QString();
            // Escape commas and quotes per RFC 4180
            if (val.contains(',') || val.contains('"') || val.contains('\n')) {
                val = '"' + val.replace('"', "\"\"") + '"';
            }
            row << val;
        }
        out << row.join(',') << '\n';
        ++exported;
    }

    QMessageBox::information(this, tr("Export CSV"),
        tr("Exported %1 rows to:\n%2").arg(exported).arg(file.fileName()));
}

QString MonitorPanel::decodeFrame(const CanFrame& f) const {
    QString result;

    if (m_dbcLoaded) {
        std::span<const uint8_t> data(f.data, f.dlc);
        result = QString::fromStdString(dbc_helper::decode_frame(*m_dbc, f.id, data));
        // Apply DBC signal name aliases (replace "SigName=" with "Alias=" in output)
        if (!m_aliases.isEmpty() && !result.isEmpty()) {
            for (auto it = m_aliases.cbegin(); it != m_aliases.cend(); ++it) {
                if (!it.key().startsWith("0x"))
                    result.replace(it.key() + "=", it.value() + "=");
            }
        }
    }

    // Show raw byte aliases even when no DBC match
    if (!m_aliases.isEmpty()) {
        for (int b = 0; b < f.dlc; ++b) {
            const QString canonical = QString("0x%1[B%2]")
                .arg(QString::number(f.id, 16).toUpper()).arg(b);
            auto it = m_aliases.find(canonical);
            if (it == m_aliases.end() || it.value().isEmpty()) continue;
            if (!result.isEmpty()) result += "  ";
            result += QString("%1=%2").arg(it.value()).arg(f.data[b]);
        }
    }

    return result;
}

} // namespace socketspy::gui
