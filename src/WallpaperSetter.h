#pragma once

#include <QString>
#include <QStringList>
#include <QList>

namespace WallpaperSetter
{
// Sets the desktop wallpaper to the image at `path`. Returns true on success.
bool setWallpaper(const QString &path);

// Opens `dir` in the platform's file manager.
void openDir(const QString &dir);

// Pure helpers with no execution — exposed for unit testing.
#if defined(Q_OS_MAC)
QString systemEventsScript(const QString &path);
QString finderScript(const QString &path);
#elif defined(Q_OS_WIN)
QString wallpaperStyleValue();
#else
QList<QStringList> candidateCommands(const QString &path);
#endif
}
