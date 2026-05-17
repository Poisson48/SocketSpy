#include "updater.h"

#include <openssl/evp.h>
#include <openssl/pem.h>

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QVersionNumber>

namespace socketspy::gui {

static constexpr const char* kPublicKeyPem = R"(
-----BEGIN PUBLIC KEY-----
MCowBQYDK2VwAyEAhIZMKziyWB8q6AfmCBHm4nMg8A7T84ThIgTwfMVacEI=
-----END PUBLIC KEY-----
)";

static constexpr const char* kManifestUrl =
    "https://github.com/Poisson48/SocketSpy/releases/latest/download/"
    "release-manifest.json";

static constexpr const char* kSigUrl =
    "https://github.com/Poisson48/SocketSpy/releases/latest/download/"
    "release-manifest.json.sig";

Updater::Updater(QObject* parent) : QObject(parent) {
    m_nam = new QNetworkAccessManager(this);
    m_nam->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
}

bool Updater::isAppImage() const {
    return !qgetenv("APPIMAGE").isEmpty();
}

void Updater::checkForUpdates() {
    if (m_state != State::Idle) return;
    m_state = State::FetchManifest;

    QNetworkRequest req{QUrl(kManifestUrl)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    m_reply = m_nam->get(req);
    connect(m_reply, &QNetworkReply::finished, this,
            [this]() { onManifestReply(m_reply); });
}

void Updater::cancel() {
    if (m_reply) m_reply->abort();
    m_state = State::Idle;
}

// ── private helpers ──────────────────────────────────────────────────────────

void Updater::onManifestReply(QNetworkReply* reply) {
    reply->deleteLater();
    m_reply = nullptr;

    if (reply->error() != QNetworkReply::NoError) {
        m_state = State::Idle;
        emit checkError(tr("Network error: %1").arg(reply->errorString()));
        return;
    }

    m_manifest = reply->readAll();
    m_state = State::FetchSig;

    QNetworkRequest req{QUrl(kSigUrl)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    m_reply = m_nam->get(req);
    connect(m_reply, &QNetworkReply::finished, this,
            [this]() { onSigReply(m_reply); });
}

void Updater::onSigReply(QNetworkReply* reply) {
    reply->deleteLater();
    m_reply = nullptr;
    m_state = State::Idle;

    if (reply->error() != QNetworkReply::NoError) {
        emit checkError(tr("Network error: %1").arg(reply->errorString()));
        return;
    }

    QByteArray sig = reply->readAll();

    if (!verifySignature(m_manifest, sig)) {
        emit checkError(tr("Signature verification failed — update rejected."));
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(m_manifest);
    if (!doc.isObject()) {
        emit checkError(tr("Malformed update manifest."));
        return;
    }
    QJsonObject obj = doc.object();
    m_version  = obj.value("version").toString();
    m_sha256   = obj.value("sha256").toString();
    m_filename = obj.value("filename").toString();

    if (m_version.isEmpty() || m_sha256.isEmpty() || m_filename.isEmpty()) {
        emit checkError(tr("Incomplete update manifest."));
        return;
    }

    QString current = QString(APP_VERSION);
    if (QVersionNumber::fromString(m_version) > QVersionNumber::fromString(current))
        emit updateAvailable(m_version);
    else
        emit upToDate();
}

void Updater::startDownload() {
    if (m_state != State::Idle || m_version.isEmpty()) return;

    QString appImagePath = QString::fromLocal8Bit(qgetenv("APPIMAGE"));
    if (appImagePath.isEmpty()) {
        emit downloadError(tr("APPIMAGE path not found."));
        return;
    }

    // Stage the download in the same directory to guarantee same filesystem.
    m_tmpPath = QFileInfo(appImagePath).absolutePath() + "/SocketSpy-update.AppImage.tmp";

    QString downloadUrl =
        QString("https://github.com/Poisson48/SocketSpy/releases/download/v%1/%2")
            .arg(m_version, m_filename);

    m_state = State::Downloading;
    QNetworkRequest req{QUrl(downloadUrl)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    m_reply = m_nam->get(req);

    connect(m_reply, &QNetworkReply::downloadProgress, this,
            &Updater::downloadProgress);
    connect(m_reply, &QNetworkReply::finished, this,
            [this]() { onAppImageReply(m_reply); });
}

void Updater::onAppImageReply(QNetworkReply* reply) {
    reply->deleteLater();
    m_reply = nullptr;
    m_state = State::Idle;

    if (reply->error() != QNetworkReply::NoError) {
        QFile::remove(m_tmpPath);
        emit downloadError(tr("Download failed: %1").arg(reply->errorString()));
        return;
    }

    QFile tmp(m_tmpPath);
    if (!tmp.open(QIODevice::WriteOnly)) {
        emit downloadError(tr("Cannot write to temporary file."));
        return;
    }
    tmp.write(reply->readAll());
    tmp.close();

    if (!verifySha256(m_tmpPath, m_sha256)) {
        QFile::remove(m_tmpPath);
        emit downloadError(tr("SHA-256 checksum mismatch — file corrupted."));
        return;
    }

    if (!atomicInstall(m_tmpPath)) {
        QFile::remove(m_tmpPath);
        emit downloadError(tr("Installation failed — check permissions."));
        return;
    }

    emit installReady();
}

// ── crypto ───────────────────────────────────────────────────────────────────

bool Updater::verifySignature(const QByteArray& data, const QByteArray& sig) const {
    BIO* bio = BIO_new_mem_buf(kPublicKeyPem, -1);
    EVP_PKEY* pkey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!pkey) return false;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    bool ok = false;
    if (EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, pkey) == 1) {
        ok = (EVP_DigestVerify(ctx,
                               reinterpret_cast<const unsigned char*>(sig.constData()),
                               static_cast<std::size_t>(sig.size()),
                               reinterpret_cast<const unsigned char*>(data.constData()),
                               static_cast<std::size_t>(data.size())) == 1);
    }
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    return ok;
}

bool Updater::verifySha256(const QString& path, const QString& expected) const {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&f);
    return hash.result().toHex().toLower() == expected.toLower().trimmed();
}

bool Updater::atomicInstall(const QString& tmpPath) const {
    QString appImagePath = QString::fromLocal8Bit(qgetenv("APPIMAGE"));
    if (appImagePath.isEmpty()) return false;

    QFile::Permissions exec =
        QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
        QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther;
    QFile::setPermissions(tmpPath, exec);

    // rename() is atomic and safe on Linux even over a running AppImage.
    if (QFile::rename(tmpPath, appImagePath)) return true;

    // Cross-device fallback: stage in same directory then rename.
    QString staged = appImagePath + ".new";
    if (!QFile::copy(tmpPath, staged)) return false;
    QFile::remove(tmpPath);
    QFile::setPermissions(staged, exec);
    return QFile::rename(staged, appImagePath);
}

}  // namespace socketspy::gui
