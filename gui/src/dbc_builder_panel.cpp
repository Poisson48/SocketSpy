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

// ─── DbcBuilderPanel (UI) ────────────────────────────────────────────────────

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

    m_previewLabel = new QLabel(tr("Preview: –"), box);
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


} // namespace socketspy::gui
