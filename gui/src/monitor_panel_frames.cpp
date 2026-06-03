// monitor_panel_frames.cpp — réception et affichage des trames CAN du
// MonitorPanel (log et tracking), helpers de cellule. Extrait de monitor_panel.cpp.
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
#include "monitor_panel_internal.h"

namespace socketspy::gui {

namespace {
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
} // namespace

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


} // namespace socketspy::gui
