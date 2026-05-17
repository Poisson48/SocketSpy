#include "update_dialog.h"

#include <QHBoxLayout>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QVBoxLayout>

namespace socketspy::gui {

UpdateDialog::UpdateDialog(Updater* updater, QWidget* parent)
    : QDialog(parent, Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint),
      m_updater(updater) {
    setWindowTitle(tr("Software Update"));
    setMinimumWidth(400);
    setModal(true);

    m_icon   = new QLabel(this);
    m_status = new QLabel(tr("Checking for updates…"), this);
    m_status->setWordWrap(true);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 0);  // indeterminate
    m_progress->setVisible(false);

    m_actionBtn = new QPushButton(tr("Download"), this);
    m_actionBtn->setVisible(false);
    m_closeBtn = new QPushButton(tr("Close"), this);

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnRow->addWidget(m_actionBtn);
    btnRow->addWidget(m_closeBtn);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(12);
    root->addWidget(m_status);
    root->addWidget(m_progress);
    root->addLayout(btnRow);

    connect(m_closeBtn,  &QPushButton::clicked, this, &QDialog::accept);
    connect(m_actionBtn, &QPushButton::clicked, this, &UpdateDialog::onActionClicked);

    connect(m_updater, &Updater::updateAvailable,  this, &UpdateDialog::onUpdateAvailable);
    connect(m_updater, &Updater::upToDate,         this, &UpdateDialog::onUpToDate);
    connect(m_updater, &Updater::checkError,       this, &UpdateDialog::onCheckError);
    connect(m_updater, &Updater::downloadProgress, this, &UpdateDialog::onDownloadProgress);
    connect(m_updater, &Updater::downloadError,    this, &UpdateDialog::onDownloadError);
    connect(m_updater, &Updater::installReady,     this, &UpdateDialog::onInstallReady);
}

void UpdateDialog::startCheck() {
    setPhase(Phase::Checking);
    m_updater->checkForUpdates();
}

// ── Updater signal handlers ──────────────────────────────────────────────────

void UpdateDialog::onUpdateAvailable(const QString& version) {
    m_status->setText(tr("Version <b>%1</b> is available.").arg(version));
    setPhase(Phase::Available);
}

void UpdateDialog::onUpToDate() {
    m_status->setText(tr("SocketSpy is up to date."));
    setPhase(Phase::Done);
}

void UpdateDialog::onCheckError(const QString& msg) {
    m_status->setText(tr("Check failed: %1").arg(msg));
    setPhase(Phase::Error);
}

void UpdateDialog::onDownloadProgress(qint64 done, qint64 total) {
    if (total > 0) {
        m_progress->setRange(0, static_cast<int>(total / 1024));
        m_progress->setValue(static_cast<int>(done / 1024));
    }
}

void UpdateDialog::onDownloadError(const QString& msg) {
    m_status->setText(tr("Download failed: %1").arg(msg));
    setPhase(Phase::Error);
}

void UpdateDialog::onInstallReady() {
    m_status->setText(
        tr("Update installed. Restart SocketSpy to apply the new version."));
    setPhase(Phase::Done);
}

void UpdateDialog::onActionClicked() {
    if (m_phase == Phase::Available) {
        setPhase(Phase::Downloading);
        m_updater->startDownload();
    }
}

// ── UI state machine ─────────────────────────────────────────────────────────

void UpdateDialog::setPhase(Phase phase) {
    m_phase = phase;
    switch (phase) {
        case Phase::Checking:
            m_progress->setRange(0, 0);
            m_progress->setVisible(true);
            m_actionBtn->setVisible(false);
            break;

        case Phase::Available:
            m_progress->setVisible(false);
            m_actionBtn->setText(tr("Download && Install"));
            m_actionBtn->setVisible(true);
            break;

        case Phase::Downloading:
            m_status->setText(tr("Downloading update…"));
            m_progress->setRange(0, 0);
            m_progress->setVisible(true);
            m_actionBtn->setEnabled(false);
            break;

        case Phase::Done:
            m_progress->setVisible(false);
            m_actionBtn->setVisible(false);
            break;

        case Phase::Error:
            m_progress->setVisible(false);
            m_actionBtn->setVisible(false);
            break;
    }
}

}  // namespace socketspy::gui
