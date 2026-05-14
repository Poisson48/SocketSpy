// dbc_helper.h must be included before any Qt headers to avoid the
// `signals` macro collision with socketspy::dbc::Message::signals.
#include "dbc_helper.h"
#include "monitor_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QMenu>
#include <QString>
#include <QLabel>

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
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_table, &QTableWidget::customContextMenuRequested,
            this,    &MonitorPanel::onContextMenu);

    m_clear = new QPushButton("Clear", this);
    m_pause = new QCheckBox("Pause", this);
    connect(m_clear, &QPushButton::clicked, this, &MonitorPanel::onClear);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText("Filter by ID (hex)…");
    m_search->setMaximumWidth(160);
    connect(m_search, &QLineEdit::textChanged, this, &MonitorPanel::onSearchChanged);

    auto* toolbar = new QHBoxLayout;
    toolbar->addWidget(m_clear);
    toolbar->addWidget(m_pause);
    toolbar->addWidget(new QLabel("ID:", this));
    toolbar->addWidget(m_search);
    toolbar->addStretch();

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(toolbar);
    layout->addWidget(m_table);
}

void MonitorPanel::onFrameReceived(CanFrame frame) {
    if (m_pause->isChecked()) return;
    if (!m_filter.accepts(frame)) return;

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

void MonitorPanel::onFilterChanged(const FrameFilter& filter) {
    m_filter = filter;
}

void MonitorPanel::onDbcLoaded(DbcDatabase db) {
    *m_dbc      = std::move(db);
    m_dbcLoaded = true;
}

void MonitorPanel::onClear() {
    m_table->setRowCount(0);
    m_search->clear();
}

void MonitorPanel::onSearchChanged(const QString& text) {
    const QString filter = text.trimmed().toLower();
    for (int row = 0; row < m_table->rowCount(); ++row) {
        auto* item = m_table->item(row, 1); // colonne ID
        bool match = filter.isEmpty() || (item && item->text().toLower().contains(filter));
        m_table->setRowHidden(row, !match);
    }
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

void MonitorPanel::onContextMenu(const QPoint& pos) {
    const QModelIndex idx = m_table->indexAt(pos);
    if (!idx.isValid()) return;
    auto* idItem = m_table->item(idx.row(), 1);
    if (!idItem) return;
    bool ok = false;
    uint32_t id = idItem->text().toUInt(&ok, 16);
    if (!ok) return;

    QMenu menu(this);
    auto* graphAct = menu.addAction(
        QString("Ajouter 0x%1 au graphe").arg(id, 0, 16).toUpper());
    if (menu.exec(m_table->viewport()->mapToGlobal(pos)) == graphAct)
        emit frameGraphRequested(id);
}

QString MonitorPanel::decodeFrame(const CanFrame& f) const {
    if (!m_dbcLoaded) return {};
    std::span<const uint8_t> data(f.data, f.dlc);
    return QString::fromStdString(
        dbc_helper::decode_frame(*m_dbc, f.id, data));
}

} // namespace socketspy::gui
