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
    auto* root = new QVBoxLayout(this);
    // Consistent outer margin 8px, inner spacing 6px
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    auto* bar = new QHBoxLayout;
    bar->setSpacing(12);

    m_totalLabel  = new QLabel("Total frames: 0",  this);
    m_loadLabel   = new QLabel("Bus load: 0.0 %",   this);
    m_errorLabel  = new QLabel("Errors: 0",          this);
    m_uptimeLabel = new QLabel("Uptime: 00:00:00",   this);
    m_resetBtn    = new QPushButton("Reset",          this);
    m_resetBtn->setObjectName("resetBtn");
    m_resetBtn->setFixedWidth(72);

    bar->addWidget(m_totalLabel);
    bar->addWidget(m_loadLabel);
    bar->addWidget(m_errorLabel);
    bar->addWidget(m_uptimeLabel);
    bar->addStretch();
    bar->addWidget(m_resetBtn);
    root->addLayout(bar);

    m_table = new QTableWidget(0, 6, this);
    m_table->setHorizontalHeaderLabels(
        {"CAN ID", "Name", "Count", "Rate (f/s)", "Last DLC", "Last data"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->horizontalHeader()->setSortIndicatorShown(true);
    m_table->setSortingEnabled(true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->hide();
    // Consistent row height matching other tables
    m_table->verticalHeader()->setDefaultSectionSize(26);
    m_table->horizontalHeader()->setStretchLastSection(true);

    m_table->sortByColumn(2, Qt::DescendingOrder);

    root->addWidget(m_table);

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

    m_bitsInWindow += bitsPerFrame(frame);
}

void StatsPanel::onDbcLoaded(socketspy::dbc::DbcDatabase db) {
    m_dbc       = std::move(db);
    m_dbcLoaded = true;
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
    }

    for (auto it = m_stats.begin(); it != m_stats.end(); ++it)
        it.value().count_last_sec = it.value().count;

    m_table->setSortingEnabled(true);
    m_table->sortByColumn(2, Qt::DescendingOrder);
    m_table->setUpdatesEnabled(true);
}

void StatsPanel::onReset() {
    m_stats.clear();
    m_totalFrames  = 0;
    m_totalErrors  = 0;
    m_bitsInWindow = 0;
    m_busLoad      = 0.0;
    m_table->setRowCount(0);
    m_uptime.restart();

    m_totalLabel->setText("Total frames: 0");
    m_loadLabel->setText("Bus load: 0.0 %");
    m_errorLabel->setText("Errors: 0");
    m_uptimeLabel->setText("Uptime: 00:00:00");
}

} // namespace socketspy::gui
