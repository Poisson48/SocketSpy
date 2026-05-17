#include "opendbc_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QUrl>
#include <algorithm>

namespace socketspy::gui {

OpenDbcPanel::OpenDbcPanel(QWidget* parent) : QWidget(parent) {
    m_cacheDir = QStandardPaths::writableLocation(
                     QStandardPaths::AppConfigLocation) + "/opendbc/";
    m_nam = new QNetworkAccessManager(this);
    connect(m_nam, &QNetworkAccessManager::finished,
            this,  &OpenDbcPanel::onListingReady);
    setupUi();
    loadFromCache();
}

void OpenDbcPanel::setupUi() {
    auto* title = new QLabel(
        "<b>OpenDBC — Vehicle DBC Database (Comma.ai)</b>", this);

    // Search + Download row
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText("Filter by name…");
    m_downloadBtn = new QPushButton("Download / Update", this);

    auto* topRow = new QHBoxLayout;
    topRow->addWidget(m_search, 1);
    topRow->addWidget(m_downloadBtn);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setVisible(false);

    m_list = new QListWidget(this);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);

    m_loadBtn = new QPushButton("Load selected DBC", this);
    m_loadBtn->setEnabled(false);
    m_loadBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_cacheLabel = new QLabel(this);
    m_cacheLabel->setStyleSheet("color: #6b7280; font-size: 11px;");
    updateCacheStatus();

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    layout->addWidget(title);
    layout->addLayout(topRow);
    layout->addWidget(m_progress);
    layout->addWidget(m_list, 1);
    layout->addWidget(m_loadBtn);
    layout->addWidget(m_cacheLabel);

    connect(m_downloadBtn, &QPushButton::clicked,
            this, &OpenDbcPanel::onDownload);
    connect(m_search, &QLineEdit::textChanged,
            this, &OpenDbcPanel::onSearchChanged);
    connect(m_loadBtn, &QPushButton::clicked,
            this, &OpenDbcPanel::onLoadSelected);
    connect(m_list, &QListWidget::itemSelectionChanged, this, [this]() {
        m_loadBtn->setEnabled(m_list->currentRow() >= 0);
    });
}

// ---------------------------------------------------------------------------
// Cache helpers
// ---------------------------------------------------------------------------

void OpenDbcPanel::loadFromCache() {
    QDir dir(m_cacheDir);
    if (!dir.exists()) return;

    const QStringList files = dir.entryList({"*.dbc"}, QDir::Files, QDir::Name);
    m_allNames = files;
    applyFilter(m_search ? m_search->text() : QString{});
    updateCacheStatus();
}

void OpenDbcPanel::updateCacheStatus() {
    QDir dir(m_cacheDir);
    int count = dir.exists()
        ? dir.entryList({"*.dbc"}, QDir::Files).size()
        : 0;
    if (m_cacheLabel)
        m_cacheLabel->setText(
            QString("Cache: %1 — %2 file(s)").arg(m_cacheDir).arg(count));
}

void OpenDbcPanel::applyFilter(const QString& text) {
    m_list->clear();
    for (const QString& name : std::as_const(m_allNames)) {
        if (text.isEmpty() || name.contains(text, Qt::CaseInsensitive))
            m_list->addItem(name);
    }
    m_loadBtn->setEnabled(false);
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void OpenDbcPanel::onSearchChanged(const QString& text) {
    applyFilter(text);
}

void OpenDbcPanel::onLoadSelected() {
    QListWidgetItem* item = m_list->currentItem();
    if (!item) return;
    const QString path = m_cacheDir + item->text();
    emit dbcFileSelected(path);
}

// ---------------------------------------------------------------------------
// Download logic
// ---------------------------------------------------------------------------

void OpenDbcPanel::onDownload() {
    m_downloadBtn->setEnabled(false);
    m_progress->setVisible(true);
    m_progress->setValue(0);

    QDir().mkpath(m_cacheDir);

    // Use the Git Trees API with recursive=1 to find ALL .dbc files
    // regardless of how deep they are in the repo directory tree.
    QNetworkRequest req(QUrl(
        "https://api.github.com/repos/commaai/opendbc/git/trees/HEAD?recursive=1"));
    req.setRawHeader("Accept", "application/vnd.github.v3+json");
    req.setRawHeader("User-Agent", "SocketSpy");
    req.setAttribute(QNetworkRequest::User, QVariant("listing"));
    m_nam->get(req);
}

void OpenDbcPanel::onListingReady(QNetworkReply* reply) {
    reply->deleteLater();

    const QVariant tag = reply->request().attribute(QNetworkRequest::User);

    if (tag.toString() == "listing") {
        // --- Handle recursive tree listing ---
        if (reply->error() != QNetworkReply::NoError) {
            QMessageBox::warning(this, "OpenDBC",
                "Network error: " + reply->errorString());
            m_downloadBtn->setEnabled(true);
            m_progress->setVisible(false);
            return;
        }
        const QByteArray data = reply->readAll();
        const QJsonObject root = QJsonDocument::fromJson(data).object();
        const QJsonArray tree  = root["tree"].toArray();

        m_queue.clear();
        m_doneFiles = 0;
        m_activeDownloads = 0;

        // tree entries: { "path": "opendbc/dbc/toyota_camry.dbc", "type": "blob", ... }
        for (const QJsonValue& val : tree) {
            QJsonObject obj = val.toObject();
            if (obj["type"].toString() != "blob") continue;
            const QString path = obj["path"].toString();
            if (!path.endsWith(".dbc", Qt::CaseInsensitive)) continue;
            // Use the flat filename as the local cache name (strip path)
            const QString name = path.mid(path.lastIndexOf('/') + 1);
            // Raw download URL
            const QString url = "https://raw.githubusercontent.com/commaai/opendbc/HEAD/" + path;
            m_queue.append({name, url});
        }

        m_totalFiles = m_queue.size();
        if (m_totalFiles == 0) {
            m_downloadBtn->setEnabled(true);
            m_progress->setVisible(false);
            return;
        }

        // Start up to kMaxParallel downloads
        for (int i = 0; i < kMaxParallel && !m_queue.isEmpty(); ++i)
            fetchNextFile();
    } else {
        // --- Handle individual file reply ---
        const QString name = tag.toString();
        if (reply->error() == QNetworkReply::NoError) {
            QFile f(m_cacheDir + name);
            if (f.open(QIODevice::WriteOnly)) {
                f.write(reply->readAll());
                f.close();
            }
        }

        ++m_doneFiles;
        --m_activeDownloads;

        const int pct = m_totalFiles > 0
            ? (m_doneFiles * 100 / m_totalFiles)
            : 100;
        m_progress->setValue(pct);

        if (!m_queue.isEmpty()) {
            fetchNextFile();
        } else if (m_activeDownloads == 0) {
            // All done
            m_downloadBtn->setEnabled(true);
            m_progress->setValue(100);
            loadFromCache();
            QMessageBox::information(this, "OpenDBC",
                QString("Done — %1 file(s) downloaded.").arg(m_doneFiles));
        }
    }
}

void OpenDbcPanel::fetchNextFile() {
    if (m_queue.isEmpty()) return;
    const FileEntry entry = m_queue.takeFirst();
    ++m_activeDownloads;

    QNetworkRequest req(QUrl(entry.url));
    req.setRawHeader("User-Agent", "SocketSpy");
    // Use the file name as tag so onListingReady knows this is a file reply
    req.setAttribute(QNetworkRequest::User, QVariant(entry.name));
    m_nam->get(req);
}

} // namespace socketspy::gui
