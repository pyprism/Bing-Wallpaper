#include "WallpaperSetter.h"

#include <QDesktopServices>
#include <QDir>
#include <QProcess>
#include <QUrl>
#include <QDebug>

#if defined(Q_OS_WIN)
#include <windows.h>
#include <QSettings>
#endif

namespace WallpaperSetter
{

// ---- macOS: AppleScript --------------------------------------------------

QString escapeAppleScriptString(const QString &s)
{
    QString out = s;
    out.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    out.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return out;
}

QString systemEventsScript(const QString &path)
{
    return QStringLiteral(
        "tell application \"System Events\" to tell every desktop to set picture to \"%1\"")
        .arg(escapeAppleScriptString(path));
}

QString finderScript(const QString &path)
{
    return QStringLiteral(
        "tell application \"Finder\" to set desktop picture to POSIX file \"%1\"")
        .arg(escapeAppleScriptString(path));
}

// ---- Windows --------------------------------------------------------------

QString wallpaperStyleValue()
{
    return QStringLiteral("10"); // fill
}

// ---- Linux/freedesktop ------------------------------------------------

QString encodeFileUri(const QString &path)
{
    return QUrl::fromLocalFile(path).toString(QUrl::FullyEncoded);
}

QList<QStringList> commandsForGroup(const QString &groupName, const QString &path)
{
    const QString uri = encodeFileUri(path);

    if (groupName == QStringLiteral("gnome")) {
        return {
            {"gsettings", "set", "org.gnome.desktop.background", "picture-uri", uri},
            {"gsettings", "set", "org.gnome.desktop.background", "picture-uri-dark", uri},
        };
    }
    if (groupName == QStringLiteral("cinnamon")) {
        return {
            {"gsettings", "set", "org.cinnamon.desktop.background", "picture-uri", uri},
        };
    }
    if (groupName == QStringLiteral("mate")) {
        return {
            {"gsettings", "set", "org.mate.background", "picture-filename", path},
        };
    }
    if (groupName == QStringLiteral("kde")) {
        return {
            {"plasma-apply-wallpaperimage", path},
        };
    }
    if (groupName == QStringLiteral("xfce")) {
        return {
            {"xfconf-query", "-c", "xfce4-desktop", "-p",
             "/backdrop/screen0/monitor0/image-path", "-s", path},
        };
    }
    if (groupName == QStringLiteral("sway")) {
        return {
            {"swaymsg", "output", "*", "bg", path, "fill"},
        };
    }
    if (groupName == QStringLiteral("generic")) {
        return {
            {"feh", "--bg-fill", path},
        };
    }
    return {};
}

QStringList desktopGroupOrder(const QString &xdgCurrentDesktop)
{
    // All known groups, in a reasonable default order.
    static const QStringList kAllGroups = {
        QStringLiteral("gnome"), QStringLiteral("cinnamon"), QStringLiteral("mate"),
        QStringLiteral("kde"), QStringLiteral("xfce"), QStringLiteral("sway"),
        QStringLiteral("generic"),
    };

    const QString desktop = xdgCurrentDesktop.toLower();
    QStringList detected;

    // $XDG_CURRENT_DESKTOP can list several colon-separated names (e.g.
    // "ubuntu:GNOME" or "X-Cinnamon"), so match by substring, not equality.
    auto matches = [&desktop](const char *needle) { return desktop.contains(QLatin1String(needle)); };

    if (matches("cinnamon"))
        detected << QStringLiteral("cinnamon");
    if (matches("gnome") || matches("unity"))
        detected << QStringLiteral("gnome");
    if (matches("mate"))
        detected << QStringLiteral("mate");
    if (matches("kde") || matches("plasma"))
        detected << QStringLiteral("kde");
    if (matches("xfce"))
        detected << QStringLiteral("xfce");
    if (matches("sway"))
        detected << QStringLiteral("sway");

    QStringList order = detected;
    for (const QString &g : kAllGroups) {
        if (!order.contains(g))
            order << g;
    }
    return order;
}

// ---- Per-OS setWallpaper() (the only part that actually executes anything) --

#if defined(Q_OS_MAC)

bool setWallpaper(const QString &path)
{
    if (QProcess::execute("osascript", {"-e", systemEventsScript(path)}) == 0) {
        qInfo() << "Wallpaper set using osascript (System Events)";
        return true;
    }

    if (QProcess::execute("osascript", {"-e", finderScript(path)}) == 0) {
        qInfo() << "Wallpaper set using osascript (Finder)";
        return true;
    }

    qWarning() << "No suitable wallpaper command found.";
    return false;
}

#elif defined(Q_OS_WIN)

bool setWallpaper(const QString &path)
{
    QSettings settings("HKEY_CURRENT_USER\\Control Panel\\Desktop", QSettings::NativeFormat);
    // Only force the "fill" style the first time we ever set a wallpaper, so
    // we don't keep overwriting the user's own Fit/Center/Stretch choice on
    // every refresh.
    QSettings appSettings;
    if (!appSettings.value("wallpaperStyleInitialized", false).toBool()) {
        settings.setValue("WallpaperStyle", wallpaperStyleValue());
        settings.setValue("TileWallpaper", "0");
        appSettings.setValue("wallpaperStyleInitialized", true);
    }

    const std::wstring wpath = QDir::toNativeSeparators(path).toStdWString();
    if (!SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, const_cast<wchar_t *>(wpath.c_str()),
                                SPIF_UPDATEINIFILE | SPIF_SENDCHANGE)) {
        qWarning() << "Error setting wallpaper via SystemParametersInfoW";
        return false;
    }
    qInfo() << "Wallpaper set using SystemParametersInfoW";
    return true;
}

#else // Linux and other freedesktop-ish platforms

bool setWallpaper(const QString &path)
{
    const QString xdg = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
    for (const QString &groupName : desktopGroupOrder(xdg)) {
        const QList<QStringList> cmds = commandsForGroup(groupName, path);
        bool anySucceeded = false;
        for (const QStringList &cmd : cmds) {
            if (cmd.isEmpty())
                continue;
            if (QProcess::execute(cmd.first(), cmd.mid(1)) == 0)
                anySucceeded = true;
        }
        if (anySucceeded) {
            qInfo() << "Wallpaper set using group:" << groupName;
            return true;
        }
    }

    qWarning() << "No suitable wallpaper command found.";
    return false;
}

#endif

void openDir(const QString &dir)
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

}
