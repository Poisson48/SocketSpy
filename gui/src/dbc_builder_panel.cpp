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

// ─── BitGridWidget ────────────────────────────────────────────────────────────

BitGridWidget::BitGridWidget(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(200, 180);
}

int BitGridWidget::cellPx() const {
    int availW = width()  - kLeftPad;
    int availH = height() - kTopPad;
    int szW = (availW - 7 * kGap) / 8;
    int szH = (availH - 7 * kGap) / 8;
    return std::max(18, std::min(szW, szH));
}

QSize BitGridWidget::sizeHint() const {
    return { kLeftPad + 8 * (22 + kGap), kTopPad + 8 * (22 + kGap) };
}

QSize BitGridWidget::minimumSizeHint() const { return { 200, 180 }; }

void BitGridWidget::setData(const uint8_t* data, int dlc) {
    m_dlc = dlc;
    std::memset(m_data, 0, sizeof(m_data));
    if (data && dlc > 0)
        std::memcpy(m_data, data, static_cast<size_t>(std::min(dlc, 8)));
    update();
}

void BitGridWidget::setSignals(const std::vector<socketspy::dbc::Signal>& sigs) {
    m_signals = sigs;
    update();
}

void BitGridWidget::clearSelection() {
    m_dragStart = m_dragEnd = -1;
    m_dragging = false;
    update();
}

void BitGridWidget::setSelection(int startBit, int length) {
    if (length <= 0) {
        m_dragStart = m_dragEnd = -1;
    } else {
        m_dragStart = startBit;
        m_dragEnd   = startBit + length - 1;
    }
    update();
}

// Convert row/col to Intel flat bit index: row*8 + (7-col)
// row = byte index (0..7), col = bit within byte (0=LSB right, 7=MSB left)
static int toFlatBit(int row, int col) { return row * 8 + (7 - col); }

QRect BitGridWidget::cellRect(int bitIdx) const {
    const int cp = cellPx();
    int row = bitIdx / 8;
    int col = 7 - (bitIdx % 8);
    int x = kLeftPad + col * (cp + kGap);
    int y = kTopPad  + row * (cp + kGap);
    return { x, y, cp, cp };
}

int BitGridWidget::bitAt(QPoint pos) const {
    const int cp = cellPx();
    int col = (pos.x() - kLeftPad) / (cp + kGap);
    int row = (pos.y() - kTopPad)  / (cp + kGap);
    if (col < 0 || col > 7 || row < 0 || row > 7) return -1;
    // Verify click is inside the cell (not in the gap)
    int cx = kLeftPad + col * (cp + kGap);
    int cy = kTopPad  + row * (cp + kGap);
    if (pos.x() < cx || pos.x() >= cx + cp) return -1;
    if (pos.y() < cy || pos.y() >= cy + cp) return -1;
    return toFlatBit(row, col);
}

// Build the set of flat bit indices belonging to a Motorola BE signal
static std::vector<int> motorolaBits(const socketspy::dbc::Signal& sig) {
    std::vector<int> bits;
    bits.reserve(sig.bit_length);
    uint32_t bit_pos = sig.start_bit;
    for (uint32_t i = 0; i < sig.bit_length; ++i) {
        bits.push_back(static_cast<int>(bit_pos));
        uint32_t byte_idx = bit_pos / 8u;
        uint32_t bit_idx  = bit_pos % 8u;
        if (bit_idx == 0) bit_pos = (byte_idx + 1u) * 8u + 7u;
        else --bit_pos;
    }
    return bits;
}

void BitGridWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    const int cp = cellPx();

    // Background
    p.fillRect(rect(), QColor("#1e1e2e"));

    // Build signal coverage map: flat bit index → signal color index
    std::unordered_map<int, int> bitToSig;
    for (int si = 0; si < static_cast<int>(m_signals.size()); ++si) {
        const auto& sig = m_signals[si];
        int colorIdx = si % Palette::kNumSigColors;
        if (sig.byte_order == socketspy::dbc::ByteOrder::LittleEndian) {
            for (uint32_t b = 0; b < sig.bit_length; ++b)
                bitToSig[static_cast<int>(sig.start_bit + b)] = colorIdx;
        } else {
            for (int b : motorolaBits(sig))
                bitToSig[b] = colorIdx;
        }
    }

    // Determine selection range
    int selMin = -1, selMax = -1;
    if (m_dragStart >= 0 && m_dragEnd >= 0) {
        selMin = std::min(m_dragStart, m_dragEnd);
        selMax = std::max(m_dragStart, m_dragEnd);
    }

    QFont font = p.font();
    font.setPixelSize(10);
    p.setFont(font);

    // Column headers (7..0)
    p.setPen(QColor("#9ca3af"));
    for (int col = 0; col < 8; ++col) {
        int x = kLeftPad + col * (cp + kGap);
        p.drawText(QRect(x, 0, cp, kTopPad - 2),
                   Qt::AlignHCenter | Qt::AlignVCenter,
                   QString::number(7 - col));
    }

    // Row labels (B0..B7) and cells
    for (int row = 0; row < 8; ++row) {
        int y = kTopPad + row * (cp + kGap);
        p.setPen(QColor("#9ca3af"));
        p.drawText(QRect(0, y, kLeftPad - 4, cp),
                   Qt::AlignRight | Qt::AlignVCenter,
                   QString("B%1").arg(row));

        for (int col = 0; col < 8; ++col) {
            int flatBit = toFlatBit(row, col);
            QRect r = cellRect(flatBit);

            // Determine cell fill color
            QColor fill;
            bool inSel = (selMin >= 0 && flatBit >= selMin && flatBit <= selMax);
            if (inSel) {
                fill = QColor(100, 120, 220, 180);
            } else if (bitToSig.count(flatBit)) {
                fill = Palette::kSigColors[bitToSig[flatBit]];
                fill.setAlpha(200);
            } else if (m_dlc > 0 && row < m_dlc) {
                int byteVal = m_data[row];
                int bitVal  = (byteVal >> (7 - col)) & 1;
                fill = bitVal ? QColor("#374151") : QColor("#111827");
            } else {
                fill = QColor("#1e2030");
            }

            p.fillRect(r, fill);

            // Bit value text
            if (m_dlc > 0 && row < m_dlc) {
                int bitVal = (m_data[row] >> (7 - col)) & 1;
                p.setPen(inSel ? Qt::white : (bitToSig.count(flatBit) ? Qt::white : QColor("#9ca3af")));
                p.drawText(r, Qt::AlignCenter, QString::number(bitVal));
            }
        }
    }
}

void BitGridWidget::mousePressEvent(QMouseEvent* ev) {
    if (ev->button() != Qt::LeftButton) return;
    int b = bitAt(ev->pos());
    if (b < 0) return;
    m_dragStart = m_dragEnd = b;
    m_dragging = true;
    update();
}

void BitGridWidget::mouseMoveEvent(QMouseEvent* ev) {
    if (!m_dragging) return;
    int b = bitAt(ev->pos());
    if (b < 0) return;
    m_dragEnd = b;
    update();
}

void BitGridWidget::mouseReleaseEvent(QMouseEvent* ev) {
    if (ev->button() != Qt::LeftButton || !m_dragging) return;
    m_dragging = false;
    int b = bitAt(ev->pos());
    if (b >= 0) m_dragEnd = b;
    emitSelection();
    update();
}

void BitGridWidget::resizeEvent(QResizeEvent* ev) {
    QWidget::resizeEvent(ev);
    update();
}

void BitGridWidget::emitSelection() {
    if (m_dragStart < 0 || m_dragEnd < 0) return;
    int startBit = std::min(m_dragStart, m_dragEnd);
    int length   = std::abs(m_dragEnd - m_dragStart) + 1;
    emit selectionChanged(startBit, length);
}

// ─── DbcBuilderPanel ─────────────────────────────────────────────────────────

DbcBuilderPanel::DbcBuilderPanel(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void DbcBuilderPanel::setupUi() {
    auto* mainSplit = new QSplitter(Qt::Horizontal, this);
    auto* mainLayout = new QHBoxLayout(this);
    // Consistent outer margin 8px, no gap around the splitter itself
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(mainSplit);

    mainSplit->addWidget(setupLeftPanel(mainSplit));
    mainSplit->setStretchFactor(0, 0);

    mainSplit->addWidget(setupCenterPanel(mainSplit));
    mainSplit->setStretchFactor(1, 1);

    mainSplit->addWidget(setupRightForm(mainSplit));
    mainSplit->setStretchFactor(2, 0);

    mainSplit->setSizes({160, 700, 260});

    setupConnections();
}

// ── Left panel: message ID list ─────────────────────────────────────────────
QWidget* DbcBuilderPanel::setupLeftPanel(QSplitter* parent) {
    auto* box    = new QGroupBox(tr("Messages"), parent);
    auto* layout = new QVBoxLayout(box);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);
    m_idList = new QListWidget(box);
    layout->addWidget(m_idList);
    return box;
}

// ── Center panel: bit grid + signal table ───────────────────────────────────
QWidget* DbcBuilderPanel::setupCenterPanel(QSplitter* parent) {
    auto* centerSplit = new QSplitter(Qt::Vertical, parent);

    // Bit grid sub-widget
    auto* gridContainer = new QWidget(centerSplit);
    auto* gridLayout    = new QVBoxLayout(gridContainer);
    gridLayout->setContentsMargins(6, 6, 6, 6);
    gridLayout->setSpacing(6);
    gridLayout->setAlignment(Qt::AlignHCenter | Qt::AlignTop);

    m_grid = new BitGridWidget(gridContainer);
    m_noDataLabel = new QLabel(tr("No frame received yet"), gridContainer);
    m_noDataLabel->setAlignment(Qt::AlignCenter);
    m_noDataLabel->setStyleSheet("color: #7c8fa6; font-style: italic;");

    gridLayout->addWidget(m_noDataLabel);
    gridLayout->addWidget(m_grid);
    m_grid->hide();

    centerSplit->addWidget(gridContainer);

    // Signal table sub-widget
    m_sigTable = new QTableWidget(0, 11, centerSplit);
    m_sigTable->setHorizontalHeaderLabels({
        tr("Name"), tr("Start"), tr("Len"), tr("BO"), tr("Type"),
        tr("Factor"), tr("Offset"), tr("Unit"), tr("Value"), tr(""), tr("")
    });
    m_sigTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_sigTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_sigTable->horizontalHeader()->setSectionResizeMode(9,  QHeaderView::Fixed);
    m_sigTable->horizontalHeader()->setSectionResizeMode(10, QHeaderView::Fixed);
    m_sigTable->horizontalHeader()->setStretchLastSection(false);
    m_sigTable->setColumnWidth(0, 120);
    m_sigTable->setColumnWidth(9,  46);
    m_sigTable->setColumnWidth(10, 42);
    m_sigTable->setMinimumHeight(120);
    // Ensure rows are tall enough for embedded Edit/Del buttons
    m_sigTable->verticalHeader()->setDefaultSectionSize(30);
    m_sigTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_sigTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    centerSplit->addWidget(m_sigTable);
    centerSplit->setStretchFactor(0, 3);
    centerSplit->setStretchFactor(1, 1);
    centerSplit->setSizes({280, 180});

    return centerSplit;
}

// ── Right panel: signal definition form ─────────────────────────────────────
QWidget* DbcBuilderPanel::setupRightForm(QSplitter* parent) {
    auto* box  = new QGroupBox(tr("Define a signal"), parent);
    auto* form = new QFormLayout(box);
    form->setContentsMargins(8, 8, 8, 8);
    form->setSpacing(6);

    m_msgName = new QLineEdit(box);
    form->addRow(tr("Msg name:"), m_msgName);

    m_sigName = new QLineEdit(box);
    form->addRow(tr("Sig name:"), m_sigName);

    m_startBit = new QSpinBox(box);
    m_startBit->setRange(0, 63);
    form->addRow(tr("Start bit:"), m_startBit);

    m_bitLen = new QSpinBox(box);
    m_bitLen->setRange(1, 64);
    m_bitLen->setValue(8);
    form->addRow(tr("Length:"), m_bitLen);

    m_byteOrder = new QComboBox(box);
    m_byteOrder->addItem(tr("Intel LE"), 1);
    m_byteOrder->addItem(tr("Motorola BE"), 0);
    form->addRow(tr("Byte order:"), m_byteOrder);

    m_valueType = new QComboBox(box);
    m_valueType->addItem(tr("Unsigned"), '+');
    m_valueType->addItem(tr("Signed"),   '-');
    form->addRow(tr("Value type:"), m_valueType);

    m_factor = new QDoubleSpinBox(box);
    m_factor->setRange(-1e9, 1e9); m_factor->setDecimals(6); m_factor->setValue(1.0);
    m_factor->setLocale(QLocale::c());
    form->addRow(tr("Factor:"), m_factor);

    m_offset = new QDoubleSpinBox(box);
    m_offset->setRange(-1e9, 1e9); m_offset->setDecimals(6); m_offset->setValue(0.0);
    m_offset->setLocale(QLocale::c());
    form->addRow(tr("Offset:"), m_offset);

    m_minVal = new QDoubleSpinBox(box);
    m_minVal->setRange(-1e9, 1e9); m_minVal->setDecimals(6); m_minVal->setValue(0.0);
    m_minVal->setLocale(QLocale::c());
    form->addRow(tr("Min:"), m_minVal);

    m_maxVal = new QDoubleSpinBox(box);
    m_maxVal->setRange(-1e9, 1e9); m_maxVal->setDecimals(6); m_maxVal->setValue(0.0);
    m_maxVal->setLocale(QLocale::c());
    form->addRow(tr("Max:"), m_maxVal);

    m_unit = new QLineEdit(box);
    form->addRow(tr("Unit:"), m_unit);

    m_addBtn = new QPushButton(tr("Add signal"), box);
    form->addRow(m_addBtn);

    m_cancelEditBtn = new QPushButton(tr("Cancel"), box);
    m_cancelEditBtn->hide();
    form->addRow(m_cancelEditBtn);

    m_previewLabel = new QLabel(tr("Aperçu : –"), box);
    m_previewLabel->setStyleSheet(QString("color: %1; font-weight: 600; padding: 4px 0;").arg(Palette::kLiveGreen));
    m_previewLabel->setWordWrap(true);
    m_previewLabel->setMinimumWidth(160);
    form->addRow(m_previewLabel);

    return box;
}

// ── Signal connections ───────────────────────────────────────────────────────
void DbcBuilderPanel::setupConnections() {
    connect(m_idList,        &QListWidget::currentRowChanged, this, &DbcBuilderPanel::onIdSelected);
    connect(m_grid,          &BitGridWidget::selectionChanged, this, &DbcBuilderPanel::onBitsSelected);
    connect(m_addBtn,        &QPushButton::clicked,            this, &DbcBuilderPanel::onAddSignal);
    connect(m_cancelEditBtn, &QPushButton::clicked,            this, &DbcBuilderPanel::cancelEdit);
    connect(m_msgName,       &QLineEdit::textEdited,           this, &DbcBuilderPanel::onMsgNameChanged);

    // Live grid update when start/length spinboxes change
    connect(m_startBit, qOverload<int>(&QSpinBox::valueChanged),
            this, &DbcBuilderPanel::onFormBitsChanged);
    connect(m_bitLen, qOverload<int>(&QSpinBox::valueChanged),
            this, &DbcBuilderPanel::onFormBitsChanged);

    // Live preview update when any signal parameter changes
    connect(m_byteOrder,  qOverload<int>(&QComboBox::currentIndexChanged),
            this, &DbcBuilderPanel::updatePreview);
    connect(m_valueType,  qOverload<int>(&QComboBox::currentIndexChanged),
            this, &DbcBuilderPanel::updatePreview);
    connect(m_factor,     qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &DbcBuilderPanel::updatePreview);
    connect(m_offset,     qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &DbcBuilderPanel::updatePreview);
    connect(m_unit,       &QLineEdit::textEdited,
            this, &DbcBuilderPanel::updatePreview);
}

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
        m_previewLabel->setText(tr("Aperçu : (aucun message sélectionné)"));
        return;
    }
    auto it = m_lastFrames.find(m_selectedId);
    if (it == m_lastFrames.end()) {
        m_previewLabel->setText(tr("Aperçu : (aucune trame reçue)"));
        return;
    }

    int start = m_startBit->value();
    int len   = m_bitLen->value();
    if (len <= 0 || start + len > 64) {
        m_previewLabel->setText(tr("Aperçu : (plage invalide)"));
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
        m_previewLabel->setText(tr("Aperçu : (extraction échouée)"));
        return;
    }

    QString unit = m_unit->text().trimmed();
    QString txt  = QString("Aperçu :  %1").arg(*val, 0, 'g', 7);
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
