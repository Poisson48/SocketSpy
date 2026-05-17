#include "splash_screen.h"

#include <QApplication>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>

namespace socketspy::gui {

SplashScreen::SplashScreen(QWidget* parent)
    : QWidget(parent, Qt::SplashScreen | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    setAttribute(Qt::WA_TranslucentBackground, false);
    buildUi();

    // Center on primary screen
    if (const QScreen* scr = QApplication::primaryScreen()) {
        const QRect sg = scr->geometry();
        move(sg.center() - rect().center());
    }

    m_timer = new QTimer(this);
    m_timer->setInterval(220);
    connect(m_timer, &QTimer::timeout, this, &SplashScreen::onTick);
    m_timer->start();
}

void SplashScreen::buildUi() {
    setFixedSize(480, 260);
    setStyleSheet("QWidget { background: #0f1623; }");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(40, 32, 40, 28);
    root->setSpacing(0);

    // App name
    auto* nameLbl = new QLabel("SocketSpy", this);
    {
        QFont f = nameLbl->font();
        f.setPointSize(f.pointSize() + 22);
        f.setBold(true);
        nameLbl->setFont(f);
        nameLbl->setStyleSheet("color: #6366f1; letter-spacing: -1px;");
    }
    root->addWidget(nameLbl);

    // Version + tagline
    auto* versionLbl = new QLabel(
        QString("v%1  ·  Linux CAN Analysis").arg(APP_VERSION), this);
    {
        QFont f = versionLbl->font();
        f.setPointSize(f.pointSize() + 1);
        versionLbl->setFont(f);
        versionLbl->setStyleSheet("color: #4b5563; margin-top: 4px;");
    }
    root->addWidget(versionLbl);

    root->addStretch(1);

    // Progress bar
    m_progress = new QProgressBar(this);
    m_progress->setRange(0, kSteps);
    m_progress->setValue(0);
    m_progress->setTextVisible(false);
    m_progress->setFixedHeight(4);
    m_progress->setStyleSheet(
        "QProgressBar { background: #1e2a3a; border: none; border-radius: 2px; }"
        "QProgressBar::chunk { background: #6366f1; border-radius: 2px; }");
    root->addWidget(m_progress);

    root->addSpacing(8);

    // Loading status
    m_statusLabel = new QLabel(tr("Loading…"), this);
    m_statusLabel->setStyleSheet("color: #4b5563; font-size: 11px;");
    root->addWidget(m_statusLabel);

    root->addStretch(1);

    // Footer
    auto* footerLbl = new QLabel(tr("100% local · no telemetry · MIT license"), this);
    footerLbl->setStyleSheet("color: #374151; font-size: 10px;");
    root->addWidget(footerLbl);
}

void SplashScreen::onTick() {
    static const char* const kMessages[] = {
        QT_TR_NOOP("Loading interface…"),
        QT_TR_NOOP("Initialising CAN core…"),
        QT_TR_NOOP("Loading protocols…"),
        QT_TR_NOOP("Preparing panels…"),
        QT_TR_NOOP("Loading DBC engine…"),
        QT_TR_NOOP("Starting up…"),
    };

    if (m_step < kSteps) {
        m_statusLabel->setText(tr(kMessages[m_step]));
        m_progress->setValue(m_step + 1);
        ++m_step;
    } else {
        m_timer->stop();
    }
}

void SplashScreen::finish(QWidget* mainWindow) {
    if (mainWindow)
        mainWindow->activateWindow();
    close();
    deleteLater();
}

} // namespace socketspy::gui
