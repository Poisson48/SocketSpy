#include "project_browser.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidgetItem>
#include <QFileDialog>
#include <QFileInfo>
#include <QDateTime>
#include <QFont>
#include <QFrame>

namespace socketspy::gui {

ProjectBrowserDialog::ProjectBrowserDialog(ProjectRegistry& registry, QWidget* parent)
    : QDialog(parent), m_registry(registry)
{
    setWindowTitle(tr("Projects"));
    setMinimumSize(560, 440);
    resize(640, 500);

    auto* root = new QVBoxLayout(this);
    root->setSpacing(10);
    root->setContentsMargins(16, 16, 16, 16);

    // title
    auto* titleLabel = new QLabel(tr("Open a project"), this);
    QFont f = titleLabel->font();
    f.setPointSize(f.pointSize() + 3);
    f.setBold(true);
    titleLabel->setFont(f);
    root->addWidget(titleLabel);

    // search bar
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Search projects…"));
    m_search->setClearButtonEnabled(true);
    root->addWidget(m_search);

    // project list
    m_list = new QListWidget(this);
    m_list->setAlternatingRowColors(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    root->addWidget(m_list);

    // separator
    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    root->addWidget(sep);

    // button row
    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);

    auto* btnNew      = new QPushButton(tr("New project…"), this);
    auto* btnOpenFile = new QPushButton(tr("Browse…"), this);
    m_btnRemove       = new QPushButton(tr("Remove from list"), this);
    m_btnOpen         = new QPushButton(tr("Open"), this);

    m_btnOpen->setDefault(true);
    m_btnRemove->setEnabled(false);
    m_btnOpen->setEnabled(false);

    btnRow->addWidget(btnNew);
    btnRow->addWidget(btnOpenFile);
    btnRow->addWidget(m_btnRemove);
    btnRow->addStretch();
    btnRow->addWidget(m_btnOpen);
    root->addLayout(btnRow);

    rebuildList();

    connect(m_search,  &QLineEdit::textChanged,       this, &ProjectBrowserDialog::onSearchChanged);
    connect(m_list,    &QListWidget::itemDoubleClicked, this, &ProjectBrowserDialog::onItemDoubleClicked);
    connect(m_list,    &QListWidget::itemSelectionChanged, this, &ProjectBrowserDialog::onSelectionChanged);
    connect(m_btnOpen, &QPushButton::clicked,          this, &ProjectBrowserDialog::onOpenSelected);
    connect(btnOpenFile, &QPushButton::clicked,        this, &ProjectBrowserDialog::onOpenFile);
    connect(m_btnRemove, &QPushButton::clicked,        this, &ProjectBrowserDialog::onRemoveSelected);
    connect(btnNew,    &QPushButton::clicked,          this, &ProjectBrowserDialog::onNewProject);
}

void ProjectBrowserDialog::rebuildList(const QString& filter) {
    m_list->clear();
    const QString lo = filter.toLower();

    for (const ProjectEntry& e : m_registry.entries()) {
        if (!lo.isEmpty() && !e.name.toLower().contains(lo) && !e.path.toLower().contains(lo))
            continue;

        auto* item = new QListWidgetItem(m_list);

        // main text: name
        const QString when = e.lastOpened.isValid()
            ? e.lastOpened.toString("yyyy-MM-dd  hh:mm")
            : QString();

        item->setText(e.name);
        item->setToolTip(e.path);
        item->setData(Qt::UserRole, e.path);
        item->setData(Qt::UserRole + 1, e.path);   // display path
        item->setData(Qt::UserRole + 2, when);

        // two-line display via a small widget
        auto* w       = new QWidget;
        auto* lay     = new QVBoxLayout(w);
        lay->setContentsMargins(6, 4, 6, 4);
        lay->setSpacing(1);

        auto* nameL = new QLabel(e.name, w);
        QFont nf = nameL->font();
        nf.setBold(true);
        nameL->setFont(nf);

        QString detail = e.path;
        if (!when.isEmpty()) detail += "   ·   " + when;
        auto* detL = new QLabel(detail, w);
        detL->setStyleSheet("color: #6b7280; font-size: 11px;");
        detL->setWordWrap(false);

        lay->addWidget(nameL);
        lay->addWidget(detL);

        item->setSizeHint(w->sizeHint() + QSize(0, 8));
        m_list->setItemWidget(item, w);
    }

    onSelectionChanged();
}

void ProjectBrowserDialog::onSearchChanged(const QString& text) {
    rebuildList(text);
}

void ProjectBrowserDialog::onSelectionChanged() {
    const bool hasSel = !m_list->selectedItems().isEmpty();
    m_btnOpen->setEnabled(hasSel);
    m_btnRemove->setEnabled(hasSel);
}

void ProjectBrowserDialog::onItemDoubleClicked(QListWidgetItem* item) {
    if (!item) return;
    m_selectedPath = item->data(Qt::UserRole).toString();
    accept();
}

void ProjectBrowserDialog::onOpenSelected() {
    auto* item = m_list->currentItem();
    if (!item) return;
    m_selectedPath = item->data(Qt::UserRole).toString();
    accept();
}

void ProjectBrowserDialog::onOpenFile() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open project"), {}, tr("SocketSpy Projects (*.spyproj);;All Files (*)"));
    if (path.isEmpty()) return;
    m_selectedPath = path;
    accept();
}

void ProjectBrowserDialog::onRemoveSelected() {
    auto* item = m_list->currentItem();
    if (!item) return;
    m_registry.remove(item->data(Qt::UserRole).toString());
    rebuildList(m_search->text());
}

void ProjectBrowserDialog::onNewProject() {
    m_wantsNew = true;
    accept();
}

} // namespace socketspy::gui
