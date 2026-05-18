#include "welcome_screen.h"

#include <QCheckBox>
#include <QDesktopServices>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSettings>
#include <QUrl>
#include <QVBoxLayout>

namespace socketspy::gui {

// ── helpers ──────────────────────────────────────────────────────────────────

static QFrame* makeSep(QWidget* parent) {
    auto* sep = new QFrame(parent);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color: #2a3a52;");
    sep->setFixedHeight(1);
    return sep;
}

static QLabel* makeSectionTitle(const QString& text, QWidget* parent) {
    auto* lbl = new QLabel(text.toUpper(), parent);
    QFont f = lbl->font();
    f.setPointSize(f.pointSize() - 1);
    f.setBold(true);
    lbl->setFont(f);
    lbl->setStyleSheet("color: #4b5563; letter-spacing: 1px;");
    return lbl;
}

// ── WelcomeScreen ────────────────────────────────────────────────────────────

WelcomeScreen::WelcomeScreen(ProjectRegistry& registry, QWidget* parent)
    : QDialog(parent, Qt::Dialog), m_registry(registry)
{
    setWindowTitle(tr("Welcome to SocketSpy"));
    setFixedSize(820, 520);
    setModal(false);
    buildUi();

    // Center on parent (or screen if no parent)
    if (parent) {
        const QRect pr = parent->geometry();
        move(pr.center() - rect().center());
    }
}

bool WelcomeScreen::shouldShow() {
    QSettings s;
    return s.value("welcome/show", true).toBool();
}

void WelcomeScreen::buildUi() {
    setStyleSheet(R"(
        QDialog { background: #111827; }
        QLabel  { color: #e5e7eb; }
    )");

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(buildLeftPanel());

    // vertical divider
    auto* divider = new QFrame(this);
    divider->setFrameShape(QFrame::VLine);
    divider->setStyleSheet("color: #1f2d42;");
    divider->setFixedWidth(1);
    root->addWidget(divider);

    root->addWidget(buildRightPanel());
}

// ── Left panel : branding + links + update + start ───────────────────────────

QWidget* WelcomeScreen::buildLeftPanel() {
    auto* w   = new QWidget(this);
    w->setFixedWidth(320);
    w->setStyleSheet("QWidget { background: #0d1117; }");

    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(28, 28, 28, 20);
    lay->setSpacing(0);

    // ── Branding ────────────────────────────────────────────────────────────
    auto* logoLbl = new QLabel("SocketSpy", w);
    {
        QFont f = logoLbl->font();
        f.setPointSize(f.pointSize() + 16);
        f.setBold(true);
        logoLbl->setFont(f);
        logoLbl->setStyleSheet("color: #6366f1; background: transparent;");
    }
    lay->addWidget(logoLbl);

    auto* tagLbl = new QLabel(tr("CAN bus analyzer · Linux"), w);
    tagLbl->setStyleSheet("color: #6b7280; font-size: 12px; background: transparent;");
    lay->addWidget(tagLbl);

    lay->addSpacing(6);

    auto* verLbl = new QLabel(QString("v%1").arg(APP_VERSION), w);
    verLbl->setStyleSheet("color: #374151; font-size: 11px; background: transparent;");
    lay->addWidget(verLbl);

    lay->addSpacing(24);
    lay->addWidget(makeSep(w));
    lay->addSpacing(16);

    // ── Links ────────────────────────────────────────────────────────────────
    lay->addWidget(makeSectionTitle(tr("Resources"), w));
    lay->addSpacing(10);

    const struct { const char* icon; const char* label; const char* url; } links[] = {
        { "📖", QT_TR_NOOP("Documentation / Wiki"),
                "https://github.com/Poisson48/SocketSpy/wiki" },
        { "💻", QT_TR_NOOP("Source code (GitHub)"),
                "https://github.com/Poisson48/SocketSpy" },
        { "🐛", QT_TR_NOOP("Report a bug"),
                "https://github.com/Poisson48/SocketSpy/issues/new" },
        { "📋", QT_TR_NOOP("Changelog"),
                "https://github.com/Poisson48/SocketSpy/releases" },
    };

    for (const auto& lk : links) {
        auto* lbl = makeLink(lk.icon, tr(lk.label), lk.url, w);
        lay->addWidget(lbl);
        lay->addSpacing(4);
    }

    lay->addSpacing(16);
    lay->addWidget(makeSep(w));
    lay->addSpacing(16);

    // ── Update check ─────────────────────────────────────────────────────────
    lay->addWidget(makeSectionTitle(tr("Updates"), w));
    lay->addSpacing(10);

    auto* currentVerLbl = new QLabel(
        tr("Installed version: <b>%1</b>").arg(APP_VERSION), w);
    currentVerLbl->setStyleSheet(
        "color: #6b7280; font-size: 11px; background: transparent;");
    currentVerLbl->setTextFormat(Qt::RichText);
    lay->addWidget(currentVerLbl);

    lay->addSpacing(6);

    auto* updateBtn = new QPushButton(tr("Check for updates"), w);
    updateBtn->setStyleSheet(R"(
        QPushButton {
            background: #1a2235;
            color: #6366f1;
            border: 1px solid #6366f1;
            border-radius: 5px;
            padding: 5px 12px;
            font-size: 11px;
            text-align: left;
        }
        QPushButton:hover {
            background: #1f2d42;
            color: #818cf8;
            border-color: #818cf8;
        }
        QPushButton:pressed {
            background: #111827;
        }
    )");
    connect(updateBtn, &QPushButton::clicked, this, [this]() {
        emit checkForUpdatesRequested();
    });
    lay->addWidget(updateBtn);

    lay->addStretch();
    lay->addWidget(makeSep(w));
    lay->addSpacing(12);

    // ── Bottom: ne plus afficher + fermer ────────────────────────────────────
    auto* check = new QCheckBox(tr("Don't show at startup"), w);
    check->setStyleSheet(
        "QCheckBox { color: #4b5563; font-size: 11px; background: transparent; }"
        "QCheckBox::indicator { width: 14px; height: 14px; }");
    QSettings s;
    check->setChecked(!s.value("welcome/show", true).toBool());
    connect(check, &QCheckBox::toggled, [](bool checked) {
        QSettings ss;
        ss.setValue("welcome/show", !checked);
    });
    lay->addWidget(check);

    lay->addSpacing(8);

    auto* closeBtn = new QPushButton(tr("Get started  →"), w);
    closeBtn->setStyleSheet(R"(
        QPushButton {
            background: #6366f1;
            color: #ffffff;
            border: none;
            border-radius: 6px;
            padding: 8px 18px;
            font-weight: bold;
        }
        QPushButton:hover { background: #4f46e5; }
        QPushButton:pressed { background: #4338ca; }
    )");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    lay->addWidget(closeBtn);

    return w;
}

// ── Right panel : recent projects + quick actions ────────────────────────────

QWidget* WelcomeScreen::buildRightPanel() {
    auto* w   = new QWidget(this);
    w->setStyleSheet("QWidget { background: #111827; }");

    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(28, 28, 28, 20);
    lay->setSpacing(0);

    // Recent projects
    lay->addWidget(makeSectionTitle(tr("Recent projects"), w));
    lay->addSpacing(10);

    m_recentList = new QListWidget(w);
    m_recentList->setObjectName("welcomeRecentList");
    m_recentList->setFrameShape(QFrame::NoFrame);
    m_recentList->setAlternatingRowColors(false);
    m_recentList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_recentList->setStyleSheet(R"(
        QListWidget {
            background: #1a2235;
            border: 1px solid #1f2d42;
            border-radius: 8px;
            padding: 4px;
        }
        QListWidget::item { padding: 0px; border-radius: 6px; }
        QListWidget::item:selected { background: rgba(99,102,241,0.2); }
        QListWidget::item:hover:!selected { background: #1f2d42; }
    )");
    refreshRecentProjects();
    lay->addWidget(m_recentList, 1);

    lay->addSpacing(10);

    // Open buttons
    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);
    auto* btnOpen   = new QPushButton(tr("Open selection"), w);
    auto* btnBrowse = new QPushButton(tr("Browse…"), w);
    for (auto* b : {btnOpen, btnBrowse}) {
        b->setStyleSheet(R"(
            QPushButton {
                background: #1a2235; color: #9ca3af;
                border: 1px solid #1f2d42; border-radius: 6px;
                padding: 6px 14px; font-size: 12px;
            }
            QPushButton:hover { border-color: #6366f1; color: #e5e7eb; }
        )");
    }
    connect(btnOpen,   &QPushButton::clicked, this, &WelcomeScreen::onOpenSelectedProject);
    connect(btnBrowse, &QPushButton::clicked, this, [this]() {
        emit openProjectRequested(QString());
        accept();
    });
    btnRow->addWidget(btnOpen);
    btnRow->addWidget(btnBrowse);
    btnRow->addStretch();
    lay->addLayout(btnRow);

    lay->addSpacing(16);
    lay->addWidget(makeSep(w));
    lay->addSpacing(16);

    // Quick start
    lay->addWidget(makeSectionTitle(tr("Quick start"), w));
    lay->addSpacing(10);

    auto* quickRow = new QHBoxLayout;
    quickRow->setSpacing(8);

    const struct { const char* icon; const char* label; } tiles[] = {
        { "📁", QT_TR_NOOP("New project")   },
        { "🔌", QT_TR_NOOP("Connect vcan0") },
        { "📄", QT_TR_NOOP("Open DBC")      },
        { "⚙",  QT_TR_NOOP("Simulator")     },
    };

    auto makeTile = [&](const char* icon, const char* label) -> QPushButton* {
        auto* btn = new QPushButton(w);
        btn->setFixedSize(100, 72);
        btn->setStyleSheet(R"(
            QPushButton {
                background: #1a2235; border: 1px solid #1f2d42;
                border-radius: 8px;
            }
            QPushButton:hover { border-color: #6366f1; background: #1f2d42; }
            QPushButton:pressed { background: #111827; }
        )");
        auto* vl = new QVBoxLayout(btn);
        vl->setContentsMargins(4, 8, 4, 8);
        vl->setSpacing(4);
        vl->setAlignment(Qt::AlignCenter);

        auto* iconLbl = new QLabel(QString::fromUtf8(icon), btn);
        QFont f = iconLbl->font();
        f.setPointSize(f.pointSize() + 6);
        iconLbl->setFont(f);
        iconLbl->setAlignment(Qt::AlignCenter);
        iconLbl->setStyleSheet("background: transparent;");

        auto* textLbl = new QLabel(tr(label), btn);
        textLbl->setAlignment(Qt::AlignCenter);
        textLbl->setWordWrap(true);
        textLbl->setStyleSheet(
            "color: #9ca3af; font-size: 10px; font-weight: 600; background: transparent;");
        vl->addWidget(iconLbl);
        vl->addWidget(textLbl);
        return btn;
    };

    auto* tileNew  = makeTile(tiles[0].icon, tiles[0].label);
    auto* tileCon  = makeTile(tiles[1].icon, tiles[1].label);
    auto* tileDbc  = makeTile(tiles[2].icon, tiles[2].label);
    auto* tileSim  = makeTile(tiles[3].icon, tiles[3].label);

    connect(tileNew,  &QPushButton::clicked, this, [this]() { emit newProjectRequested();    accept(); });
    connect(tileCon,  &QPushButton::clicked, this, [this]() { emit quickConnectRequested();  accept(); });
    connect(tileDbc,  &QPushButton::clicked, this, [this]() { emit openDbcRequested();       accept(); });
    connect(tileSim,  &QPushButton::clicked, this, [this]() { emit showSimulatorRequested(); accept(); });

    quickRow->addWidget(tileNew);
    quickRow->addWidget(tileCon);
    quickRow->addWidget(tileDbc);
    quickRow->addWidget(tileSim);
    quickRow->addStretch();
    lay->addLayout(quickRow);

    connect(m_recentList, &QListWidget::itemDoubleClicked,
            this,         &WelcomeScreen::onItemDoubleClicked);

    return w;
}

// ── Link label ────────────────────────────────────────────────────────────────

QLabel* WelcomeScreen::makeLink(const QString& icon, const QString& label,
                                 const QString& url, QWidget* parent)
{
    auto* lbl = new QLabel(
        QString("%1 <a href=\"%2\" style=\"color:#6366f1; text-decoration:none;\">%3</a>")
            .arg(icon, url, label),
        parent);
    lbl->setTextFormat(Qt::RichText);
    lbl->setOpenExternalLinks(true);
    lbl->setStyleSheet("background: transparent; font-size: 12px;");
    lbl->setCursor(Qt::PointingHandCursor);
    return lbl;
}

// ── Recent projects ───────────────────────────────────────────────────────────

void WelcomeScreen::refreshRecentProjects() {
    if (!m_recentList) return;
    m_recentList->clear();

    const auto entries = m_registry.entries();
    if (entries.isEmpty()) {
        auto* item = new QListWidgetItem(m_recentList);
        item->setFlags(Qt::NoItemFlags);
        auto* lbl = new QLabel(tr("No recent projects"), m_recentList);
        lbl->setStyleSheet("color: #4b5563; padding: 12px 16px; background: transparent;");
        lbl->setAlignment(Qt::AlignCenter);
        m_recentList->setItemWidget(item, lbl);
        item->setSizeHint(QSize(0, 46));
        return;
    }

    for (const ProjectEntry& e : entries) {
        auto* item = new QListWidgetItem(m_recentList);
        item->setData(Qt::UserRole, e.path);

        auto* row = new QWidget;
        auto* rl  = new QHBoxLayout(row);
        rl->setContentsMargins(12, 8, 12, 8);
        rl->setSpacing(12);

        auto* col = new QWidget(row);
        auto* cl  = new QVBoxLayout(col);
        cl->setContentsMargins(0, 0, 0, 0);
        cl->setSpacing(2);

        auto* nameLbl = new QLabel(e.name, col);
        QFont f = nameLbl->font(); f.setBold(true); nameLbl->setFont(f);
        nameLbl->setStyleSheet("color: #f1f5f9; background: transparent;");

        auto* pathLbl = new QLabel(e.path, col);
        pathLbl->setStyleSheet("color: #6b7280; font-size: 10px; background: transparent;");
        pathLbl->setTextFormat(Qt::PlainText);

        cl->addWidget(nameLbl);
        cl->addWidget(pathLbl);
        rl->addWidget(col, 1);

        if (e.lastOpened.isValid()) {
            auto* dateLbl = new QLabel(e.lastOpened.toString("yyyy-MM-dd"), row);
            dateLbl->setStyleSheet(
                "color: #4b5563; font-size: 10px; background: transparent;");
            dateLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            rl->addWidget(dateLbl);
        }

        item->setSizeHint(row->sizeHint() + QSize(0, 6));
        m_recentList->setItemWidget(item, row);
    }
}

void WelcomeScreen::onItemDoubleClicked(QListWidgetItem* item) {
    if (!item) return;
    const QString path = item->data(Qt::UserRole).toString();
    if (!path.isEmpty()) {
        emit openProjectRequested(path);
        accept();
    }
}

void WelcomeScreen::onOpenSelectedProject() {
    auto* item = m_recentList->currentItem();
    if (!item) return;
    const QString path = item->data(Qt::UserRole).toString();
    if (!path.isEmpty()) {
        emit openProjectRequested(path);
        accept();
    }
}

} // namespace socketspy::gui
