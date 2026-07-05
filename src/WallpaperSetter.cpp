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

#if defined(Q_OS_MAC)

QString systemEventsScript(const QString &path)
{
    return QStringLiteral(
        "tell application \"System Events\" to tell every desktop to set picture to \"%1\"").arg(path);
}

QString finderScript(const QString &path)
{
    return QStringLiteral(
        "tell application \"Finder\" to set desktop picture to POSIX file \"%1\"").arg(path);
}

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

QString wallpaperStyleValue()
{
    return QStringLiteral("10"); // fill
}

bool setWallpaper(const QString &path)
{
    QSettings settings("HKEY_CURRENT_USER\\Control Panel\\Desktop", QSettings::NativeFormat);
    settings.setValue("WallpaperStyle", wallpaperStyleValue());
    settings.setValue("TileWallpaper", "0");

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

QList<QStringList> candidateCommands(const QString &path)
{
    const QString uri = QStringLiteral("file://") + path;
    return {
        {"gsettings", "set", "org.gnome.desktop.background", "picture-uri", uri},
        {"gsettings", "set", "org.gnome.desktop.background", "picture-uri-dark", uri},
        {"gsettings", "set", "org.cinnamon.desktop.background", "picture-uri", uri},
        {"gsettings", "set", "org.mate.background", "picture-filename", path},
        {"plasma-apply-wallpaperimage", path},
        {"xfconf-query", "-c", "xfce4-desktop", "-p", "/backdrop/screen0/monitor0/image-path", "-s", path},
        {"feh", "--bg-fill", path},
        {"swaymsg", "output", "*", "bg", path, "fill"},
    };
}

bool setWallpaper(const QString &path)
{
    for (const QStringList &cmd : candidateCommands(path)) {
        if (QProcess::execute(cmd.first(), cmd.mid(1)) == 0) {
            qInfo() << "Wallpaper set using:" << cmd.first();
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
