#include "range_state_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QProgressBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QThread>

namespace socketspy::gui {

static constexpr int kMaxPayloads = 500;

RangeStatePanel::RangeStatePanel(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void RangeStatePanel::setupUi() {
    m_idCombo = new QComboBox(this);
    m_idCombo->setMinimumWidth(120);
    m_idCombo->setToolTip(tr("Select CAN ID to scan"));

    m_scanBtn = new QPushButton(tr("Scan"), this);
    m_scanBtn->setToolTip(tr("Run heuristic signal scan on buffered frames"));

    auto* topRow = new QHBoxLayout;
    topRow->addWidget(new QLabel(tr("CAN ID:"), this));
    topRow->addWidget(m_idCombo, 1);
    topRow->addWidget(m_scanBtn);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setTextVisible(true);
    m_progress->hide();

    m_results = new QTableWidget(0, 7, this);
    m_results->setHorizontalHeaderLabels(
        {tr("Byte Offset"), tr("Bit Length"), tr("Endian"),
         tr("Min"), tr("Max"), tr("Mean"), tr("Coherence")});
    m_results->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_results->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_results->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_results->verticalHeader()->hide();
    m_results->verticalHeader()->setDefaultSectionSize(22);
    m_results->setAlternatingRowColors(true);
    m_results->setSortingEnabled(false);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    layout->addLayout(topRow);
    layout->addWidget(m_progress);
    layout->addWidget(m_results, 1);

    connect(m_scanBtn, &QPushButton::clicked, this, &RangeStatePanel::onScan);
}

void RangeStatePanel::onFrameReceived(const socketspy::core::CanFrame& frame) {
    const uint32_t id = frame.id;
    const int dlc = static_cast<int>(frame.dlc);
    if (dlc <= 0) return;

    QByteArray payload(reinterpret_cast<const char*>(frame.data), dlc);

    auto& buf = m_frames[id];
    buf.append(payload);
    if (buf.size() > kMaxPayloads)
        buf.removeFirst();

    // Add to combo if new ID
    const QString idStr = QString("0x%1").arg(id, 3, 16, QChar('0')).toUpper();
    if (m_idCombo->findText(idStr) < 0)
        m_idCombo->addItem(idStr, id);
}

void RangeStatePanel::onScan() {
    if (m_idCombo->count() == 0) return;

    const uint32_t selectedId = m_idCombo->currentData().toUInt();
    if (!m_frames.contains(selectedId)) return;

    const QVector<QByteArray> frames = m_frames.value(selectedId);
    if (frames.isEmpty()) return;

    m_scanBtn->setEnabled(false);
    m_progress->setValue(0);
    m_progress->show();
    m_results->setRowCount(0);

    auto* thread = new QThread(this);
    auto* worker = new RangeStateScanWorker(frames);
    worker->moveToThread(thread);

    connect(thread, &QThread::started,  worker, &RangeStateScanWorker::run);
    connect(worker, &RangeStateScanWorker::progress, this, &RangeStatePanel::onScanProgress);
    connect(worker, &RangeStateScanWorker::finished, this, &RangeStatePanel::onScanFinished);
    connect(worker, &RangeStateScanWorker::finished, thread, &QThread::quit);
    connect(thread, &QThread::finished, worker,      &QObject::deleteLater);
    connect(thread, &QThread::finished, thread,      &QObject::deleteLater);

    thread->start();
}

void RangeStatePanel::onScanProgress(int value) {
    m_progress->setValue(value);
}

void RangeStatePanel::onScanFinished(QVector<socketspy::gui::ScanResult> results) {
    m_progress->hide();
    m_scanBtn->setEnabled(true);
    populateResults(results);
}

void RangeStatePanel::populateResults(const QVector<ScanResult>& results) {
    m_results->setRowCount(0);
    m_results->setRowCount(results.size());

    auto makeItem = [](const QString& text, Qt::Alignment align = Qt::AlignCenter) {
        auto* it = new QTableWidgetItem(text);
        it->setTextAlignment(align);
        return it;
    };

    for (int row = 0; row < results.size(); ++row) {
        const auto& r = results[row];
        m_results->setItem(row, 0, makeItem(QString::number(r.byteOffset)));
        m_results->setItem(row, 1, makeItem(QString::number(r.bitLength)));
        m_results->setItem(row, 2, makeItem(r.bigEndian ? tr("BE") : tr("LE")));
        m_results->setItem(row, 3, makeItem(QString::number(r.minVal, 'f', 1)));
        m_results->setItem(row, 4, makeItem(QString::number(r.maxVal, 'f', 1)));
        m_results->setItem(row, 5, makeItem(QString::number(r.mean, 'f', 2)));
        m_results->setItem(row, 6, makeItem(QString::number(r.coherence, 'f', 3)));
    }
}

} // namespace socketspy::gui
