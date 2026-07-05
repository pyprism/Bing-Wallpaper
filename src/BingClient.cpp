#include "BingClient.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSettings>
#include <QUrl>

namespace {
const char *kBingApiBase = "https://www.bing.com/HPImageArchive.aspx?format=js&idx=0";
}

BingClient::BingClient(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
{
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
    const QString dir = computeSaveDir();
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

void BingClient::fetchAndUpdate()
{
    QNetworkRequest request{QUrl(apiUrl(1))};
    QNetworkReply *reply = m_manager->get(request);
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

    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred(QStringLiteral("Error fetching Bing API: %1").arg(reply->errorString()));
        return;
    }

    const QList<ImageInfo> images = parseApiResponse(reply->readAll());
    if (images.isEmpty()) {
        emit errorOccurred(QStringLiteral("No images found"));
        return;
    }

    useImage(images.first());
}

void BingClient::onArchiveReply()
{
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        return;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred(QStringLiteral("Error fetching archive: %1").arg(reply->errorString()));
        return;
    }

    emit archiveReady(parseApiResponse(reply->readAll()));
}

void BingClient::useImage(const ImageInfo &info)
{
    const QString savePath = saveDir() + "/" + info.date + ".jpg";

    if (QFile::exists(savePath)) {
        emit wallpaperReady(savePath, info.copyright, info.date);
        return;
    }

    QNetworkRequest request{QUrl(buildImageUrl(info.url))};
    QNetworkReply *reply = m_manager->get(request);
    // Carried on the reply itself (rather than a member) so concurrent
    // downloads — e.g. picking two archive entries in a row — don't clobber
    // each other's destination.
    reply->setProperty("savePath", savePath);
    reply->setProperty("copyright", info.copyright);
    reply->setProperty("date", info.date);
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

    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred(QStringLiteral("Error downloading image: %1").arg(reply->errorString()));
        return;
    }

    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly)) {
        emit errorOccurred(QStringLiteral("Error creating file: %1").arg(savePath));
        return;
    }
    file.write(reply->readAll());
    file.close();

    emit wallpaperReady(savePath, copyright, date);
}
