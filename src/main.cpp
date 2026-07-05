#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QLockFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

#include "Installer.h"
#include "TrayController.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);
    QCoreApplication::setOrganizationName(QStringLiteral("bing-wallpaper"));
    QCoreApplication::setApplicationName(QStringLiteral("BingWallpaper"));

    {
        QSettings settings;
        if (!settings.contains("refreshIntervalHours"))
            settings.setValue("refreshIntervalHours", 5);
        if (!settings.contains("market"))
            settings.setValue("market", QStringLiteral("en-US"));
        if (!settings.contains("autostart/enabled"))
            settings.setValue("autostart/enabled", true);
    }

    Installer::ensureInstalled();
    Installer::syncAutostart();

    // Single-instance guard. QLockFile records the owning PID and discards the
    // lock itself if that process is no longer running, so a crash doesn't
    // permanently block future launches the way a leaked QSharedMemory would.
    const QString lockPath = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                  .filePath(QStringLiteral("bing-wallpaper.lock"));
    static QLockFile instanceLock(lockPath);
    if (!instanceLock.tryLock(100)) {
        return 0;
    }

    TrayController tray;
    tray.show();
    tray.triggerUpdate();

    // A due-check every 15 minutes (rather than a fixed-interval timer) lets
    // the configured refresh interval change at runtime, and catches up on a
    // missed tick after the system sleeps through the QTimer's scheduled fire.
    QTimer dueCheckTimer;
    dueCheckTimer.setInterval(15 * 60 * 1000);
    QObject::connect(&dueCheckTimer, &QTimer::timeout, &tray, [&tray]() {
        QSettings settings;
        const qint64 intervalSecs = settings.value("refreshIntervalHours", 5).toInt() * 3600LL;
        const QDateTime last = settings.value("lastUpdate").toDateTime();
        if (!last.isValid() || last.secsTo(QDateTime::currentDateTimeUtc()) >= intervalSecs) {
            tray.triggerUpdate();
        }
    });
    dueCheckTimer.start();

    return app.exec();
}
