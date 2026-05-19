#pragma once
#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>

#include "updater.h"

namespace socketspy::gui {

// Self-contained dialog that drives the full update flow:
//   Checking → Update available / Up-to-date / Error
//   → Downloading (progress) → Installed (restart prompt)
class UpdateDialog : public QDialog {
    Q_OBJECT
public:
    explicit UpdateDialog(Updater* updater, QWidget* parent = nullptr);
    void startCheck();

private slots:
    void onUpdateAvailable(const QString& version);
    void onUpToDate();
    void onCheckError(const QString& msg);
    void onDownloadProgress(qint64 done, qint64 total);
    void onDownloadError(const QString& msg);
    void onInstallReady();
    void onActionClicked();

private:
    enum class Phase { Checking, Available, Downloading, Done, Error };

    void setPhase(Phase phase);

    Updater*      m_updater{nullptr};
    QLabel*       m_icon{nullptr};
    QLabel*       m_status{nullptr};
    QProgressBar* m_progress{nullptr};
    QPushButton*  m_actionBtn{nullptr};
    QPushButton*  m_closeBtn{nullptr};
    Phase         m_phase{Phase::Checking};
    bool          m_isAppImage{false};
};

}  // namespace socketspy::gui
