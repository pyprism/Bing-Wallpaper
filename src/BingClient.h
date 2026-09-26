#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QList>
#include <QSet>

class QNetworkAccessManager;
class QNetworkReply;

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
    // `manual` distinguishes a user-initiated refresh (menu click, market
    // change) from an automatic due-check tick — see shouldApply().
    void fetchAndUpdate(bool manual = true);

    // Fetch metadata for the last `n` days (no download) — feeds the
    // "Previous Wallpapers" submenu.
    void fetchArchive(int n = 8);

    // Ensure `info`'s image is cached locally (downloading if needed), then
    // emit wallpaperReady() with gated=false (always-apply semantics) — used
    // for explicit picks (archive submenu, Gallery window).
    void useImage(const ImageInfo &info);

    // Pure helpers with no I/O — exposed for unit testing.
    static QList<ImageInfo> parseApiResponse(const QByteArray &json);
    static QString buildImageUrl(const QString &urlField);
    static QString fallbackImageUrl(const QString &urlField);
    static QString computeSaveDir();
    static QString imageId(const QString &urlField);
    static QString sanitizeForFilename(const QString &s);
    static QString fileBaseName(const QString &date, const QString &market, const QString &id);

    // computeSaveDir() plus ensuring the directory exists on disk.
    static QString saveDir();

    // Full save path (no side effects beyond saveDir()'s mkpath) for `info`
    // under `market` — used both by the download path (with the market the
    // in-flight request was actually made for, not whatever the setting
    // reads *now*) and by callers (e.g. the tray menu) that want to know
    // where an image would land without downloading it (there, the current
    // setting is the right thing to pass).
    static QString expectedPath(const ImageInfo &info, const QString &market);

    // Writes/reads the `<basename>.json` sidecar next to a saved image.
    static void writeSidecar(const QString &imagePath, const QString &date, const QString &market,
                              const QString &copyright, const QString &sourceUrl);
    static QString readSidecarCopyright(const QString &imagePath);

    // Pure decision: should a fetched "latest" image actually be applied to
    // the desktop? `manual` (explicit user action) always applies. Otherwise
    // it applies only if `fetchedDate` is newer than `lastAppliedDate`
    // (lexicographic compare is correct for YYYYMMDD), so an automatic
    // due-check tick doesn't re-apply — and re-notify about — an unchanged
    // image, and never reverts a manual/archive/Gallery pick.
    static bool shouldApply(const QString &lastAppliedDate, const QString &fetchedDate, bool manual);

signals:
    // `market`: the market this image was actually fetched/saved under —
    // captured once at request time and carried through, *not* re-read from
    // QSettings on completion (switching Market while a fetch is in flight
    // must not let the new market's setting get attached to the old
    // request's response — see the reply-property plumbing in .cpp).
    // `gated`: true for the fetchAndUpdate() "latest image" flow (subject to
    // shouldApply()); false for explicit useImage() picks (always apply).
    // `manual`: only meaningful when gated is true.
    void wallpaperReady(const QString &path, const QString &copyright, const QString &date,
                         const QString &market, bool gated, bool manual);
    // Emitted whenever a fetchAndUpdate() API call resolved successfully,
    // even if the image turned out to be skipped (user-deleted) or not
    // applied (shouldApply() said no). Lets the caller record "lastUpdate"
    // unconditionally so the 15-minute due-check doesn't keep hammering the
    // API forever once the interval has passed.
    void fetchCompleted(const QString &date);
    void archiveReady(const QList<ImageInfo> &images);
    // `userInitiated`: true for a manual "Refresh Now"/market-change/archive-
    // pick/Gallery request; false for a background due-check tick or the
    // gated startup catch-up fetch. Only user-initiated failures should ever
    // surface as a tray notification — otherwise a laptop that's offline
    // gets (or a login that races Wi-Fi coming up gets) a warning bubble
    // every 15 minutes.
    void errorOccurred(const QString &message, bool userInitiated);

private slots:
    void onApiReply();
    void onArchiveReply();
    void onImageReply();

private:
    static QString apiUrl(int n, const QString &market);
    void useImageInternal(const ImageInfo &info, bool gated, bool manual, const QString &market);
    void startDownload(const ImageInfo &info, const QString &savePath, const QString &market,
                        bool gated, bool manual, bool isFallbackAttempt);

    QNetworkAccessManager *m_manager;
    QSet<QString> m_inFlightSavePaths;
};
