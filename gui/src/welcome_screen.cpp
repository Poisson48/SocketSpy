#include "welcome_screen.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QScrollArea>
#include <QListWidgetItem>
#include <QFileInfo>
#include <QFont>
#include <QSizePolicy>
#include <QSpacerItem>

namespace socketspy::gui {

// ── helper: section title ───────────────────────────────────────────────────
static QLabel* makeSectionTitle(const QString& text, QWidget* parent) {
    auto* lbl = new QLabel(text, parent);
    QFont f = lbl->font();
    f.setPointSize(f.pointSize() + 1);
    f.setBold(true);
    lbl->setFont(f);
    lbl->setStyleSheet("color: #7c8fa6; letter-spacing: 1px; text-transform: uppercase;");
    return lbl;
}

// ── helper: thin horizontal rule ────────────────────────────────────────────
static QFrame* makeSeparator(QWidget* parent) {
    auto* sep = new QFrame(parent);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Plain);
    sep->setStyleSheet("color: #2a3a52;");
    sep->setFixedHeight(1);
    return sep;
}

// ── WelcomeScreen ────────────────────────────────────────────────────────────
WelcomeScreen::WelcomeScreen(ProjectRegistry& registry, QWidget* parent)
    : QWidget(parent), m_registry(registry)
{
    buildUi();
}

void WelcomeScreen::buildUi() {
    // Outer scroll area so the welcome page works even at small heights
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* content = new QWidget(scroll);
    scroll->setWidget(content);

    auto* root = new QVBoxLayout(content);
    root->setContentsMargins(48, 40, 48, 40);
    root->setSpacing(32);

    root->addWidget(buildHeader());
    root->addWidget(makeSeparator(content));
    root->addWidget(buildRecentSection());
    root->addWidget(makeSeparator(content));

    // Bottom row: quick actions + quick start side by side
    auto* bottomRow = new QHBoxLayout;
    bottomRow->setSpacing(32);
    bottomRow->addWidget(buildQuickActionsSection(), 1);
    bottomRow->addWidget(buildQuickStartSection(),   1);
    root->addLayout(bottomRow);

    root->addStretch();

    // Wrap scroll area in own layout
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(scroll);
}

// ── Header ───────────────────────────────────────────────────────────────────
QWidget* WelcomeScreen::buildHeader() {
    auto* w   = new QWidget(this);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);

    auto* titleLbl = new QLabel(QString::fromUtf8("SocketSpy"), w);
    {
        QFont f = titleLbl->font();
        f.setPointSize(f.pointSize() + 14);
        f.setBold(true);
        titleLbl->setFont(f);
        titleLbl->setStyleSheet("color: #6366f1; letter-spacing: -0.5px;");
    }

    auto* subtitleLbl = new QLabel(tr("Linux CAN Analysis"), w);
    {
        QFont f = subtitleLbl->font();
        f.setPointSize(f.pointSize() + 3);
        subtitleLbl->setFont(f);
        subtitleLbl->setStyleSheet("color: #7c8fa6;");
    }

    lay->addWidget(titleLbl);
    lay->addWidget(subtitleLbl);
    return w;
}

// ── Recent projects ──────────────────────────────────────────────────────────
QWidget* WelcomeScreen::buildRecentSection() {
    auto* w   = new QWidget(this);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(10);

    lay->addWidget(makeSectionTitle(tr("Recent Projects"), w));

    m_recentList = new QListWidget(w);
    m_recentList->setObjectName("welcomeRecentList");
    m_recentList->setFrameShape(QFrame::NoFrame);
    m_recentList->setAlternatingRowColors(false);
    m_recentList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_recentList->setMinimumHeight(160);
    m_recentList->setMaximumHeight(300);
    m_recentList->setStyleSheet(
        "QListWidget { background: #1a2235; border: 1px solid #2a3a52; border-radius: 8px; padding: 4px; }"
        "QListWidget::item { padding: 0px; border-radius: 6px; }"
        "QListWidget::item:selected { background: rgba(99,102,241,0.18); }"
        "QListWidget::item:hover:!selected { background: #232f45; }"
    );
    refreshRecentProjects();
    lay->addWidget(m_recentList);

    // button row below the list
    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);

    auto* btnOpen   = new QPushButton(tr("Open selected"), w);
    btnOpen->setObjectName("welcomeOpenBtn");
    auto* btnBrowse = new QPushButton(tr("Browse…"), w);
    btnBrowse->setProperty("secondary", true);

    btnRow->addWidget(btnOpen);
    btnRow->addWidget(btnBrowse);
    btnRow->addStretch();
    lay->addLayout(btnRow);

    connect(m_recentList, &QListWidget::itemDoubleClicked,
            this,         &WelcomeScreen::onItemDoubleClicked);
    connect(btnOpen,   &QPushButton::clicked, this, &WelcomeScreen::onOpenSelectedProject);
    connect(btnBrowse, &QPushButton::clicked, this, [this]() {
        emit openProjectRequested(QString()); // empty → show browser dialog
    });

    return w;
}

// ── Quick actions ────────────────────────────────────────────────────────────
QWidget* WelcomeScreen::buildQuickActionsSection() {
    auto* w   = new QWidget(this);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(10);

    lay->addWidget(makeSectionTitle(tr("Quick Actions"), w));

    auto makeBtn = [&](const QString& label, const QString& desc) -> QPushButton* {
        auto* btn = new QPushButton(w);
        btn->setProperty("secondary", true);
        btn->setCursor(Qt::PointingHandCursor);

        auto* inner   = new QWidget(btn);
        auto* innerLy = new QVBoxLayout(inner);
        innerLy->setContentsMargins(0, 0, 0, 0);
        innerLy->setSpacing(2);

        auto* lbl  = new QLabel(label, inner);
        QFont f = lbl->font();
        f.setBold(true);
        lbl->setFont(f);
        lbl->setStyleSheet("color: #f1f5f9; background: transparent;");

        auto* sub  = new QLabel(desc, inner);
        sub->setStyleSheet("color: #7c8fa6; font-size: 11px; background: transparent;");

        innerLy->addWidget(lbl);
        innerLy->addWidget(sub);

        // We use a plain layout-based approach to get the two-line look
        // without fighting QPushButton's internal layout.
        auto* btnLy = new QVBoxLayout(btn);
        btnLy->setContentsMargins(14, 10, 14, 10);
        btnLy->addWidget(inner);

        btn->setStyleSheet(
            "QPushButton { background: #1a2235; border: 1px solid #2a3a52; border-radius: 8px; text-align: left; }"
            "QPushButton:hover { border-color: #6366f1; background: #232f45; }"
            "QPushButton:pressed { background: #1a2235; }"
        );
        btn->setFixedHeight(62);
        return btn;
    };

    auto* btnNew  = makeBtn(tr("New Project"),
                            tr("Start fresh with a blank workspace"));
    auto* btnDbc  = makeBtn(tr("Open DBC File\xe2\x80\xa6"),
                            tr("Load a .dbc database for signal decoding"));
    auto* btnProj = makeBtn(tr("Open Project\xe2\x80\xa6"),
                            tr("Browse and load a .spyproj file"));

    lay->addWidget(btnNew);
    lay->addWidget(btnDbc);
    lay->addWidget(btnProj);
    lay->addStretch();

    connect(btnNew,  &QPushButton::clicked, this, &WelcomeScreen::newProjectRequested);
    connect(btnDbc,  &QPushButton::clicked, this, &WelcomeScreen::openDbcRequested);
    connect(btnProj, &QPushButton::clicked, this, [this]() {
        emit openProjectRequested(QString());
    });

    return w;
}

// ── Quick start ──────────────────────────────────────────────────────────────
QWidget* WelcomeScreen::buildQuickStartSection() {
    auto* w   = new QWidget(this);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(10);

    lay->addWidget(makeSectionTitle(tr("Quick Start"), w));

    auto* grid = new QGridLayout;
    grid->setSpacing(10);

    struct TileInfo { QString icon; QString label; };
    const TileInfo tiles[] = {
        { QString::fromUtf8("\xf0\x9f\x94\x8c"), tr("Connect\nvcan0")     },
        { QString::fromUtf8("\xf0\x9f\x93\x84"), tr("Load\nDBC")          },
        { QString::fromUtf8("\xe2\x9a\x99"),      tr("Launch\nSimulator")  },
        { QString::fromUtf8("\xf0\x9f\x96\xa5"),  tr("Open\nMonitor")      },
    };

    auto makeTile = [&](const TileInfo& ti) -> QPushButton* {
        auto* btn = new QPushButton(w);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedSize(128, 100);
        btn->setStyleSheet(
            "QPushButton { background: #1a2235; border: 1px solid #2a3a52; border-radius: 10px; }"
            "QPushButton:hover { border-color: #6366f1; background: #232f45; }"
            "QPushButton:pressed { background: #111827; }"
        );

        auto* vlay = new QVBoxLayout(btn);
        vlay->setContentsMargins(8, 12, 8, 12);
        vlay->setSpacing(6);
        vlay->setAlignment(Qt::AlignCenter);

        auto* iconLbl = new QLabel(ti.icon, btn);
        {
            QFont f = iconLbl->font();
            f.setPointSize(f.pointSize() + 8);
            iconLbl->setFont(f);
        }
        iconLbl->setAlignment(Qt::AlignCenter);
        iconLbl->setStyleSheet("background: transparent;");

        auto* textLbl = new QLabel(ti.label, btn);
        textLbl->setAlignment(Qt::AlignCenter);
        textLbl->setStyleSheet("color: #f1f5f9; font-size: 11px; font-weight: 600; background: transparent;");
        textLbl->setWordWrap(true);

        vlay->addWidget(iconLbl);
        vlay->addWidget(textLbl);
        return btn;
    };

    auto* tileConnect   = makeTile(tiles[0]);
    auto* tileDbc       = makeTile(tiles[1]);
    auto* tileSimulator = makeTile(tiles[2]);
    auto* tileMonitor   = makeTile(tiles[3]);

    grid->addWidget(tileConnect,   0, 0);
    grid->addWidget(tileDbc,       0, 1);
    grid->addWidget(tileSimulator, 1, 0);
    grid->addWidget(tileMonitor,   1, 1);

    lay->addLayout(grid);
    lay->addStretch();

    connect(tileConnect,   &QPushButton::clicked, this, &WelcomeScreen::quickConnectRequested);
    connect(tileDbc,       &QPushButton::clicked, this, &WelcomeScreen::openDbcRequested);
    connect(tileSimulator, &QPushButton::clicked, this, &WelcomeScreen::showSimulatorRequested);
    connect(tileMonitor,   &QPushButton::clicked, this, &WelcomeScreen::showMonitorRequested);

    return w;
}

// ── Slots ────────────────────────────────────────────────────────────────────
void WelcomeScreen::refreshRecentProjects() {
    if (!m_recentList) return;

    m_recentList->clear();
    const auto entries = m_registry.entries();

    if (entries.isEmpty()) {
        auto* item = new QListWidgetItem(m_recentList);
        item->setFlags(Qt::NoItemFlags);
        auto* lbl = new QLabel(tr("No recent projects"), m_recentList);
        lbl->setStyleSheet("color: #7c8fa6; padding: 12px 16px;");
        lbl->setAlignment(Qt::AlignCenter);
        m_recentList->setItemWidget(item, lbl);
        item->setSizeHint(QSize(0, 46));
        return;
    }

    for (const ProjectEntry& e : entries) {
        auto* item = new QListWidgetItem(m_recentList);
        item->setData(Qt::UserRole, e.path);

        auto* w   = new QWidget;
        auto* lay = new QHBoxLayout(w);
        lay->setContentsMargins(12, 8, 12, 8);
        lay->setSpacing(12);

        // left: name + path
        auto* textCol = new QWidget(w);
        auto* textLay = new QVBoxLayout(textCol);
        textLay->setContentsMargins(0, 0, 0, 0);
        textLay->setSpacing(2);

        auto* nameLbl = new QLabel(e.name, textCol);
        {
            QFont f = nameLbl->font();
            f.setBold(true);
            nameLbl->setFont(f);
            nameLbl->setStyleSheet("color: #f1f5f9; background: transparent;");
        }
        auto* pathLbl = new QLabel(e.path, textCol);
        pathLbl->setStyleSheet("color: #7c8fa6; font-size: 11px; background: transparent;");
        pathLbl->setTextFormat(Qt::PlainText);

        textLay->addWidget(nameLbl);
        textLay->addWidget(pathLbl);
        lay->addWidget(textCol, 1);

        // right: date
        if (e.lastOpened.isValid()) {
            auto* dateLbl = new QLabel(e.lastOpened.toString("yyyy-MM-dd"), w);
            dateLbl->setStyleSheet("color: #7c8fa6; font-size: 11px; background: transparent;");
            dateLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            lay->addWidget(dateLbl);
        }

        item->setSizeHint(w->sizeHint() + QSize(0, 6));
        m_recentList->setItemWidget(item, w);
    }
}

void WelcomeScreen::onItemDoubleClicked(QListWidgetItem* item) {
    if (!item) return;
    const QString path = item->data(Qt::UserRole).toString();
    if (!path.isEmpty())
        emit openProjectRequested(path);
}

void WelcomeScreen::onOpenSelectedProject() {
    auto* item = m_recentList->currentItem();
    if (!item) return;
    const QString path = item->data(Qt::UserRole).toString();
    if (!path.isEmpty())
        emit openProjectRequested(path);
}

} // namespace socketspy::gui
