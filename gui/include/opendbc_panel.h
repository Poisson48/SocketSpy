#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QListWidget>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonArray>
#include <QString>
#include <QStringList>

namespace socketspy::gui {

class OpenDbcPanel : public QWidget {
    Q_OBJECT

public:
    explicit OpenDbcPanel(QWidget* parent = nullptr);

signals:
    void dbcFileSelected(const QString& path);

private slots:
    void onDownload();
    void onSearchChanged(const QString& text);
    void onLoadSelected();
    void onListingReady(QNetworkReply* reply);
    void fetchNextFile();

private:
    void setupUi();
    void loadFromCache();
    void updateCacheStatus();
    void applyFilter(const QString& text);
    void downloadFile(const QString& name, const QString& url);

    QLineEdit*             m_search{nullptr};
    QPushButton*           m_downloadBtn{nullptr};
    QProgressBar*          m_progress{nullptr};
    QListWidget*           m_list{nullptr};
    QPushButton*           m_loadBtn{nullptr};
    QLabel*                m_cacheLabel{nullptr};

    QNetworkAccessManager* m_nam{nullptr};
    QString                m_cacheDir;

    // Download queue state
    struct FileEntry { QString name; QString url; };
    QList<FileEntry>       m_queue;
    int                    m_totalFiles{0};
    int                    m_doneFiles{0};
    int                    m_activeDownloads{0};
    static constexpr int   kMaxParallel = 5;
    QStringList            m_allNames;   // unfiltered list
};

} // namespace socketspy::gui
