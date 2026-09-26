#pragma once

#include <QObject>
#include <QList>
#include <QPointer>

#include "BingClient.h"

class QSystemTrayIcon;
class QMenu;
class GalleryWindow;

class TrayController : public QObject
{
    Q_OBJECT

public:
    explicit TrayController(QObject *parent = nullptr);
    ~TrayController() override;

    void show();

    // Shown once, the first time autostart is silently enabled by default.
    void maybeShowFirstRunNotice();

public slots:
    void triggerUpdate(bool manual = true);
    void openGallery();

private slots:
    void onWallpaperReady(const QString &path, const QString &copyright, const QString &date,
                           const QString &market, bool gated, bool manual);
    void onFetchCompleted(const QString &date);
    void onArchiveReady(const QList<BingClient::ImageInfo> &images);
    void onError(const QString &message, bool userInitiated);
    void copyDescription();
    void toggleStartAtLogin(bool checked);
    void showAbout();
    void onGalleryWallpaperApplied(const QString &path, const QString &copyright);

private:
    void buildMenu();
    void populateMarketMenu(QMenu *menu);
    void populateIntervalMenu(QMenu *menu);
    void populateRetentionMenu(QMenu *menu);
    void notify(const QString &title, const QString &body);
    void notifyError(const QString &message);
    void restoreLastCopyrightFromDisk();

    QSystemTrayIcon *m_trayIcon;
    QMenu *m_menu;
    QMenu *m_archiveMenu;
    BingClient *m_client;
    QString m_lastCopyright;
    qint64 m_lastErrorNotifySecs = 0;
    qint64 m_archiveCacheSecs = 0;
    QList<BingClient::ImageInfo> m_archiveCache;
    QPointer<GalleryWindow> m_gallery;
};
