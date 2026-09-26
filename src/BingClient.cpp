#include "BingClient.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QUrl>

#include "WallpaperLibrary.h"

namespace {
const char *kBingApiBase = "https://www.bing.com/HPImageArchive.aspx?format=js&idx=0";
}

BingClient::BingClient(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
{
    m_manager->setTransferTimeout(30000);
}

QString BingClient::apiUrl(int n) const
{
    QSettings settings;
    const QString market = settings.value("market", "en-US").toString();
    return QStringLiteral("%1&n=%2&mkt=%3").arg(kBingApiBase).arg(n).arg(market);
}

QString BingClient::computeSaveDir()
{
#if defined(Q_OS_MAC)
    return QDir::homePath() + "/Library/Application Support/bing-wallpapers";
#elif defined(Q_OS_WIN)
    QString appData = qEnvironmentVariable("APPDATA");
    if (appData.isEmpty())
        appData = QDir::homePath() + "/AppData/Roaming";
    return appData + "/bing-wallpapers";
#else
    return QDir::homePath() + "/.local/share/bing-wallpapers";
#endif
}

QString BingClient::saveDir()
{
    // BING_WALLPAPER_DIR lets tests (and manual verification of delete/prune
    // logic) point at a scratch directory instead of the user's real,
    // possibly-populated save directory.
    const QString override = qEnvironmentVariable("BING_WALLPAPER_DIR");
    const QString dir = override.isEmpty() ? computeSaveDir() : override;
    QDir().mkpath(dir);
    return dir;
}

QList<BingClient::ImageInfo> BingClient::parseApiResponse(const QByteArray &json)
{
    QList<ImageInfo> result;
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    const QJsonArray images = doc.object().value("images").toArray();
    result.reserve(images.size());
    for (const QJsonValue &v : images) {
        const QJsonObject obj = v.toObject();
        ImageInfo info;
        info.url = obj.value("url").toString();
        info.date = obj.value("startdate").toString();
        info.copyright = obj.value("copyright").toString();
        result.append(info);
    }
    return result;
}

QString BingClient::imageId(const QString &urlField)
{
    // Extract the "OHR.Name_MKT1234567890" id out of
    // "/th?id=OHR.Name_EN-US1234567890_1920x1080.jpg&rf=...&pid=hp".
    static const QRegularExpression kIdRe(QStringLiteral("id=([^&]+?)_\\d+x\\d+\\.jpg"));
    const QRegularExpressionMatch match = kIdRe.match(urlField);
    if (match.hasMatch())
        return match.captured(1);
    return QString();
}

QString BingClient::buildImageUrl(const QString &urlField)
{
    // Bing's `url` field looks like "/th?id=OHR.Name_EN-US1234567890_1920x1080.jpg&rf=...&pid=hp" —
    // the resolution suffix is NOT at the end of the string, so it can't be
    // replaced in place. Truncating at "id=<...>" (before the resolution
    // suffix) and appending "_UHD.jpg" is the well-known trick to get the
    // highest-resolution variant; if the field doesn't match this shape,
    // fall back to the field as-is.
    static const QRegularExpression kIdPrefix(QStringLiteral("^(/th\\?id=[^&]+?)_\\d+x\\d+\\.jpg"));
    const QRegularExpressionMatch match = kIdPrefix.match(urlField);
    const QString field = match.hasMatch() ? match.captured(1) + QStringLiteral("_UHD.jpg") : urlField;
    return QStringLiteral("https://www.bing.com") + field;
}

QString BingClient::fallbackImageUrl(const QString &urlField)
{
    // The plain resolution Bing already gave us in the `url` field — used
    // when the UHD variant 404s (not every image/market has one).
    return QStringLiteral("https://www.bing.com") + urlField;
}

QString BingClient::sanitizeForFilename(const QString &s)
{
    QString out = s;
    static const QRegularExpression kUnsafe(QStringLiteral("[^A-Za-z0-9._-]"));
    out.replace(kUnsafe, QStringLiteral("_"));
    return out;
}

QString BingClient::fileBaseName(const QString &date, const QString &market, const QString &id)
{
    const QString safeId = id.isEmpty() ? QStringLiteral("img") : sanitizeForFilename(id);
    return QStringLiteral("%1_%2_%3").arg(date, sanitizeForFilename(market), safeId);
}

QString BingClient::expectedPath(const ImageInfo &info)
{
    QSettings settings;
    const QString market = settings.value("market", "en-US").toString();
    const QString base = fileBaseName(info.date, market, imageId(info.url));
    return saveDir() + "/" + base + ".jpg";
}

void BingClient::writeSidecar(const QString &imagePath, const QString &date, const QString &market,
                               const QString &copyright, const QString &sourceUrl)
{
    // Shared with WallpaperLibrary::scan()'s self-heal for a sidecar written
    // by an earlier, buggy version of this split (see WallpaperLibrary.h).
    const QString title = WallpaperLibrary::deriveTitle(copyright);

    QJsonObject obj;
    obj["date"] = date;
    obj["market"] = market;
    obj["title"] = title;
    obj["copyright"] = copyright;
    obj["sourceUrl"] = sourceUrl;

    const QString sidecarPath = QFileInfo(imagePath).path() + "/"
        + QFileInfo(imagePath).completeBaseName() + ".json";
    QFile file(sidecarPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

QString BingClient::readSidecarCopyright(const QString &imagePath)
{
    const QString sidecarPath = QFileInfo(imagePath).path() + "/"
        + QFileInfo(imagePath).completeBaseName() + ".json";
    QFile file(sidecarPath);
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
    return obj.value("copyright").toString();
}

bool BingClient::shouldApply(const QString &lastAppliedDate, const QString &fetchedDate, bool manual)
{
    if (manual)
        return true;
    if (lastAppliedDate.isEmpty())
        return true;
    return fetchedDate > lastAppliedDate;
}

void BingClient::fetchAndUpdate(bool manual)
{
    QNetworkRequest request{QUrl(apiUrl(1))};
    QNetworkReply *reply = m_manager->get(request);
    reply->setProperty("manual", manual);
    connect(reply, &QNetworkReply::finished, this, &BingClient::onApiReply);
}

void BingClient::fetchArchive(int n)
{
    QNetworkRequest request{QUrl(apiUrl(n))};
    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, &BingClient::onArchiveReply);
}

void BingClient::onApiReply()
{
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        return;
    reply->deleteLater();
    const bool manual = reply->property("manual").toBool();

    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred(QStringLiteral("Error fetching Bing API: %1").arg(reply->errorString()), manual);
        return;
    }

    const QList<ImageInfo> images = parseApiResponse(reply->readAll());
    if (images.isEmpty()) {
        emit errorOccurred(QStringLiteral("No images found"), manual);
        return;
    }

    useImageInternal(images.first(), /*gated=*/true, manual);
}

void BingClient::onArchiveReply()
{
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        return;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        // Always user-initiated: fetchArchive() only ever runs when the
        // "Previous Wallpapers" submenu is actually opened.
        emit errorOccurred(QStringLiteral("Error fetching archive: %1").arg(reply->errorString()), true);
        return;
    }

    emit archiveReady(parseApiResponse(reply->readAll()));
}

void BingClient::useImage(const ImageInfo &info)
{
    useImageInternal(info, /*gated=*/false, /*manual=*/false);
}

void BingClient::useImageInternal(const ImageInfo &info, bool gated, bool manual)
{
    if (gated) {
        // Record the fetch happened even if it turns out to be skipped or
        // not applied below, so the caller can keep "lastUpdate" honest.
        emit fetchCompleted(info.date);
    }

    const QString savePath = expectedPath(info);

    if (gated && !manual && WallpaperLibrary::isDeleted(QFileInfo(savePath).completeBaseName())) {
        // The user deleted this exact image from the Gallery; an automatic
        // refresh must not silently resurrect it.
        return;
    }

    if (QFile::exists(savePath)) {
        const QString copyright = [&]() {
            const QString sidecar = readSidecarCopyright(savePath);
            return sidecar.isEmpty() ? info.copyright : sidecar;
        }();
        emit wallpaperReady(savePath, copyright, info.date, gated, manual);
        return;
    }

    startDownload(info, savePath, gated, manual, /*isFallbackAttempt=*/false);
}

void BingClient::startDownload(const ImageInfo &info, const QString &savePath, bool gated, bool manual,
                                bool isFallbackAttempt)
{
    if (m_inFlightSavePaths.contains(savePath))
        return;
    m_inFlightSavePaths.insert(savePath);

    const QString url = isFallbackAttempt ? fallbackImageUrl(info.url) : buildImageUrl(info.url);
    QNetworkRequest request{QUrl(url)};
    QNetworkReply *reply = m_manager->get(request);
    // Carried on the reply itself (rather than a member) so concurrent
    // downloads — e.g. picking two archive entries in a row — don't clobber
    // each other's destination.
    reply->setProperty("savePath", savePath);
    reply->setProperty("copyright", info.copyright);
    reply->setProperty("date", info.date);
    reply->setProperty("sourceUrl", info.url);
    reply->setProperty("gated", gated);
    reply->setProperty("manual", manual);
    reply->setProperty("isFallbackAttempt", isFallbackAttempt);
    connect(reply, &QNetworkReply::finished, this, &BingClient::onImageReply);
}

void BingClient::onImageReply()
{
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        return;
    reply->deleteLater();

    const QString savePath = reply->property("savePath").toString();
    const QString copyright = reply->property("copyright").toString();
    const QString date = reply->property("date").toString();
    const QString sourceUrl = reply->property("sourceUrl").toString();
    const bool gated = reply->property("gated").toBool();
    const bool manual = reply->property("manual").toBool();
    const bool wasFallbackAttempt = reply->property("isFallbackAttempt").toBool();

    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool networkOk = reply->error() == QNetworkReply::NoError;
    const bool httpOk = httpStatus == 0 /* not HTTP, e.g. file:// in tests */ || httpStatus == 200;

    // Only a manual action (or an explicit archive/Gallery pick, which is
    // never gated) should ever surface a tray notification for a failure —
    // otherwise every background due-check tick that happens while offline
    // pops a warning bubble.
    const bool userInitiated = !gated || manual;

    QSettings settings;
    QString market = settings.value("market", "en-US").toString();

    if (!networkOk || !httpOk) {
        m_inFlightSavePaths.remove(savePath);
        if (!wasFallbackAttempt) {
            // The UHD variant may not exist for this image/market — retry
            // once with Bing's own resolution before giving up.
            ImageInfo info{sourceUrl, date, copyright};
            startDownload(info, savePath, gated, manual, /*isFallbackAttempt=*/true);
            return;
        }
        emit errorOccurred(QStringLiteral("Error downloading image (HTTP %1): %2")
                                .arg(httpStatus).arg(reply->errorString()), userInitiated);
        return;
    }

    const QByteArray data = reply->readAll();

    // Validate the bytes actually decode as an image before caching them
    // forever — a truncated download or an HTML error page must not become
    // a permanently-stuck "already cached" file.
    QBuffer buffer;
    buffer.setData(data);
    buffer.open(QIODevice::ReadOnly);
    const bool looksLikeImage = QImageReader(&buffer).canRead();

    if (!looksLikeImage) {
        m_inFlightSavePaths.remove(savePath);
        if (!wasFallbackAttempt) {
            ImageInfo info{sourceUrl, date, copyright};
            startDownload(info, savePath, gated, manual, /*isFallbackAttempt=*/true);
            return;
        }
        emit errorOccurred(QStringLiteral("Downloaded data for %1 is not a valid image").arg(savePath),
                           userInitiated);
        return;
    }

    QSaveFile file(savePath);
    if (!file.open(QIODevice::WriteOnly)) {
        m_inFlightSavePaths.remove(savePath);
        emit errorOccurred(QStringLiteral("Error creating file: %1").arg(savePath), userInitiated);
        return;
    }
    const qint64 written = file.write(data);
    if (written != data.size()) {
        file.cancelWriting();
        m_inFlightSavePaths.remove(savePath);
        emit errorOccurred(QStringLiteral("Error writing file: %1").arg(savePath), userInitiated);
        return;
    }
    if (!file.commit()) {
        m_inFlightSavePaths.remove(savePath);
        emit errorOccurred(QStringLiteral("Error committing file: %1").arg(savePath), userInitiated);
        return;
    }

    writeSidecar(savePath, date, market, copyright, sourceUrl);
    m_inFlightSavePaths.remove(savePath);

    emit wallpaperReady(savePath, copyright, date, gated, manual);
}
