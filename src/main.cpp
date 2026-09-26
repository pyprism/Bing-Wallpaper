#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QSettings>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTimer>

#include "Installer.h"
#include "TrayController.h"

namespace {

// Scopes the lock file and local-socket names to the current user, so two
// different accounts on the same machine (sharing /tmp on Linux) don't
// collide over a single-instance guard that was never meant to be
// system-wide.
QString instanceKey()
{
    QString user = qEnvironmentVariable("USER");
    if (user.isEmpty())
        user = qEnvironmentVariable("USERNAME"); // Windows
    if (user.isEmpty())
        user = QStringLiteral("unknown");
    return QStringLiteral("bing-wallpaper-") + user;
}

}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);
    QCoreApplication::setOrganizationName(QStringLiteral("bing-wallpaper"));
    QCoreApplication::setApplicationName(QStringLiteral("BingWallpaper"));
#ifdef APP_VERSION
    QCoreApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
#endif

    {
        QSettings settings;
        if (!settings.contains("refreshIntervalHours"))
            settings.setValue("refreshIntervalHours", 5);
        if (!settings.contains("market"))
            settings.setValue("market", QStringLiteral("en-US"));
        if (!settings.contains("autostart/enabled"))
            settings.setValue("autostart/enabled", true);
    }

    // Single-instance guard, taken *before* any install/autostart side
    // effects so a second launch can't race the first into overwriting the
    // installed binary or re-registering autostart. QLockFile records the
    // owning PID and discards the lock itself if that process is no longer
    // running, so a crash doesn't permanently block future launches the way
    // a leaked QSharedMemory would.
    //
    // RuntimeLocation (falling back to TempLocation) plus a per-user key
    // avoids both a stale /tmp collision between users and the previous
    // hardcoded "bing-wallpaper.lock" name.
    QString lockDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (lockDir.isEmpty())
        lockDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString key = instanceKey();
    const QString lockPath = QDir(lockDir).filePath(key + QStringLiteral(".lock"));
    static QLockFile instanceLock(lockPath);

    if (!instanceLock.tryLock(100)) {
        // Another instance is already running — ask it to open the Gallery
        // window (so clicking the app icon a second time does *something*
        // visible) instead of silently doing nothing.
        QLocalSocket socket;
        socket.connectToServer(key);
        if (socket.waitForConnected(200)) {
            socket.write("show");
            socket.waitForBytesWritten(200);
            socket.disconnectFromServer();
        }
        return 0;
    }

    if (Installer::ensureInstalled()) {
        // A newer/relocated copy was launched to replace this process.
        // Release the lock explicitly rather than relying on process exit,
        // so the just-launched child doesn't have to wait out tryLock()'s
        // retry window to acquire it.
        instanceLock.unlock();
        return 0;
    }
    Installer::syncAutostart();

    TrayController tray;

    // Second-instance handshake: any later launch that lost the lock above
    // connects here and sends "show".
    QLocalServer localServer;
    QLocalServer::removeServer(key); // clear a stale socket left by a crash
    if (localServer.listen(key)) {
        QObject::connect(&localServer, &QLocalServer::newConnection, &tray, [&localServer, &tray]() {
            while (QLocalSocket *client = localServer.nextPendingConnection()) {
                QObject::connect(client, &QLocalSocket::disconnected, client, &QObject::deleteLater);
                QObject::connect(client, &QLocalSocket::readyRead, &tray, [client, &tray]() {
                    if (client->readAll().contains("show"))
                        tray.openGallery();
                });
            }
        });
    }

    const bool hasTray = QSystemTrayIcon::isSystemTrayAvailable();
    if (!hasTray) {
        qWarning("No system tray detected — opening the Gallery window as a fallback."
                 " (On GNOME, installing an AppIndicator extension restores the tray icon.)");
    }

    tray.show();
    tray.maybeShowFirstRunNotice();
    if (!hasTray)
        tray.openGallery();

    // Gated (manual=false), like the due-check: a plain restart must catch
    // up to a genuinely new day's image, but must not steamroll a manual
    // archive/Gallery pick from the previous session the way an
    // unconditional "always apply" launch-time fetch would.
    tray.triggerUpdate(false);

    // A due-check every 15 minutes (rather than a fixed-interval timer) lets
    // the configured refresh interval change at runtime, and catches up on a
    // missed tick after the system sleeps through the QTimer's scheduled fire.
    QTimer dueCheckTimer;
    dueCheckTimer.setInterval(15 * 60 * 1000);
    QObject::connect(&dueCheckTimer, &QTimer::timeout, &tray, [&tray]() {
        QSettings settings;
        const qint64 intervalSecs = settings.value("refreshIntervalHours", 5).toInt() * 3600LL;
        const qint64 lastEpoch = settings.value("lastUpdate", 0).toLongLong();
        const qint64 nowEpoch = QDateTime::currentSecsSinceEpoch();
        if (lastEpoch == 0 || nowEpoch - lastEpoch >= intervalSecs) {
            tray.triggerUpdate(false);
        }
    });
    dueCheckTimer.start();

    return app.exec();
}
