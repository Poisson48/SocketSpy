// dbc_builder_panel_signals.cpp — logique signaux/messages du DbcBuilderPanel
// (réception trames, édition de signaux, aperçu DBC). Extrait de dbc_builder_panel.cpp.
#include "dbc_builder_panel.h"

#include <QPainter>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QSplitter>
#include <QGroupBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QStackedWidget>
#include <QMessageBox>
#include <QTimer>
#include <cstring>
#include <span>

// Permanently undef Qt's `signals` macro so we can access dbc::Message::signals.
// Safe in .cpp files — no `signals:` access specifier is used here.
#include "dbc_compat.h"
#include "gui_palette.h"

namespace socketspy::gui {

void DbcBuilderPanel::onFrameReceived(socketspy::core::CanFrame frame) {
    LastFrame& lf = m_lastFrames[frame.id];
    lf.dlc = frame.dlc;
    std::memcpy(lf.data, frame.data, static_cast<size_t>(std::min((int)frame.dlc, 8)));

    // Add to list if new
    bool found = false;
    for (int i = 0; i < m_idList->count(); ++i) {
        bool ok;
        uint32_t id = m_idList->item(i)->text().toUInt(&ok, 16);
        if (ok && id == frame.id) { found = true; break; }
    }
    if (!found) {
        m_idList->addItem(QString("0x%1").arg(frame.id, 8, 16, QChar('0')).toUpper());
    }

    if (frame.id == m_selectedId) {
        refreshGrid();
        updatePreview();
        // Refresh signal decoded values at most every 200ms
        if (!m_tableRefreshPending) {
            m_tableRefreshPending = true;
            QTimer::singleShot(200, this, [this]{
                m_tableRefreshPending = false;
                refreshSignalTable();
            });
        }
    }
}

void DbcBuilderPanel::onIdSelected(int row) {
    if (row < 0 || row >= m_idList->count()) return;
    bool ok;
    uint32_t id = m_idList->item(row)->text().toUInt(&ok, 16);
    if (!ok) return;
    m_selectedId = id;
    cancelEdit();

    // Update message name field: use existing name from DB, or show the auto-generated default
    bool found = false;
    for (auto& msg : m_db.messages) {
        if (msg.id == id) {
            m_msgName->setText(QString::fromStdString(msg.name));
            found = true;
            break;
        }
    }
    if (!found) {
        // Show the default name that will be assigned when signals are added
        m_msgName->setText(QString("MSG_%1").arg(id, 4, 16, QChar('0')).toUpper());
    }

    refreshGrid();
    refreshSignalTable();
    updatePreview();
}

void DbcBuilderPanel::refreshGrid() {
    if (m_lastFrames.count(m_selectedId)) {
        const auto& lf = m_lastFrames[m_selectedId];
        m_grid->setData(lf.data, lf.dlc);
        m_noDataLabel->hide();
        m_grid->show();
    } else {
        m_noDataLabel->show();
        m_grid->hide();
    }

    // Find signals for this message
    std::vector<socketspy::dbc::Signal> sigs;
    for (const auto& msg : m_db.messages) {
        if (msg.id == m_selectedId) {
            sigs = msg.signals;
            break;
        }
    }
    m_grid->setSignals(sigs);
}

void DbcBuilderPanel::refreshSignalTable() {
    m_sigTable->setRowCount(0);

    const socketspy::dbc::Message* msg = nullptr;
    for (const auto& m : m_db.messages) {
        if (m.id == m_selectedId) { msg = &m; break; }
    }
    if (!msg) return;

    for (int i = 0; i < static_cast<int>(msg->signals.size()); ++i) {
        const auto& sig = msg->signals[i];
        m_sigTable->insertRow(i);

        auto set = [&](int col, const QString& txt) {
            m_sigTable->setItem(i, col, new QTableWidgetItem(txt));
        };

        set(0, QString::fromStdString(sig.name));
        set(1, QString::number(sig.start_bit));
        set(2, QString::number(sig.bit_length));
        set(3, sig.byte_order == socketspy::dbc::ByteOrder::LittleEndian ? "LE" : "BE");
        set(4, sig.value_type == socketspy::dbc::ValueType::Unsigned ? "U" : "S");
        set(5, QString::number(sig.factor));
        set(6, QString::number(sig.offset));
        set(7, QString::fromStdString(sig.unit));

        // Column 8: decoded value from last received frame
        QString valStr = "-";
        auto it = m_lastFrames.find(m_selectedId);
        if (it != m_lastFrames.end()) {
            std::span<const uint8_t> data(it->second.data, static_cast<size_t>(it->second.dlc));
            auto decoded = socketspy::dbc::extract_signal(sig, data);
            if (decoded) {
                valStr = QString::number(*decoded, 'g', 6);
                if (!sig.unit.empty())
                    valStr += " " + QString::fromStdString(sig.unit);
            }
        }
        auto* valItem = new QTableWidgetItem(valStr);
        valItem->setForeground(QColor(Palette::kLiveGreen));
        m_sigTable->setItem(i, 8, valItem);

        const int idx = i;

        // Edit button — wrapped in a centered container so it fills the cell cleanly
        auto* editBtn = new QPushButton("Edit");
        editBtn->setObjectName("editSigBtn");
        connect(editBtn, &QPushButton::clicked, this, [this, idx]() { onEditSignal(idx); });
        auto* editContainer = new QWidget();
        editContainer->setObjectName("sigBtnContainer");
        auto* editLayout = new QHBoxLayout(editContainer);
        editLayout->setContentsMargins(2, 2, 2, 2);
        editLayout->setSpacing(0);
        editLayout->addWidget(editBtn);
        m_sigTable->setCellWidget(i, 9, editContainer);

        // Del button — same container pattern
        auto* delBtn = new QPushButton("Del");
        delBtn->setObjectName("delSigBtn");
        connect(delBtn, &QPushButton::clicked, this, [this, idx]() { onDeleteSignal(idx); });
        auto* delContainer = new QWidget();
        delContainer->setObjectName("sigBtnContainer");
        auto* delLayout = new QHBoxLayout(delContainer);
        delLayout->setContentsMargins(2, 2, 2, 2);
        delLayout->setSpacing(0);
        delLayout->addWidget(delBtn);
        m_sigTable->setCellWidget(i, 10, delContainer);

        const int rowH = qMax(editBtn->sizeHint().height(), 24) + 6;
        m_sigTable->setRowHeight(i, rowH);
    }
}

void DbcBuilderPanel::onBitsSelected(int startBit, int length) {
    // Block form spinbox signals to avoid re-triggering setSelection while we fill them
    m_startBit->blockSignals(true);
    m_bitLen->blockSignals(true);
    m_startBit->setValue(startBit);
    m_bitLen->setValue(length);
    m_startBit->blockSignals(false);
    m_bitLen->blockSignals(false);

    m_grid->setSelection(startBit, length);
    updatePreview();
}

void DbcBuilderPanel::onFormBitsChanged() {
    int start = m_startBit->value();
    int len   = m_bitLen->value();
    if (start >= 0 && len > 0 && start + len <= 64)
        m_grid->setSelection(start, len);
    updatePreview();
}

socketspy::dbc::Signal DbcBuilderPanel::buildSignalFromForm(const QString& name) const {
    socketspy::dbc::Signal sig;
    sig.name       = name.toStdString();
    sig.start_bit  = static_cast<uint8_t>(m_startBit->value());
    sig.bit_length = static_cast<uint8_t>(m_bitLen->value());
    sig.byte_order = m_byteOrder->currentData().toInt() == 1
                     ? socketspy::dbc::ByteOrder::LittleEndian
                     : socketspy::dbc::ByteOrder::BigEndian;
    sig.value_type = m_valueType->currentData().toInt() == '+'
                     ? socketspy::dbc::ValueType::Unsigned
                     : socketspy::dbc::ValueType::Signed;
    sig.factor  = m_factor->value();
    sig.offset  = m_offset->value();
    sig.min_val = m_minVal->value();
    sig.max_val = m_maxVal->value();
    sig.unit    = m_unit->text().trimmed().toStdString();
    return sig;
}

void DbcBuilderPanel::onAddSignal() {
    if (m_selectedId == 0xFFFFFFFF) {
        QMessageBox::warning(this, tr("No message"), tr("Select a message ID first."));
        return;
    }
    const QString name = m_sigName->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("Empty name"), tr("Signal name cannot be empty."));
        return;
    }
    int start = m_startBit->value();
    int len   = m_bitLen->value();
    if (start + len > 64) {
        QMessageBox::warning(this, tr("Out of range"),
            tr("start_bit + length exceeds 64 bits."));
        return;
    }

    // ── Update existing signal (edit mode) ────────────────────────────────────
    if (m_editingIdx >= 0) {
        auto* msg = findMessage(m_selectedId);
        if (msg && m_editingIdx < static_cast<int>(msg->signals.size())) {
            // Duplicate name check: skip the signal being edited
            for (int i = 0; i < static_cast<int>(msg->signals.size()); ++i) {
                if (i == m_editingIdx) continue;
                if (msg->signals[i].name == name.toStdString()) {
                    QMessageBox::warning(this, tr("Duplicate"), tr("Signal name already exists."));
                    return;
                }
            }
            msg->signals[m_editingIdx] = buildSignalFromForm(name);

            m_editingIdx = -1;
            m_addBtn->setText(tr("Add signal"));
            m_cancelEditBtn->hide();
            refreshSignalTable();
            refreshGrid();
            emit dbcUpdated(m_db);
        }
        return;
    }

    // ── Add new signal ────────────────────────────────────────────────────────
    auto* msg = findOrCreateMessage(m_selectedId);

    // Check for duplicate name
    for (const auto& s : msg->signals) {
        if (s.name == name.toStdString()) {
            QMessageBox::warning(this, tr("Duplicate"), tr("Signal name already exists."));
            return;
        }
    }

    msg->signals.push_back(buildSignalFromForm(name));

    refreshSignalTable();
    refreshGrid();
    emit dbcUpdated(m_db);
    m_sigName->clear();
    m_grid->clearSelection();
}

void DbcBuilderPanel::onEditSignal(int sigIdx) {
    auto* msg = findMessage(m_selectedId);
    if (!msg || sigIdx < 0 || sigIdx >= static_cast<int>(msg->signals.size())) return;

    const auto& sig = msg->signals[sigIdx];

    // Block spinbox signals to avoid triggering updatePreview() mid-load
    m_startBit->blockSignals(true);
    m_bitLen->blockSignals(true);
    m_factor->blockSignals(true);
    m_offset->blockSignals(true);
    m_minVal->blockSignals(true);
    m_maxVal->blockSignals(true);

    m_sigName->setText(QString::fromStdString(sig.name));
    m_startBit->setValue(static_cast<int>(sig.start_bit));
    m_bitLen->setValue(static_cast<int>(sig.bit_length));
    m_byteOrder->setCurrentIndex(
        sig.byte_order == socketspy::dbc::ByteOrder::LittleEndian ? 0 : 1);
    m_valueType->setCurrentIndex(
        sig.value_type == socketspy::dbc::ValueType::Unsigned ? 0 : 1);
    m_factor->setValue(sig.factor);
    m_offset->setValue(sig.offset);
    m_minVal->setValue(sig.min_val);
    m_maxVal->setValue(sig.max_val);
    m_unit->setText(QString::fromStdString(sig.unit));

    m_startBit->blockSignals(false);
    m_bitLen->blockSignals(false);
    m_factor->blockSignals(false);
    m_offset->blockSignals(false);
    m_minVal->blockSignals(false);
    m_maxVal->blockSignals(false);

    // Highlight bits on the grid
    m_grid->setSelection(static_cast<int>(sig.start_bit),
                         static_cast<int>(sig.bit_length));

    m_editingIdx = sigIdx;
    m_addBtn->setText(tr("Update signal"));
    m_cancelEditBtn->show();

    updatePreview();
}

void DbcBuilderPanel::cancelEdit() {
    m_editingIdx = -1;
    m_addBtn->setText(tr("Add signal"));
    if (m_cancelEditBtn) m_cancelEditBtn->hide();
    m_grid->clearSelection();
}

socketspy::dbc::Message* DbcBuilderPanel::findMessage(uint32_t id) {
    for (auto& msg : m_db.messages) {
        if (msg.id == id) return &msg;
    }
    return nullptr;
}

void DbcBuilderPanel::onDeleteSignal(int sigIdx) {
    socketspy::dbc::Message* msg = nullptr;
    for (auto& m : m_db.messages) {
        if (m.id == m_selectedId) { msg = &m; break; }
    }
    if (!msg || sigIdx < 0 || sigIdx >= static_cast<int>(msg->signals.size())) return;
    msg->signals.erase(msg->signals.begin() + sigIdx);
    refreshSignalTable();
    refreshGrid();
    emit dbcUpdated(m_db);
}

void DbcBuilderPanel::onMsgNameChanged(const QString& text) {
    for (auto& msg : m_db.messages) {
        if (msg.id == m_selectedId) {
            msg.name = text.toStdString();
            return;
        }
    }
}

socketspy::dbc::Message* DbcBuilderPanel::findOrCreateMessage(uint32_t id) {
    for (auto& msg : m_db.messages) {
        if (msg.id == id) return &msg;
    }
    socketspy::dbc::Message msg;
    msg.id   = id;
    msg.dlc  = 8;
    // Use the name the user typed in m_msgName, or auto-generate one
    QString name = m_msgName ? m_msgName->text().trimmed() : QString();
    if (name.isEmpty())
        name = QString("MSG_%1").arg(id, 4, 16, QChar('0')).toUpper();
    msg.name = name.toStdString();
    m_db.messages.push_back(std::move(msg));
    return &m_db.messages.back();
}

void DbcBuilderPanel::updatePreview() {
    if (!m_previewLabel) return;

    if (m_selectedId == 0xFFFFFFFF) {
        m_previewLabel->setText(tr("Preview: (no message selected)"));
        return;
    }
    auto it = m_lastFrames.find(m_selectedId);
    if (it == m_lastFrames.end()) {
        m_previewLabel->setText(tr("Preview: (no frame received)"));
        return;
    }

    int start = m_startBit->value();
    int len   = m_bitLen->value();
    if (len <= 0 || start + len > 64) {
        m_previewLabel->setText(tr("Preview: (invalid range)"));
        return;
    }

    socketspy::dbc::Signal sig;
    sig.start_bit  = static_cast<uint8_t>(start);
    sig.bit_length = static_cast<uint8_t>(len);
    sig.byte_order = (m_byteOrder->currentIndex() == 0)
                     ? socketspy::dbc::ByteOrder::LittleEndian
                     : socketspy::dbc::ByteOrder::BigEndian;
    sig.value_type = (m_valueType->currentIndex() == 0)
                     ? socketspy::dbc::ValueType::Unsigned
                     : socketspy::dbc::ValueType::Signed;
    sig.factor = m_factor->value();
    sig.offset = m_offset->value();

    const auto& frame = it->second;
    std::span<const uint8_t> data(frame.data, static_cast<size_t>(frame.dlc));
    auto val = socketspy::dbc::extract_signal(sig, data);

    if (!val) {
        m_previewLabel->setText(tr("Preview: (extraction failed)"));
        return;
    }

    QString unit = m_unit->text().trimmed();
    QString txt  = QString("Preview:  %1").arg(*val, 0, 'g', 7);
    if (!unit.isEmpty()) txt += " " + unit;
    m_previewLabel->setText(txt);
}

void DbcBuilderPanel::loadDbc(const socketspy::dbc::DbcDatabase& db) {
    m_db = db;
    m_idList->clear();

    // Add IDs from DB
    for (const auto& msg : m_db.messages) {
        m_idList->addItem(QString("0x%1").arg(msg.id, 8, 16, QChar('0')).toUpper());
    }

    // Add IDs from live frames not in DB
    for (const auto& [id, lf] : m_lastFrames) {
        bool found = false;
        for (const auto& msg : m_db.messages) {
            if (msg.id == id) { found = true; break; }
        }
        if (!found) {
            m_idList->addItem(QString("0x%1").arg(id, 8, 16, QChar('0')).toUpper());
        }
    }

    // Re-select current ID if still present
    m_selectedId = 0xFFFFFFFF;
    refreshGrid();
    refreshSignalTable();
}

} // namespace socketspy::gui
