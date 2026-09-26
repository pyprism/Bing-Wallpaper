#include "Installer.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QDebug>

namespace Installer
{

QString xmlEscape(const QString &s)
{
    QString out = s;
    out.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    out.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    out.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    out.replace(QLatin1Char('"'), QStringLiteral("&quot;"));
    out.replace(QLatin1Char('\''), QStringLiteral("&apos;"));
    return out;
}

bool isUnderApplicationsFolder(const QString &execPath)
{
    return execPath.startsWith(QStringLiteral("/Applications/"))
        || execPath.startsWith(QDir::homePath() + QStringLiteral("/Applications/"));
}

QString quotedRegistryPath(const QString &path)
{
    QString native = QDir::toNativeSeparators(path);
    if (!native.startsWith(QLatin1Char('"')))
        native = QStringLiteral("\"%1\"").arg(native);
    return native;
}

namespace {

#if defined(Q_OS_MAC)

// Returns the executable path this Installer.cpp should reason about for the
// current platform (there's only one candidate on macOS/Windows; Linux has
// the AppImage wrinkle handled in execPathForAutostart() below).
QString currentExecPath()
{
    return QCoreApplication::applicationFilePath();
}

void applyAutostart(bool enabled, const QString &execPath)
{
    const QString launchAgentsDir = QDir::homePath() + "/Library/LaunchAgents";
    const QString plistPath = launchAgentsDir + "/com.bing-wallpaper.plist";

    // Deliberately no `launchctl` calls anywhere in this function. The
    // plist has RunAtLoad=true, so `launchctl bootstrap` on it starts a
    // *second* copy of the app immediately — which can't take the
    // single-instance lock, so it just signals the already-running instance
    // and exits, but still means every fresh install / every "Start at
    // Login" toggle pops the Gallery window unprompted. And `launchctl
    // bootout` on a job whose process is this very one (e.g. the app was
    // itself launched by launchd at login, and the user unchecks "Start at
    // Login" while it's running) sends it SIGTERM — killing the app that's
    // asking to disable its own autostart. Writing/removing the plist file
    // is enough either way; it simply takes effect at the next login.
    if (!enabled) {
        QFile::remove(plistPath);
        return;
    }

    if (!isUnderApplicationsFolder(execPath)) {
        // Registering autostart against a path under /Volumes (mounted .dmg)
        // or ~/Downloads would point at something that disappears later.
        qWarning() << "Not registering autostart: not running from /Applications:" << execPath;
        return;
    }

    QDir().mkpath(launchAgentsDir);
    const QString plistContent = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\">\n<dict>\n"
        "\t<key>Label</key>\n\t<string>com.bing-wallpaper</string>\n"
        "\t<key>ProgramArguments</key>\n\t<array>\n\t\t<string>%1</string>\n\t</array>\n"
        "\t<key>RunAtLoad</key>\n\t<true/>\n"
        "\t<key>KeepAlive</key>\n\t<false/>\n"
        "</dict>\n</plist>\n").arg(xmlEscape(execPath));

    QString existingContent;
    QFile existing(plistPath);
    if (existing.exists() && existing.open(QIODevice::ReadOnly)) {
        existingContent = QString::fromUtf8(existing.readAll());
        existing.close();
    }
    if (existingContent == plistContent) {
        return; // already up to date — nothing to write
    }

    QFile file(plistPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(plistContent.toUtf8());
        file.close();
    }
    // No `launchctl bootstrap` here — see the comment at the top of this
    // function. It takes effect at the next login.
}

#elif defined(Q_OS_WIN)

QString currentExecPath()
{
    return QCoreApplication::applicationFilePath();
}

void applyAutostart(bool enabled, const QString &execPath)
{
    QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                  QSettings::NativeFormat);
    if (enabled)
        reg.setValue("BingWallpaper", quotedRegistryPath(execPath));
    else
        reg.remove("BingWallpaper");
}

#else

// Running from an AppImage means QCoreApplication::applicationFilePath()
// points at a `/tmp/.mount_*` squashfs mount that's unique to *this* run and
// gone as soon as it exits — useless for both a self-copy and an autostart
// Exec= line. $APPIMAGE is the stable path to the .AppImage file itself.
QString currentExecPath()
{
    const QString appImage = qEnvironmentVariable("APPIMAGE");
    if (!appImage.isEmpty())
        return appImage;
    return QCoreApplication::applicationFilePath();
}

void applyAutostart(bool enabled, const QString &execPath)
{
    const QString autostartDir = QDir::homePath() + "/.config/autostart";
    const QString desktopFile = autostartDir + "/bing-wallpaper.desktop";

    if (!enabled) {
        QFile::remove(desktopFile);
        return;
    }

    QDir().mkpath(autostartDir);
    const QString content = QStringLiteral(
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Exec=%1\n"
        "Hidden=false\n"
        "NoDisplay=true\n"
        "X-GNOME-Autostart-enabled=true\n"
        "Name=Bing Wallpaper\n"
        "Comment=Daily Bing Wallpaper\n").arg(execPath);

    QFile file(desktopFile);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(content.toUtf8());
        file.close();
    }
}

#endif

} // namespace

bool ensureInstalled()
{
#if defined(Q_OS_MAC)
    // Installed via drag-to-Applications from the .dmg (Phase 7) rather than a
    // self-copy — nothing to do here beyond what syncAutostart() handles.
    return false;

#elif defined(Q_OS_WIN)
    // Windows packages are installed by Inno Setup. A Qt deployment cannot be
    // self-copied as a single executable because the adjacent Qt DLLs and
    // plugins, especially platforms/qwindows.dll, must move with it.
    return false;

#else
    const QString execPath = QCoreApplication::applicationFilePath();

    // Running from an AppImage: there is nothing sane to self-copy (the
    // mount path is ephemeral), so leave it where it is. syncAutostart()
    // points Exec= at $APPIMAGE instead of this transient path.
    if (!qEnvironmentVariable("APPIMAGE").isEmpty())
        return false;

    // Running straight out of a build directory (dev workflow) — don't
    // install a copy of a debug/dev build to ~/.local/bin.
    const QFileInfo selfInfo(execPath);
    if (QFile::exists(selfInfo.absolutePath() + "/CMakeCache.txt"))
        return false;

    const QString localBin = QDir::homePath() + "/.local/bin";
    QDir().mkpath(localBin);
    const QString targetPath = localBin + "/bing-wallpaper";

    if (execPath == targetPath)
        return false;

    QFile::remove(targetPath);
    if (QFile::copy(execPath, targetPath)) {
        QFile::setPermissions(targetPath,
            QFile::permissions(targetPath) | QFileDevice::ExeOwner | QFileDevice::ExeGroup | QFileDevice::ExeOther);
        qInfo() << "Installed to" << targetPath;
        QProcess::startDetached(targetPath, {});
        return true; // caller unlocks and exits
    }
    qWarning() << "Failed to install to" << targetPath << "- continuing from" << execPath;
    return false;
#endif
}

void syncAutostart()
{
    QSettings settings;
    const bool enabled = settings.value("autostart/enabled", true).toBool();
    applyAutostart(enabled, currentExecPath());
}

bool isAutostartEnabled()
{
    QSettings settings;
    return settings.value("autostart/enabled", true).toBool();
}

void setAutostartEnabled(bool enabled)
{
    QSettings settings;
    settings.setValue("autostart/enabled", enabled);
    applyAutostart(enabled, currentExecPath());
}

} // namespace Installer
