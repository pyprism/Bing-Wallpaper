#pragma once

#include <QObject>
#include <QList>

#include "BingClient.h"

class QSystemTrayIcon;
class QMenu;

class TrayController : public QObject
{
    Q_OBJECT

public:
    explicit TrayController(QObject *parent = nullptr);
    ~TrayController() override;

    void show();

public slots:
    void triggerUpdate();

private slots:
    void onWallpaperReady(const QString &path, const QString &copyright, const QString &date);
    void onArchiveReady(const QList<BingClient::ImageInfo> &images);
    void onError(const QString &message);
    void browseSavedImages();
    void copyDescription();
    void toggleStartAtLogin(bool checked);

private:
    void buildMenu();
    void populateMarketMenu(QMenu *menu);
    void populateIntervalMenu(QMenu *menu);

    QSystemTrayIcon *m_trayIcon;
    QMenu *m_menu;
    QMenu *m_archiveMenu;
    BingClient *m_client;
    QString m_lastCopyright;
};
