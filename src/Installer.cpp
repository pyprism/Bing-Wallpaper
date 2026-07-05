#include "Installer.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QProcess>
#include <QSettings>
#include <QDebug>

namespace Installer
{

namespace {

#if defined(Q_OS_MAC)

void applyAutostart(bool enabled, const QString &execPath)
{
    const QString launchAgentsDir = QDir::homePath() + "/Library/LaunchAgents";
    const QString plistPath = launchAgentsDir + "/com.bing-wallpaper.plist";

    if (!enabled) {
        QProcess::execute("launchctl", {"unload", plistPath});
        QFile::remove(plistPath);
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
        "</dict>\n</plist>\n").arg(execPath);

    QFile file(plistPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(plistContent.toUtf8());
        file.close();
    }
    QProcess::execute("launchctl", {"load", plistPath});
}

#elif defined(Q_OS_WIN)

void applyAutostart(bool enabled, const QString &execPath)
{
    QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                  QSettings::NativeFormat);
    if (enabled)
        reg.setValue("BingWallpaper", QDir::toNativeSeparators(execPath));
    else
        reg.remove("BingWallpaper");
}

#else

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

void ensureInstalled()
{
    const QString execPath = QCoreApplication::applicationFilePath();

#if defined(Q_OS_MAC)
    // Installed via drag-to-Applications from the .dmg (Phase 7) rather than a
    // self-copy — nothing to do here beyond what syncAutostart() handles.
    Q_UNUSED(execPath);

#elif defined(Q_OS_WIN)
    // Windows packages are installed by Inno Setup. A Qt deployment cannot be
    // self-copied as a single executable because the adjacent Qt DLLs and
    // plugins, especially platforms/qwindows.dll, must move with it.
    Q_UNUSED(execPath);

#else
    const QString localBin = QDir::homePath() + "/.local/bin";
    QDir().mkpath(localBin);
    const QString targetPath = localBin + "/bing-wallpaper";

    if (execPath != targetPath) {
        QFile::remove(targetPath);
        if (QFile::copy(execPath, targetPath)) {
            QFile::setPermissions(targetPath,
                QFile::permissions(targetPath) | QFileDevice::ExeOwner | QFileDevice::ExeGroup | QFileDevice::ExeOther);
            qInfo() << "Installed to" << targetPath;
            QProcess::startDetached(targetPath, {});
            std::exit(0);
        }
        qWarning() << "Failed to install to" << targetPath << "- continuing from" << execPath;
    }
#endif
}

void syncAutostart()
{
    QSettings settings;
    const bool enabled = settings.value("autostart/enabled", true).toBool();
    applyAutostart(enabled, QCoreApplication::applicationFilePath());
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
    applyAutostart(enabled, QCoreApplication::applicationFilePath());
}

} // namespace Installer
