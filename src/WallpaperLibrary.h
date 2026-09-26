#pragma once

#include <QDate>
#include <QList>
#include <QString>
#include <QStringList>

namespace WallpaperLibrary
{
struct Entry
{
    QString path;       // absolute path to the .jpg
    QString baseName;   // filename without extension — stable id for settings/skip-list
    QDate date;
    QString market;
    QString title;
    QString copyright;
    qint64 size = 0;
};

// Lists every *.jpg in `dir`, reading each `<basename>.json` sidecar
// (BingClient::writeSidecar()) when present and falling back to the date
// parsed from a legacy bare "YYYYMMDD.jpg" name otherwise. Sorted newest
// date first. Files with no parseable date (stray non-ours noise) are
// skipped.
QList<Entry> scan(const QString &dir);

// Pure: given already-scanned entries, returns those strictly older than
// `cutoff` — the set prune() would remove for a given retention cutoff.
QList<Entry> entriesOlderThan(const QList<Entry> &entries, const QDate &cutoff);

qint64 totalSize(const QList<Entry> &entries);

// Removes one entry's image + sidecar. Tries the OS trash first when
// `useTrash` is true; falls back to a hard delete if that fails (or always,
// when `useTrash` is false — used by tests and by any environment where
// trashing isn't available). The caller is responsible for confirming with
// the user before calling this.
bool remove(const Entry &entry, bool useTrash = true);

// keepDays <= 0 means "forever" (a no-op) — this is the default so nothing
// is ever silently deleted unless the user opts into a retention limit.
// Otherwise removes every entry strictly older than `keepDays` days ago.
void prune(const QString &dir, int keepDays, bool useTrash = true);

// The "deleted by the user" skip-list (QSettings-backed) that stops
// auto-refresh from silently re-downloading an image the user just deleted
// from the Gallery. A manual "Refresh Now" ignores it.
bool isDeleted(const QString &baseName);
void markDeleted(const QString &baseName);

// Splits the description out of a Bing `copyright` string like
// "Description (© Photographer)" → "Description". Falls back to the whole
// string if it doesn't match that shape. Shared by `BingClient::writeSidecar`
// (writing a new sidecar) and `scan()` below (self-healing an older sidecar
// that was written with `title == copyright` by a since-fixed bug).
QString deriveTitle(const QString &copyright);

// Pure helpers behind scan()/the skip-list, exposed for unit testing.
QDate parseLegacyDate(const QString &baseName);
bool containsId(const QStringList &ids, const QString &baseName);
QStringList withId(const QStringList &ids, const QString &baseName);
}
