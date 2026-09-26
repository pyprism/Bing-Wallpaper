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

// Pure helpers with no execution — exposed for unit testing on any OS (not
// just the one that runs them), so the logic itself is actually covered by
// ctest instead of being "written, unverified".

// macOS: AppleScript snippets, and escaping for the path they embed.
QString escapeAppleScriptString(const QString &s);
QString systemEventsScript(const QString &path);
QString finderScript(const QString &path);

// Windows: the WallpaperStyle registry value for "fill".
QString wallpaperStyleValue();

// Linux/freedesktop: one command group per desktop environment. Every
// command in the winning group is run (not just the first), so e.g. GNOME's
// light *and* dark picture-uri keys both get set.
QList<QStringList> commandsForGroup(const QString &groupName, const QString &path);

// Order of group names to try, given $XDG_CURRENT_DESKTOP (may contain
// multiple colon-separated names, e.g. "ubuntu:GNOME"). The detected desktop
// is tried first; the rest follow as a best-effort fallback.
QStringList desktopGroupOrder(const QString &xdgCurrentDesktop);

// file:// URI for a local path, percent-encoded so spaces/'#'/etc. survive
// being passed to `gsettings`.
QString encodeFileUri(const QString &path);
}
