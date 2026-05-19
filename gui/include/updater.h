#pragma once
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QString>

namespace socketspy::gui {

// Manual update checker for AppImage installations.
// Network calls happen ONLY when checkForUpdates() is explicitly invoked.
class Updater : public QObject {
    Q_OBJECT
public:
    explicit Updater(QObject* parent = nullptr);

    // Returns true when running inside an AppImage (APPIMAGE env var set).
    bool isAppImage() const;

    // Step 1 — fetch manifest + signature from GitHub Releases and verify.
    void checkForUpdates();

    // Step 2 — download + verify SHA-256 + install atomically.
    // Call after the user confirms the update via updateAvailable().
    void startDownload();
    void cancel();

signals:
    void updateAvailable(const QString& version);
    void upToDate();
    void checkError(const QString& msg);
    void downloadProgress(qint64 done, qint64 total);
    void downloadError(const QString& msg);
    void installReady();  // emit after atomic install — caller should prompt restart

private:
    enum class State { Idle, FetchManifest, FetchSig, Downloading };

    void onManifestReply(QNetworkReply* reply);
    void onSigReply(QNetworkReply* reply);
    void onAppImageReply(QNetworkReply* reply);

    bool verifySignature(const QByteArray& manifest, const QByteArray& sig) const;
    bool verifySha256(const QString& path, const QString& expected) const;
    bool atomicInstall(const QString& tmpPath) const;

    QNetworkAccessManager* m_nam{nullptr};
    QNetworkReply*         m_reply{nullptr};
    State                  m_state{State::Idle};

    QByteArray m_manifest;
    QString    m_version;
    QString    m_sha256;
    QString    m_filename;
    QString    m_tmpPath;
};

}  // namespace socketspy::gui
