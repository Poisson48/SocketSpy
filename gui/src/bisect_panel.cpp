#include "bisect_panel.h"
#include "iface_detector.h"
#include "cancore.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <cerrno>
#include <cstring>

using namespace socketspy::core;

namespace socketspy::gui {

BisectPanel::BisectPanel(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void BisectPanel::setupUi() {
    // Interface selector row
    m_iface = new QComboBox(this);
    m_iface->addItems(IfaceDetector::scanCanIfaces());

    auto* refreshBtn = new QPushButton(QString::fromUtf8("↺"), this);
    refreshBtn->setFixedWidth(28);
    refreshBtn->setToolTip(tr("Refresh interface list"));
    connect(refreshBtn, &QPushButton::clicked, this, [this]() {
        const QString cur = m_iface->currentText();
        m_iface->blockSignals(true);
        m_iface->clear();
        m_iface->addItems(IfaceDetector::scanCanIfaces());
        m_iface->blockSignals(false);
        int idx = m_iface->findText(cur);
        if (idx >= 0) m_iface->setCurrentIndex(idx);
    });

    auto* ifaceRow = new QHBoxLayout;
    ifaceRow->addWidget(new QLabel(tr("Interface:"), this));
    ifaceRow->addWidget(m_iface, 1);
    ifaceRow->addWidget(refreshBtn);

    // Row 1: Load + frame count
    m_loadBtn        = new QPushButton(tr("Load Capture…"), this);
    m_frameCountLabel = new QLabel(tr("0 frames loaded"), this);
    m_frameCountLabel->setStyleSheet("color: #6b7280;");

    auto* loadRow = new QHBoxLayout;
    loadRow->addWidget(m_loadBtn);
    loadRow->addWidget(m_frameCountLabel);
    loadRow->addStretch();

    // Row 2: Window info label
    m_windowLabel = new QLabel(tr("Window: frames [0, 0] — size 0"), this);

    // Row 3: Progress bar
    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 1);
    m_progress->setValue(0);
    m_progress->setTextVisible(false);

    // Row 4: Replay buttons
    m_replayFirstBtn  = new QPushButton(tr("Replay First Half"), this);
    m_replaySecondBtn = new QPushButton(tr("Replay Second Half"), this);
    m_replayFirstBtn->setEnabled(false);
    m_replaySecondBtn->setEnabled(false);

    auto* replayRow = new QHBoxLayout;
    replayRow->addWidget(m_replayFirstBtn);
    replayRow->addWidget(m_replaySecondBtn);
    replayRow->addStretch();

    // Row 5: Event decision buttons
    m_eventFirstBtn  = new QPushButton(tr("✓ Event in First Half"), this);
    m_eventSecondBtn = new QPushButton(tr("✓ Event in Second Half"), this);
    m_eventFirstBtn->setEnabled(false);
    m_eventSecondBtn->setEnabled(false);

    auto* eventRow = new QHBoxLayout;
    eventRow->addWidget(m_eventFirstBtn);
    eventRow->addWidget(m_eventSecondBtn);
    eventRow->addStretch();

    // Row 6: Reset (full width)
    m_resetBtn = new QPushButton(tr("Reset"), this);
    m_resetBtn->setEnabled(false);

    // Row 7: Result label
    m_resultLabel = new QLabel(this);
    m_resultLabel->hide();
    m_resultLabel->setStyleSheet("font-weight: bold; color: #6366f1;");

    // Row 8: Result table
    m_table = new QTableWidget(0, 3, this);
    m_table->setHorizontalHeaderLabels({tr("ID"), tr("Timestamp"), tr("Data")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->hide();
    m_table->verticalHeader()->setDefaultSectionSize(24);
    m_table->setAlternatingRowColors(true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    layout->addLayout(ifaceRow);
    layout->addLayout(loadRow);
    layout->addWidget(m_windowLabel);
    layout->addWidget(m_progress);
    layout->addLayout(replayRow);
    layout->addLayout(eventRow);
    layout->addWidget(m_resetBtn);
    layout->addWidget(m_resultLabel);
    layout->addWidget(m_table, 1);

    connect(m_loadBtn,        &QPushButton::clicked, this, &BisectPanel::onLoadCapture);
    connect(m_replayFirstBtn, &QPushButton::clicked, this, &BisectPanel::onReplayFirstHalf);
    connect(m_replaySecondBtn,&QPushButton::clicked, this, &BisectPanel::onReplaySecondHalf);
    connect(m_eventFirstBtn,  &QPushButton::clicked, this, &BisectPanel::onEventInFirstHalf);
    connect(m_eventSecondBtn, &QPushButton::clicked, this, &BisectPanel::onEventInSecondHalf);
    connect(m_resetBtn,       &QPushButton::clicked, this, &BisectPanel::onReset);
}

bool BisectPanel::parseLogFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QTextStream ts(&f);
    m_frames.clear();
    while (!ts.atEnd()) {
        QString line = ts.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        // Format: TIMESTAMP IFACE ID#HEXDATA
        // e.g.: 1716482.709000 vcan0 0CF00400#F01C00FFFFFFFFFFF
        int hashIdx = line.lastIndexOf('#');
        if (hashIdx < 0) continue;
        QString dataHex = line.mid(hashIdx + 1).trimmed();
        QString left    = line.left(hashIdx).trimmed();

        // Split left part: "TIMESTAMP IFACE ID"
        QStringList tokens = left.split(' ', Qt::SkipEmptyParts);
        if (tokens.size() < 3) continue;

        bool tsOk = false;
        double tsVal = tokens[0].toDouble(&tsOk);

        bool idOk = false;
        uint32_t id = tokens[2].toUInt(&idOk, 16);
        if (!idOk) continue;

        CanFrame fr{};
        fr.id           = id;
        fr.timestamp_us = tsOk ? static_cast<uint64_t>(tsVal * 1e6) : 0;
        fr.dlc          = static_cast<uint8_t>(dataHex.length() / 2);
        if (fr.dlc > 64) fr.dlc = 64;
        for (int i = 0; i < fr.dlc; ++i)
            fr.data[i] = static_cast<uint8_t>(dataHex.mid(i * 2, 2).toUInt(nullptr, 16));
        m_frames.append(fr);
    }
    return !m_frames.isEmpty();
}

void BisectPanel::onLoadCapture() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Capture"), {},
        tr("CAN Log Files (*.log);;All Files (*)"));
    if (path.isEmpty()) return;
    if (!parseLogFile(path)) {
        QMessageBox::critical(this, tr("Bisect"), tr("Cannot read or empty file: %1").arg(path));
        return;
    }
    m_lo = 0;
    m_hi = m_frames.size();
    m_frameCountLabel->setText(tr("%1 frames loaded").arg(m_frames.size()));
    m_replayFirstBtn->setEnabled(true);
    m_replaySecondBtn->setEnabled(true);
    m_eventFirstBtn->setEnabled(true);
    m_eventSecondBtn->setEnabled(true);
    m_resetBtn->setEnabled(true);
    updateDisplay();
}

void BisectPanel::updateDisplay() {
    const int size = m_hi - m_lo;
    m_windowLabel->setText(
        tr("Window: frames [%1, %2] — size %3")
            .arg(m_lo).arg(m_hi).arg(size));

    // Progress bar: lo..hi within total range
    if (!m_frames.isEmpty()) {
        m_progress->setRange(0, m_frames.size());
        m_progress->setValue(m_hi - m_lo);
    }

    // Show result table when window is small enough
    if (size <= 5 && !m_frames.isEmpty()) {
        m_resultLabel->setText(
            tr("Result: %1 candidate frame(s)").arg(size));
        m_resultLabel->show();
        m_table->setRowCount(0);
        for (int i = m_lo; i < m_hi; ++i) {
            const CanFrame& fr = m_frames[i];
            int row = m_table->rowCount();
            m_table->insertRow(row);
            m_table->setItem(row, 0,
                new QTableWidgetItem(QString("%1")
                    .arg(fr.id, 8, 16, QChar('0')).toUpper()));
            m_table->setItem(row, 1,
                new QTableWidgetItem(QString::number(fr.timestamp_us)));
            QString hexData;
            for (int b = 0; b < fr.dlc; ++b) {
                if (b) hexData += ' ';
                hexData += QString("%1").arg(fr.data[b], 2, 16, QChar('0')).toUpper();
            }
            m_table->setItem(row, 2, new QTableWidgetItem(hexData));
        }
    } else {
        m_resultLabel->hide();
        m_table->setRowCount(0);
    }

    // Disable decision buttons when bisection is done
    const bool done = (size <= 1);
    m_eventFirstBtn->setEnabled(!done);
    m_eventSecondBtn->setEnabled(!done);
    m_replayFirstBtn->setEnabled(!done);
    m_replaySecondBtn->setEnabled(!done);
}

void BisectPanel::replayRange(int from, int to) {
    const QString ifaceName = m_iface->currentText();
    if (ifaceName.isEmpty()) {
        QMessageBox::warning(this, tr("Bisect"), tr("No CAN interface selected."));
        return;
    }
    IfaceHandle h = can_open(ifaceName.toStdString());
    if (!h.valid()) {
        QMessageBox::critical(this, tr("Bisect"),
            tr("can_open failed: %1").arg(strerror(errno)));
        return;
    }
    for (int i = from; i < to; ++i)
        can_send(h, m_frames[i]);
    can_close(h);
}

void BisectPanel::onReplayFirstHalf() {
    const int mid = (m_lo + m_hi) / 2;
    replayRange(m_lo, mid);
}

void BisectPanel::onReplaySecondHalf() {
    const int mid = (m_lo + m_hi) / 2;
    replayRange(mid, m_hi);
}

void BisectPanel::onEventInFirstHalf() {
    m_hi = (m_lo + m_hi) / 2;
    updateDisplay();
}

void BisectPanel::onEventInSecondHalf() {
    m_lo = (m_lo + m_hi) / 2;
    updateDisplay();
}

void BisectPanel::onReset() {
    m_lo = 0;
    m_hi = m_frames.size();
    updateDisplay();
}

} // namespace socketspy::gui
