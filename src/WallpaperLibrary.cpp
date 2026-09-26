#include "WallpaperLibrary.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

#include <algorithm>

namespace WallpaperLibrary
{

QString deriveTitle(const QString &copyright)
{
    const int marker = copyright.indexOf(QStringLiteral(" (©"));
    return marker > 0 ? copyright.left(marker) : copyright;
}

QDate parseLegacyDate(const QString &baseName)
{
    // Handles both the legacy bare "YYYYMMDD" filename and the new
    // "YYYYMMDD_<market>_<id>" scheme — both start with the date.
    const QString datePart = baseName.section(QLatin1Char('_'), 0, 0);
    return QDate::fromString(datePart, QStringLiteral("yyyyMMdd"));
}

QList<Entry> scan(const QString &dir)
{
    QList<Entry> result;
    QDir d(dir);
    if (!d.exists())
        return result;

    const QStringList files = d.entryList({QStringLiteral("*.jpg")}, QDir::Files);
    for (const QString &fileName : files) {
        Entry entry;
        entry.path = d.filePath(fileName);
        entry.baseName = QFileInfo(fileName).completeBaseName();
        entry.size = QFileInfo(entry.path).size();

        const QString sidecarPath = d.filePath(entry.baseName + QStringLiteral(".json"));
        QFile sidecar(sidecarPath);
        bool haveSidecar = false;
        if (sidecar.open(QIODevice::ReadOnly)) {
            const QJsonObject obj = QJsonDocument::fromJson(sidecar.readAll()).object();
            if (!obj.isEmpty()) {
                entry.date = QDate::fromString(obj.value("date").toString(), QStringLiteral("yyyyMMdd"));
                entry.market = obj.value("market").toString();
                entry.title = obj.value("title").toString();
                entry.copyright = obj.value("copyright").toString();
                // Self-heal a sidecar written by the once-buggy title split
                // in BingClient::writeSidecar() (fixed, but any sidecar it
                // already wrote is stuck with title == copyright on disk).
                if (!entry.copyright.isEmpty() && entry.title == entry.copyright)
                    entry.title = deriveTitle(entry.copyright);
                haveSidecar = true;
            }
        }
        if (!haveSidecar) {
            entry.date = parseLegacyDate(entry.baseName);
            entry.title = entry.date.isValid() ? entry.date.toString(Qt::ISODate) : entry.baseName;
        }

        if (!entry.date.isValid())
            continue; // not one of ours — skip stray files in the save dir

        result.append(entry);
    }

    std::sort(result.begin(), result.end(), [](const Entry &a, const Entry &b) {
        if (a.date != b.date)
            return a.date > b.date;
        return a.baseName > b.baseName;
    });
    return result;
}

QList<Entry> entriesOlderThan(const QList<Entry> &entries, const QDate &cutoff)
{
    QList<Entry> out;
    for (const Entry &e : entries) {
        if (e.date.isValid() && e.date < cutoff)
            out.append(e);
    }
    return out;
}

qint64 totalSize(const QList<Entry> &entries)
{
    qint64 total = 0;
    for (const Entry &e : entries)
        total += e.size;
    return total;
}

bool remove(const Entry &entry, bool useTrash)
{
    bool imageRemoved = false;
    if (useTrash)
        imageRemoved = QFile::moveToTrash(entry.path);
    if (!imageRemoved)
        imageRemoved = QFile::remove(entry.path);

    const QString sidecarPath = QFileInfo(entry.path).path() + QLatin1Char('/') + entry.baseName
        + QStringLiteral(".json");
    if (QFile::exists(sidecarPath)) {
        if (!(useTrash && QFile::moveToTrash(sidecarPath)))
            QFile::remove(sidecarPath);
    }

    return imageRemoved;
}

void prune(const QString &dir, int keepDays, bool useTrash)
{
    if (keepDays <= 0)
        return; // "forever" — the safe default, never deletes anything.

    const QDate cutoff = QDate::currentDate().addDays(-keepDays);
    const QList<Entry> entries = scan(dir);
    for (const Entry &e : entriesOlderThan(entries, cutoff))
        remove(e, useTrash);
}

bool containsId(const QStringList &ids, const QString &baseName)
{
    return ids.contains(baseName);
}

QStringList withId(const QStringList &ids, const QString &baseName)
{
    QStringList out = ids;
    if (!out.contains(baseName))
        out.append(baseName);
    return out;
}

bool isDeleted(const QString &baseName)
{
    QSettings settings;
    return containsId(settings.value("deletedImageIds").toStringList(), baseName);
}

void markDeleted(const QString &baseName)
{
    QSettings settings;
    settings.setValue("deletedImageIds",
                       withId(settings.value("deletedImageIds").toStringList(), baseName));
}

}
