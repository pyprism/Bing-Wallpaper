#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QList>

class QNetworkAccessManager;

class BingClient : public QObject
{
    Q_OBJECT

public:
    struct ImageInfo
    {
        QString url;       // relative "/th?id=..." path from the API
        QString date;      // startdate, e.g. "20260704"
        QString copyright; // e.g. "Description (© Photographer)"
    };

    explicit BingClient(QObject *parent = nullptr);

    // Fetch the latest image for the configured market and update the wallpaper.
    void fetchAndUpdate();

    // Fetch metadata for the last `n` days (no download) — feeds the
    // "Previous Wallpapers" submenu.
    void fetchArchive(int n = 8);

    // Ensure `info`'s image is cached locally (downloading if needed), then
    // emit wallpaperReady(). Used for the main flow and for archive picks.
    void useImage(const ImageInfo &info);

    // Pure helpers with no I/O — exposed for unit testing.
    static QList<ImageInfo> parseApiResponse(const QByteArray &json);
    static QString buildImageUrl(const QString &urlField);
    static QString computeSaveDir();

    // computeSaveDir() plus ensuring the directory exists on disk.
    static QString saveDir();

signals:
    void wallpaperReady(const QString &path, const QString &copyright, const QString &date);
    void archiveReady(const QList<ImageInfo> &images);
    void errorOccurred(const QString &message);

private slots:
    void onApiReply();
    void onArchiveReply();
    void onImageReply();

private:
    QString apiUrl(int n) const;

    QNetworkAccessManager *m_manager;
};
