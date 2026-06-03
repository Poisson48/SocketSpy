// monitor_panel_actions.cpp — filtres, menu contextuel, recherche, export CSV
// et décodage du MonitorPanel. Extrait de monitor_panel.cpp.
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
