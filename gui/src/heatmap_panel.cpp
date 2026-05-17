#include "heatmap_panel.h"
#include "heatmap_widget.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QTimer>
#include <QDateTime>
#include <QGroupBox>

namespace socketspy::gui {

HeatmapPanel::HeatmapPanel(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void HeatmapPanel::setupUi() {
    // --- Left: ID list ---
    auto* leftBox  = new QGroupBox(tr("CAN IDs"), this);
    auto* leftVBox = new QVBoxLayout(leftBox);
    leftVBox->setContentsMargins(4, 4, 4, 4);

    m_idList = new QListWidget(this);
    m_idList->setMinimumWidth(180);
    m_idList->setMaximumWidth(240);
    m_idList->setFont(QFont("Monospace", 9));
    leftVBox->addWidget(m_idList);

    // --- Right: heatmap + legend ---
    auto* rightBox  = new QGroupBox(tr("Bit activity"), this);
    auto* rightVBox = new QVBoxLayout(rightBox);
    rightVBox->setContentsMargins(8, 8, 8, 8);

    m_heatmap = new HeatmapWidget(this);
    rightVBox->addWidget(m_heatmap);

    // Legend row
    auto* legendRow = new QHBoxLayout;
    auto makeLegend = [&](const QString& label, QColor col) {
        auto* swatch = new QLabel(this);
        swatch->setFixedSize(14, 14);
        swatch->setStyleSheet(QString("background:%1; border:1px solid #555;").arg(col.name()));
        legendRow->addWidget(swatch);
        legendRow->addWidget(new QLabel(label, this));
        legendRow->addSpacing(8);
    };
    makeLegend(tr("Never seen"),   QColor(160, 160, 160));
    makeLegend(tr("Stable 0"),     QColor(80,  200, 100));
    makeLegend(tr("Stable 1"),     QColor(60,  130, 220));
    makeLegend(tr("Moderate"),     QColor(230, 130, 30));
    makeLegend(tr("Frequent"),     QColor(220, 50,  50));
    legendRow->addStretch();
    rightVBox->addLayout(legendRow);
    rightVBox->addStretch();

    // --- Main layout ---
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);
    mainLayout->addWidget(leftBox);
    mainLayout->addWidget(rightBox, 1);

    // --- 30 fps refresh timer ---
    m_timer = new QTimer(this);
    m_timer->setInterval(33);   // ~30 fps
    connect(m_timer, &QTimer::timeout, this, &HeatmapPanel::onRefreshTimer);
    m_timer->start();

    connect(m_idList, &QListWidget::itemClicked,
            this,     &HeatmapPanel::onIdSelected);
}

// ---------------------------------------------------------------------------
// Frame ingestion
// ---------------------------------------------------------------------------

void HeatmapPanel::onFrameReceived(const socketspy::core::CanFrame& frame) {
    if (frame.dlc == 0) return;

    HeatmapState& st = m_states[frame.id];
    ++st.frame_count;

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const int    nbits = static_cast<int>(frame.dlc) * 8;

    for (int i = 0; i < nbits && i < 64; ++i) {
        const int    byte   = i / 8;
        const int    bit    = i % 8;
        const uint8_t mask  = static_cast<uint8_t>(1u << bit);
        const uint8_t val   = (frame.data[byte] & mask) ? 1u : 0u;

        if (val != st.last_val[i]) {
            ++st.toggle_count[i];
            st.last_toggle_ms[i] = nowMs;
            st.last_val[i]       = val;
        } else if (st.last_toggle_ms[i] == 0) {
            // First time we see this bit — record it even without a toggle
            st.last_toggle_ms[i] = nowMs;
            st.last_val[i]       = val;
        }
    }

    updateIdList(frame.id);

    if (frame.id == m_selectedId)
        m_dirty = true;
}

// ---------------------------------------------------------------------------
// ID list management
// ---------------------------------------------------------------------------

void HeatmapPanel::updateIdList(uint32_t id) {
    // Check if already present
    for (int i = 0; i < m_idList->count(); ++i) {
        QListWidgetItem* it = m_idList->item(i);
        if (it->data(Qt::UserRole).toUInt() == id) {
            // Update frame count label
            const HeatmapState& st = m_states[id];
            it->setText(QString("0x%1  (%2)")
                .arg(id, 8, 16, QChar('0')).toUpper()
                .arg(st.frame_count));
            return;
        }
    }
    // New ID — add item
    auto* item = new QListWidgetItem(
        QString("0x%1  (%2)")
            .arg(id, 8, 16, QChar('0')).toUpper()
            .arg(1ULL));
    item->setData(Qt::UserRole, id);
    m_idList->addItem(item);

    // Auto-select first ID
    if (m_selectedId == 0xFFFFFFFF) {
        m_selectedId = id;
        m_idList->setCurrentItem(item);
        m_dirty = true;
    }
}

// ---------------------------------------------------------------------------
// Selection
// ---------------------------------------------------------------------------

void HeatmapPanel::onIdSelected(QListWidgetItem* item) {
    if (!item) return;
    m_selectedId = item->data(Qt::UserRole).toUInt();
    m_dirty = true;
    pushBitsToWidget();
}

// ---------------------------------------------------------------------------
// Timer → repaint
// ---------------------------------------------------------------------------

void HeatmapPanel::onRefreshTimer() {
    if (m_selectedId == 0xFFFFFFFF) return;
    // Always push so fade animation updates even without new frames
    pushBitsToWidget();
}

void HeatmapPanel::pushBitsToWidget() {
    if (m_selectedId == 0xFFFFFFFF) return;
    const HeatmapState& st = m_states[m_selectedId];

    BitState bits[64];
    for (int i = 0; i < 64; ++i) {
        bits[i].toggle_count   = st.toggle_count[i];
        bits[i].last_toggle_ms = st.last_toggle_ms[i];
        bits[i].last_val       = st.last_val[i];
    }
    m_heatmap->setBits(bits);
    m_dirty = false;
}

} // namespace socketspy::gui
