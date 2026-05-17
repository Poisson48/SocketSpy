#include "filter_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QRegularExpressionValidator>
#include <QIntValidator>

namespace socketspy::gui {

FilterPanel::FilterPanel(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void FilterPanel::setupUi() {
    auto* hexValidator = new QRegularExpressionValidator(
        QRegularExpression("(0x)?[0-9A-Fa-f]{1,8}"), this);
    auto* dlcValidator = new QIntValidator(0, 15, this);

    m_enable = new QCheckBox(tr("Enable"), this);
    m_clear  = new QPushButton(tr("Clear"), this);

    m_idMin  = new QLineEdit("0x000", this);
    m_idMax  = new QLineEdit("0x1FFFFFFF", this);
    m_idMask = new QLineEdit("0xFFFFFFFF", this);
    m_dlcMin = new QLineEdit("0", this);
    m_dlcMax = new QLineEdit("15", this);

    m_idMin->setValidator(hexValidator);
    m_idMax->setValidator(hexValidator);
    m_idMask->setValidator(hexValidator);
    m_dlcMin->setValidator(dlcValidator);
    m_dlcMax->setValidator(dlcValidator);

    m_showStd = new QCheckBox("STD", this);
    m_showExt = new QCheckBox("EXT", this);
    m_showFd  = new QCheckBox("FD",  this);
    m_showErr = new QCheckBox("ERR", this);

    m_showStd->setChecked(true);
    m_showExt->setChecked(true);
    m_showFd->setChecked(true);
    m_showErr->setChecked(false);

    auto* topRow = new QHBoxLayout;
    topRow->addWidget(new QLabel(tr("Filter"), this));
    topRow->addStretch();
    topRow->addWidget(m_enable);
    topRow->addWidget(m_clear);

    auto* grid = new QGridLayout;
    grid->addWidget(new QLabel(tr("ID Range:"), this), 0, 0);
    grid->addWidget(m_idMin, 0, 1);
    grid->addWidget(new QLabel(tr("to"), this), 0, 2);
    grid->addWidget(m_idMax, 0, 3);
    grid->addWidget(new QLabel(tr("Mask:"), this), 0, 4);
    grid->addWidget(m_idMask, 0, 5);

    grid->addWidget(new QLabel(tr("DLC:"), this), 1, 0);
    grid->addWidget(m_dlcMin, 1, 1);
    grid->addWidget(new QLabel(tr("to"), this), 1, 2);
    grid->addWidget(m_dlcMax, 1, 3);

    auto* typesRow = new QHBoxLayout;
    typesRow->addWidget(new QLabel(tr("Types:"), this));
    typesRow->addWidget(m_showStd);
    typesRow->addWidget(m_showExt);
    typesRow->addWidget(m_showFd);
    typesRow->addWidget(m_showErr);
    typesRow->addStretch();

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(topRow);
    layout->addLayout(grid);
    layout->addLayout(typesRow);

    connect(m_enable,  &QCheckBox::toggled,         this, &FilterPanel::onAnyFieldChanged);
    connect(m_showStd, &QCheckBox::toggled,         this, &FilterPanel::onAnyFieldChanged);
    connect(m_showExt, &QCheckBox::toggled,         this, &FilterPanel::onAnyFieldChanged);
    connect(m_showFd,  &QCheckBox::toggled,         this, &FilterPanel::onAnyFieldChanged);
    connect(m_showErr, &QCheckBox::toggled,         this, &FilterPanel::onAnyFieldChanged);
    connect(m_idMin,   &QLineEdit::textChanged,     this, &FilterPanel::onAnyFieldChanged);
    connect(m_idMax,   &QLineEdit::textChanged,     this, &FilterPanel::onAnyFieldChanged);
    connect(m_idMask,  &QLineEdit::textChanged,     this, &FilterPanel::onAnyFieldChanged);
    connect(m_dlcMin,  &QLineEdit::textChanged,     this, &FilterPanel::onAnyFieldChanged);
    connect(m_dlcMax,  &QLineEdit::textChanged,     this, &FilterPanel::onAnyFieldChanged);
    connect(m_clear,   &QPushButton::clicked,       this, &FilterPanel::onClear);
}

bool FilterPanel::parseHex(const QString& s, uint32_t& out) {
    QString stripped = s;
    if (stripped.startsWith("0x", Qt::CaseInsensitive))
        stripped.remove(0, 2);
    bool ok = false;
    out = stripped.toUInt(&ok, 16);
    return ok;
}

FrameFilter FilterPanel::currentFilter() const {
    FrameFilter f;
    f.enabled  = m_enable->isChecked();
    f.show_std = m_showStd->isChecked();
    f.show_ext = m_showExt->isChecked();
    f.show_fd  = m_showFd->isChecked();
    f.show_err = m_showErr->isChecked();
    uint32_t v = 0;
    if (parseHex(m_idMin->text(), v))  f.id_min  = v;
    if (parseHex(m_idMax->text(), v))  f.id_max  = v;
    if (parseHex(m_idMask->text(), v)) f.id_mask = v;
    bool ok = false;
    int iv = m_dlcMin->text().toInt(&ok); if (ok) f.dlc_min = iv;
    iv = m_dlcMax->text().toInt(&ok);     if (ok) f.dlc_max = iv;
    return f;
}

void FilterPanel::onAnyFieldChanged() { emit filterChanged(currentFilter()); }

void FilterPanel::applyFilter(const FrameFilter& f) {
    const auto blk = [](QObject* o) { return QSignalBlocker(o); };
    auto b1=blk(m_enable); auto b2=blk(m_idMin);  auto b3=blk(m_idMax);
    auto b4=blk(m_idMask); auto b5=blk(m_dlcMin); auto b6=blk(m_dlcMax);
    auto b7=blk(m_showStd); auto b8=blk(m_showExt); auto b9=blk(m_showFd); auto b10=blk(m_showErr);
    m_enable->setChecked(f.enabled);
    m_idMin->setText(QString("0x%1").arg(f.id_min, 0, 16).toUpper());
    m_idMax->setText(QString("0x%1").arg(f.id_max, 0, 16).toUpper());
    m_idMask->setText(QString("0x%1").arg(f.id_mask, 0, 16).toUpper());
    m_dlcMin->setText(QString::number(f.dlc_min));
    m_dlcMax->setText(QString::number(f.dlc_max));
    m_showStd->setChecked(f.show_std); m_showExt->setChecked(f.show_ext);
    m_showFd->setChecked(f.show_fd);   m_showErr->setChecked(f.show_err);
    emit filterChanged(currentFilter());
}

void FilterPanel::onClear() {
    m_enable->setChecked(false);
    m_idMin->setText("0x000");
    m_idMax->setText("0x1FFFFFFF");
    m_idMask->setText("0xFFFFFFFF");
    m_dlcMin->setText("0");
    m_dlcMax->setText("15");
    m_showStd->setChecked(true);
    m_showExt->setChecked(true);
    m_showFd->setChecked(true);
    m_showErr->setChecked(false);
}

} // namespace socketspy::gui
