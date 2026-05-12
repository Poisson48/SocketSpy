// dbc_helper.h must be included before any Qt headers to avoid the
// `signals` macro collision with socketspy::dbc::Message::signals.
#include "dbc_helper.h"
#include "monitor_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QString>

using namespace socketspy::core;
using namespace socketspy::dbc;

namespace socketspy::gui {

MonitorPanel::MonitorPanel(QWidget* parent) : QWidget(parent) {
    m_dbc = new DbcDatabase{};
    setupUi();
}

MonitorPanel::~MonitorPanel() {
    delete m_dbc;
}

void MonitorPanel::setupUi() {
    m_table = new QTableWidget(0, 5, this);
    m_table->setHorizontalHeaderLabels(
        {"Timestamp (µs)", "ID (hex)", "DLC", "Data (hex)", "Decoded"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->hide();
    m_table->setAlternatingRowColors(true);

    connect(m_table, &QTableWidget::cellDoubleClicked,
            this,    &MonitorPanel::onCellDoubleClicked);

    m_clear = new QPushButton("Clear", this);
    m_pause = new QCheckBox("Pause", this);
    connect(m_clear, &QPushButton::clicked, this, &MonitorPanel::onClear);

    auto* toolbar = new QHBoxLayout;
    toolbar->addWidget(m_clear);
    toolbar->addWidget(m_pause);
    toolbar->addStretch();

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(toolbar);
    layout->addWidget(m_table);
}

void MonitorPanel::onFrameReceived(CanFrame frame) {
    if (m_pause->isChecked()) return;

    if (m_table->rowCount() >= kMaxRows)
        m_table->removeRow(0);

    int row = m_table->rowCount();
    m_table->insertRow(row);

    m_table->setItem(row, 0,
        new QTableWidgetItem(QString::number(frame.timestamp_us)));
    m_table->setItem(row, 1,
        new QTableWidgetItem(
            QString("%1").arg(frame.id, 8, 16, QChar('0')).toUpper()));
    m_table->setItem(row, 2,
        new QTableWidgetItem(QString::number(frame.dlc)));

    QString hexData;
    for (int i = 0; i < frame.dlc; ++i) {
        if (i) hexData += ' ';
        hexData += QString("%1").arg(frame.data[i], 2, 16, QChar('0')).toUpper();
    }
    m_table->setItem(row, 3, new QTableWidgetItem(hexData));
    m_table->setItem(row, 4, new QTableWidgetItem(decodeFrame(frame)));
    m_table->scrollToBottom();
}

void MonitorPanel::onDbcLoaded(DbcDatabase db) {
    *m_dbc      = std::move(db);
    m_dbcLoaded = true;
}

void MonitorPanel::onClear() {
    m_table->setRowCount(0);
}

void MonitorPanel::onCellDoubleClicked(int row, int /*col*/) {
    if (!m_dbcLoaded) return;
    bool ok = false;
    uint32_t id = m_table->item(row, 1)->text().toUInt(&ok, 16);
    if (!ok) return;
    std::string name = dbc_helper::first_signal_name(*m_dbc, id);
    if (!name.empty())
        emit signalDoubleClicked(QString::fromStdString(name), id);
}

QString MonitorPanel::decodeFrame(const CanFrame& f) const {
    if (!m_dbcLoaded) return {};
    std::span<const uint8_t> data(f.data, f.dlc);
    return QString::fromStdString(
        dbc_helper::decode_frame(*m_dbc, f.id, data));
}

} // namespace socketspy::gui
