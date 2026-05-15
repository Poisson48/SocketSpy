// dbc_helper.h must be included before any Qt headers to avoid the
// `signals` macro collision with socketspy::dbc::Message::signals.
#include "dbc_helper.h"
#include "stats_panel.h"
#include "cancore.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QTableWidgetItem>

using namespace socketspy::core;

namespace socketspy::gui {

StatsPanel::StatsPanel(QWidget* parent) : QWidget(parent) {
    setupUi();
    m_uptime.start();
}

void StatsPanel::setupUi() {
    // ---- Summary bar (shared across both tabs) ----
    m_totalLabel  = new QLabel("Total frames: 0",  this);
    m_loadLabel   = new QLabel("Bus load: 0.0 %",   this);
    m_errorLabel  = new QLabel("Errors: 0",          this);
    m_uptimeLabel = new QLabel("Uptime: 00:00:00",   this);
    m_resetBtn    = new QPushButton("Reset",          this);
    m_resetBtn->setObjectName("resetBtn");
    m_resetBtn->setFixedWidth(72);

    auto* bar = new QHBoxLayout;
    bar->setSpacing(12);
    bar->addWidget(m_totalLabel);
    bar->addWidget(m_loadLabel);
    bar->addWidget(m_errorLabel);
    bar->addWidget(m_uptimeLabel);
    bar->addStretch();
    bar->addWidget(m_resetBtn);

    // ---- Tab 0 — per-ID bus stats ----
    m_table = new QTableWidget(0, 8, this);
    m_table->setHorizontalHeaderLabels(
        {"CAN ID", "Name", "Count", "Rate (f/s)", "Last DLC", "Last data",
         "B0 min", "B0 max"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->horizontalHeader()->setSortIndicatorShown(true);
    m_table->setSortingEnabled(true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->hide();
    m_table->verticalHeader()->setDefaultSectionSize(26);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->sortByColumn(2, Qt::DescendingOrder);

    auto* busTab = new QWidget(this);
    auto* busLayout = new QVBoxLayout(busTab);
    busLayout->setContentsMargins(0, 4, 0, 0);
    busLayout->setSpacing(4);
    busLayout->addWidget(m_table);

    // ---- Tab 1 — per-signal decoded stats (Gap B) ----
    m_signalTable = new QTableWidget(0, 7, this);
    m_signalTable->setHorizontalHeaderLabels(
        {"Message", "Signal", "Count", "Last value", "Min", "Max", "Mean"});
    m_signalTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_signalTable->horizontalHeader()->setSortIndicatorShown(true);
    m_signalTable->setSortingEnabled(true);
    m_signalTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_signalTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_signalTable->setAlternatingRowColors(true);
    m_signalTable->verticalHeader()->hide();
    m_signalTable->verticalHeader()->setDefaultSectionSize(26);
    m_signalTable->horizontalHeader()->setStretchLastSection(true);
    m_signalTable->sortByColumn(2, Qt::DescendingOrder);

    auto* sigTab = new QWidget(this);
    auto* sigLayout = new QVBoxLayout(sigTab);
    sigLayout->setContentsMargins(0, 4, 0, 0);
    sigLayout->setSpacing(4);
    sigLayout->addWidget(new QLabel(
        "Decoded signal statistics — requires a DBC file to be loaded", sigTab));
    sigLayout->addWidget(m_signalTable);

    // ---- Tab widget ----
    m_tabs = new QTabWidget(this);
    m_tabs->addTab(busTab,  "Bus / ID");
    m_tabs->addTab(sigTab,  "Signals");

    // ---- Root layout ----
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);
    root->addLayout(bar);
    root->addWidget(m_tabs);

    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &StatsPanel::onTick);
    m_timer->start();

    connect(m_resetBtn, &QPushButton::clicked, this, &StatsPanel::onReset);
}

uint64_t StatsPanel::bitsPerFrame(const CanFrame& f) {
    const bool isFd       = f.flags & static_cast<uint8_t>(FrameFlags::FD);
    const bool isExtended = f.id & 0x80000000u;
    if (isFd)       return 64u + 8u * f.dlc;
    if (isExtended) return 64u + 8u * f.dlc;
    return 44u + 8u * static_cast<uint64_t>(f.dlc);
}

void StatsPanel::onFrameReceived(CanFrame frame) {
    const bool isError = frame.flags & static_cast<uint8_t>(FrameFlags::Error);
    if (isError) {
        ++m_totalErrors;
        return;
    }

    ++m_totalFrames;

    auto& s = m_stats[frame.id];
    ++s.count;
    s.bits_in_window += bitsPerFrame(frame);
    s.last_dlc = frame.dlc;
    const int copyBytes = qMin(static_cast<int>(frame.dlc), 8);
    for (int i = 0; i < copyBytes; ++i)
        s.last_data[i] = frame.data[i];
    for (int i = copyBytes; i < 8; ++i)
        s.last_data[i] = 0;

    // Gap B — track byte-0 min/max
    if (frame.dlc > 0) {
        s.b0_min = qMin(s.b0_min, frame.data[0]);
        s.b0_max = qMax(s.b0_max, frame.data[0]);
        s.b0_sum += frame.data[0];
    }

    m_bitsInWindow += bitsPerFrame(frame);

    // Gap B — decode signals and accumulate stats if DBC is loaded
    if (m_dbcLoaded) {
        std::span<const uint8_t> data(frame.data, frame.dlc);
        auto sigNames = socketspy::gui::dbc_helper::signal_names_for_msg(m_dbc, frame.id);
        for (const auto& sigName : sigNames) {
            auto val = socketspy::gui::dbc_helper::decode_signal(
                m_dbc, frame.id, sigName, data);
            if (!val.has_value()) continue;

            const QString key = QString("%1::%2")
                .arg(frame.id, 8, 16, QChar('0')).arg(QString::fromStdString(sigName));
            auto& ss = m_sigStats[key];
            if (ss.count == 0) {
                // First observation — fill metadata
                for (const auto& msg : m_dbc.messages) {
                    if (msg.id == (frame.id & 0x1FFFFFFFu)) {
                        ss.msgName = QString::fromStdString(msg.name);
                        ss.msgId   = frame.id;
                        break;
                    }
                }
                ss.sigName = QString::fromStdString(sigName);
                ss.unit    = QString::fromStdString(
                    socketspy::gui::dbc_helper::signal_unit(m_dbc, frame.id, sigName));
            }
            ++ss.count;
            ss.valLast = *val;
            if (*val < ss.valMin) ss.valMin = *val;
            if (*val > ss.valMax) ss.valMax = *val;
            ss.valSum += *val;
        }
    }
}

void StatsPanel::onDbcLoaded(socketspy::dbc::DbcDatabase db) {
    m_dbc       = std::move(db);
    m_dbcLoaded = true;
    m_sigStats.clear();
    m_signalTable->setRowCount(0);
}

void StatsPanel::refreshSignalTable() {
    if (!m_dbcLoaded || m_sigStats.isEmpty()) return;

    m_signalTable->setUpdatesEnabled(false);
    m_signalTable->setSortingEnabled(false);

    const int needed = m_sigStats.size();
    const int existing = m_signalTable->rowCount();
    m_signalTable->setRowCount(needed);
    for (int r = existing; r < needed; ++r) {
        for (int c = 0; c < m_signalTable->columnCount(); ++c)
            m_signalTable->setItem(r, c, new QTableWidgetItem);
    }

    int row = 0;
    for (auto it = m_sigStats.cbegin(); it != m_sigStats.cend(); ++it, ++row) {
        const PerSignalStats& ss = it.value();

        double mean = (ss.count > 0) ? (ss.valSum / static_cast<double>(ss.count)) : 0.0;
        QString unitSuffix = ss.unit.isEmpty() ? QString() : ("  " + ss.unit);

        auto setCell = [&](int col, const QString& text, QVariant sortKey = {}) {
            QTableWidgetItem* item = m_signalTable->item(row, col);
            item->setText(text);
            if (sortKey.isValid())
                item->setData(Qt::UserRole, sortKey);
        };

        setCell(0, ss.msgName);
        setCell(1, ss.sigName);
        setCell(2, QString::number(ss.count),
            QVariant::fromValue<qulonglong>(ss.count));
        setCell(3, QString::number(ss.valLast, 'f', 3) + unitSuffix);
        setCell(4, ss.count > 0
            ? QString::number(ss.valMin, 'f', 3) + unitSuffix
            : "–");
        setCell(5, ss.count > 0
            ? QString::number(ss.valMax, 'f', 3) + unitSuffix
            : "–");
        setCell(6, ss.count > 0
            ? QString::number(mean, 'f', 3) + unitSuffix
            : "–");
    }

    m_signalTable->setSortingEnabled(true);
    m_signalTable->sortByColumn(2, Qt::DescendingOrder);
    m_signalTable->setUpdatesEnabled(true);
}

void StatsPanel::setBitrate(int bitrate) {
    if (bitrate > 0)
        m_bitrate = static_cast<uint64_t>(bitrate);
}

void StatsPanel::onTick() {
    m_busLoad = static_cast<double>(m_bitsInWindow) / static_cast<double>(m_bitrate) * 100.0;
    if (m_busLoad > 100.0) m_busLoad = 100.0;
    m_bitsInWindow = 0;

    const qint64 elapsedMs   = m_uptime.elapsed();
    const qint64 totalSec    = elapsedMs / 1000;
    const qint64 h  = totalSec / 3600;
    const qint64 m  = (totalSec % 3600) / 60;
    const qint64 s  = totalSec % 60;
    const QString uptimeStr  = QString("%1:%2:%3")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'));

    m_totalLabel->setText(QString("Total frames: %1")
        .arg(m_totalFrames));
    m_loadLabel->setText(QString("Bus load: %1 %")
        .arg(m_busLoad, 0, 'f', 1));
    m_errorLabel->setText(QString("Errors: %1")
        .arg(m_totalErrors));
    m_uptimeLabel->setText("Uptime: " + uptimeStr);

    m_table->setUpdatesEnabled(false);
    m_table->setSortingEnabled(false);

    const int existingRows = m_table->rowCount();
    const int needed       = m_stats.size();
    m_table->setRowCount(needed);
    const int colCount = m_table->columnCount();
    for (int r = existingRows; r < needed; ++r) {
        for (int c = 0; c < colCount; ++c)
            m_table->setItem(r, c, new QTableWidgetItem);
    }

    int row = 0;
    for (auto it = m_stats.cbegin(); it != m_stats.cend(); ++it, ++row) {
        const uint32_t  id  = it.key();
        const PerIdStats& ps = it.value();

        const uint64_t rate = ps.count - ps.count_last_sec;

        QString name;
        if (m_dbcLoaded) {
            for (const auto& msg : m_dbc.messages) {
                if (msg.id == (id & 0x1FFFFFFFu)) {
                    name = QString::fromStdString(msg.name);
                    break;
                }
            }
        }

        const int copyBytes = qMin(static_cast<int>(ps.last_dlc), 8);
        QStringList hexBytes;
        for (int b = 0; b < copyBytes; ++b)
            hexBytes << QString("%1").arg(ps.last_data[b], 2, 16, QChar('0')).toUpper();
        const QString dataStr = hexBytes.join(' ');

        auto setCell = [&](int col, const QString& text, QVariant sortKey = {}) {
            QTableWidgetItem* item = m_table->item(row, col);
            item->setText(text);
            if (sortKey.isValid())
                item->setData(Qt::UserRole, sortKey);
        };

        setCell(0, QString("0x%1").arg(id, 8, 16, QChar('0')).toUpper());
        setCell(1, name);
        setCell(2, QString::number(ps.count), QVariant::fromValue<qulonglong>(ps.count));
        setCell(3, QString::number(rate),     QVariant::fromValue<qulonglong>(rate));
        setCell(4, QString::number(ps.last_dlc));
        setCell(5, dataStr);
        // Gap B — B0 min/max
        if (ps.last_dlc > 0 && ps.count > 0) {
            setCell(6, QString::number(ps.b0_min),
                QVariant::fromValue<uint>(ps.b0_min));
            setCell(7, QString::number(ps.b0_max),
                QVariant::fromValue<uint>(ps.b0_max));
        } else {
            setCell(6, "–");
            setCell(7, "–");
        }
    }

    for (auto it = m_stats.begin(); it != m_stats.end(); ++it)
        it.value().count_last_sec = it.value().count;

    m_table->setSortingEnabled(true);
    m_table->sortByColumn(2, Qt::DescendingOrder);
    m_table->setUpdatesEnabled(true);

    // Refresh signal stats tab
    refreshSignalTable();
}

void StatsPanel::onReset() {
    m_stats.clear();
    m_sigStats.clear();
    m_totalFrames  = 0;
    m_totalErrors  = 0;
    m_bitsInWindow = 0;
    m_busLoad      = 0.0;
    m_table->setRowCount(0);
    m_signalTable->setRowCount(0);
    m_uptime.restart();

    m_totalLabel->setText("Total frames: 0");
    m_loadLabel->setText("Bus load: 0.0 %");
    m_errorLabel->setText("Errors: 0");
    m_uptimeLabel->setText("Uptime: 00:00:00");
}

} // namespace socketspy::gui
