#include "TrayController.h"
#include "WallpaperSetter.h"
#include "WallpaperLibrary.h"
#include "Installer.h"
#include "GalleryWindow.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QIcon>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QDebug>

namespace {

struct MarketOption
{
    const char *code;
    const char *label;
};

const MarketOption kMarkets[] = {
    {"en-US", "English (US)"},
    {"en-GB", "English (UK)"},
    {"en-CA", "English (Canada)"},
    {"en-AU", "English (Australia)"},
    {"de-DE", "Deutsch"},
    {"fr-FR", "Français"},
    {"fr-CA", "Français (Canada)"},
    {"es-ES", "Español"},
    {"it-IT", "Italiano"},
    {"ja-JP", "日本語"},
    {"zh-CN", "中文 (简体)"},
    {"pt-BR", "Português (Brasil)"},
};

struct IntervalOption
{
    int hours;
    const char *label;
};

const IntervalOption kIntervals[] = {
    {1, "Every hour"},
    {3, "Every 3 hours"},
    {5, "Every 5 hours"},
    {12, "Every 12 hours"},
    {24, "Once a day"},
};

struct RetentionOption
{
    int days;
    const char *label;
};

const RetentionOption kRetentions[] = {
    {0, "Keep forever"},
    {30, "Keep last 30 days"},
    {90, "Keep last 90 days"},
    {180, "Keep last 180 days"},
};

constexpr qint64 kArchiveCacheTtlSecs = 3600;

}

TrayController::TrayController(QObject *parent)
    : QObject(parent)
    , m_trayIcon(new QSystemTrayIcon(this))
    , m_menu(new QMenu())
    , m_archiveMenu(nullptr)
    , m_client(new BingClient(this))
{
    const QIcon icon(":/bing.png");
    m_trayIcon->setIcon(icon);
    m_trayIcon->setToolTip(QStringLiteral("Bing Wallpaper"));

    buildMenu();
    m_trayIcon->setContextMenu(m_menu);

    connect(m_client, &BingClient::wallpaperReady, this, &TrayController::onWallpaperReady);
    connect(m_client, &BingClient::fetchCompleted, this, &TrayController::onFetchCompleted);
    connect(m_client, &BingClient::archiveReady, this, &TrayController::onArchiveReady);
    connect(m_client, &BingClient::errorOccurred, this, &TrayController::onError);

    // Left-click opens the menu on macOS by platform convention, so only
    // wire double-click-to-open-Gallery on Windows/Linux.
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
#if !defined(Q_OS_MAC)
        if (reason == QSystemTrayIcon::DoubleClick)
            openGallery();
#else
        Q_UNUSED(reason);
#endif
    });

    restoreLastCopyrightFromDisk();
}

TrayController::~TrayController()
{
    delete m_menu;
}

void TrayController::restoreLastCopyrightFromDisk()
{
    QSettings settings;
    const QString path = settings.value("currentWallpaperPath").toString();
    if (!path.isEmpty())
        m_lastCopyright = BingClient::readSidecarCopyright(path);
}

void TrayController::buildMenu()
{
    QAction *refreshNow = m_menu->addAction(QStringLiteral("Refresh Now"));
    connect(refreshNow, &QAction::triggered, this, [this]() { triggerUpdate(true); });

    QAction *gallery = m_menu->addAction(QStringLiteral("Open Gallery…"));
    connect(gallery, &QAction::triggered, this, &TrayController::openGallery);

    QAction *copyDesc = m_menu->addAction(QStringLiteral("Copy Description"));
    connect(copyDesc, &QAction::triggered, this, &TrayController::copyDescription);

    m_archiveMenu = m_menu->addMenu(QStringLiteral("Previous Wallpapers"));
    connect(m_archiveMenu, &QMenu::aboutToShow, this, [this]() {
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        if (!m_archiveCache.isEmpty() && now - m_archiveCacheSecs < kArchiveCacheTtlSecs) {
            onArchiveReady(m_archiveCache);
            return;
        }
        m_archiveMenu->clear();
        QAction *loading = m_archiveMenu->addAction(QStringLiteral("Loading…"));
        loading->setEnabled(false);
        m_client->fetchArchive(8);
    });

    m_menu->addSeparator();

    QMenu *marketMenu = m_menu->addMenu(QStringLiteral("Market"));
    populateMarketMenu(marketMenu);

    QMenu *intervalMenu = m_menu->addMenu(QStringLiteral("Refresh Interval"));
    populateIntervalMenu(intervalMenu);

    QMenu *retentionMenu = m_menu->addMenu(QStringLiteral("Keep Wallpapers"));
    populateRetentionMenu(retentionMenu);

    QAction *startAtLogin = m_menu->addAction(QStringLiteral("Start at Login"));
    startAtLogin->setCheckable(true);
    startAtLogin->setChecked(Installer::isAutostartEnabled());
    connect(startAtLogin, &QAction::toggled, this, &TrayController::toggleStartAtLogin);

    m_menu->addSeparator();

    QAction *about = m_menu->addAction(QStringLiteral("About"));
    connect(about, &QAction::triggered, this, &TrayController::showAbout);

    m_menu->addSeparator();

    QAction *quit = m_menu->addAction(QStringLiteral("Quit"));
    connect(quit, &QAction::triggered, qApp, &QCoreApplication::quit);
}

void TrayController::populateMarketMenu(QMenu *menu)
{
    QSettings settings;
    const QString current = settings.value("market", "en-US").toString();
    auto *group = new QActionGroup(menu);
    group->setExclusive(true);

    for (const auto &opt : kMarkets) {
        const QString code = QString::fromLatin1(opt.code);
        QAction *action = menu->addAction(QString::fromUtf8(opt.label));
        action->setCheckable(true);
        action->setChecked(current == code);
        group->addAction(action);
        connect(action, &QAction::triggered, this, [this, code]() {
            QSettings s;
            s.setValue("market", code);
            triggerUpdate(true); // apply the new market immediately, not on the next tick
        });
    }
}

void TrayController::populateIntervalMenu(QMenu *menu)
{
    QSettings settings;
    const int current = settings.value("refreshIntervalHours", 5).toInt();
    auto *group = new QActionGroup(menu);
    group->setExclusive(true);

    for (const auto &opt : kIntervals) {
        QAction *action = menu->addAction(QString::fromLatin1(opt.label));
        action->setCheckable(true);
        action->setChecked(current == opt.hours);
        group->addAction(action);
        const int hours = opt.hours;
        connect(action, &QAction::triggered, this, [hours]() {
            QSettings s;
            s.setValue("refreshIntervalHours", hours);
        });
    }
}

void TrayController::populateRetentionMenu(QMenu *menu)
{
    QSettings settings;
    const int current = settings.value("retentionDays", 0).toInt();
    auto *group = new QActionGroup(menu);
    group->setExclusive(true);

    for (const auto &opt : kRetentions) {
        QAction *action = menu->addAction(QString::fromLatin1(opt.label));
        action->setCheckable(true);
        action->setChecked(current == opt.days);
        group->addAction(action);
        const int days = opt.days;
        connect(action, &QAction::triggered, this, [days]() {
            QSettings s;
            s.setValue("retentionDays", days);
        });
    }
}

void TrayController::show()
{
    m_trayIcon->show();
}

void TrayController::maybeShowFirstRunNotice()
{
    QSettings settings;
    if (settings.value("autostart/firstRunNoticeShown", false).toBool())
        return;
    settings.setValue("autostart/firstRunNoticeShown", true);
    if (Installer::isAutostartEnabled()) {
        notify(QStringLiteral("Bing Wallpaper"),
               QStringLiteral("Will start automatically at login — change this from the tray menu."));
    }
}

void TrayController::triggerUpdate(bool manual)
{
    m_client->fetchAndUpdate(manual);
}

void TrayController::openGallery()
{
    if (!m_gallery) {
        m_gallery = new GalleryWindow();
        connect(m_gallery, &GalleryWindow::refreshRequested, this, [this]() { triggerUpdate(true); });
        connect(m_gallery, &GalleryWindow::wallpaperApplied, this,
                &TrayController::onGalleryWallpaperApplied);
    }
    m_gallery->show();
    m_gallery->raise();
    m_gallery->activateWindow();
}

void TrayController::onGalleryWallpaperApplied(const QString &, const QString &copyright)
{
    m_lastCopyright = copyright;
}

void TrayController::notify(const QString &title, const QString &body)
{
    if (m_trayIcon->supportsMessages())
        m_trayIcon->showMessage(title, body, QSystemTrayIcon::Information, 5000);
}

void TrayController::notifyError(const QString &message)
{
    qWarning() << message;
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (now - m_lastErrorNotifySecs < 60)
        return; // rate-limited so a string of background failures doesn't spam the user
    m_lastErrorNotifySecs = now;
    if (m_trayIcon->supportsMessages())
        m_trayIcon->showMessage(QStringLiteral("Bing Wallpaper — Error"), message,
                                 QSystemTrayIcon::Warning, 5000);
}

void TrayController::onFetchCompleted(const QString &)
{
    // Recorded unconditionally — even when the fetched image is skipped or
    // not applied — so the 15-minute due-check timer doesn't treat "nothing
    // changed" as "never checked" and hammer the API every tick forever.
    QSettings settings;
    settings.setValue("lastUpdate", QDateTime::currentSecsSinceEpoch());
}

void TrayController::onWallpaperReady(const QString &path, const QString &copyright, const QString &date,
                                       const QString &market, bool gated, bool manual)
{
    QSettings settings;

    if (!gated) {
        // An explicit pick (archive submenu / Gallery window) — always
        // apply, and never touch lastAutoAppliedDate (an older archive date
        // must not make a later auto-refresh think nothing is new).
        if (!WallpaperSetter::setWallpaper(path)) {
            notifyError(QStringLiteral("Failed to set wallpaper."));
            return;
        }
        settings.setValue("currentWallpaperPath", path);
        m_lastCopyright = copyright;
        notify(QStringLiteral("Bing Wallpaper"), copyright.isEmpty() ? date : copyright);
        WallpaperLibrary::prune(BingClient::saveDir(), settings.value("retentionDays", 0).toInt());
        return;
    }

    // `market` here is the one this specific fetch was actually made for
    // (see BingClient::fetchAndUpdate/onImageReply) — not re-read from the
    // live setting, which could already reflect a market the user switched
    // to after this fetch started. That distinction matters exactly for
    // this key: using the live setting here could record today's date as
    // "auto-applied" under the *new* market from an image that was actually
    // fetched (and named/saved) under the *old* one.
    const QString key = QStringLiteral("lastAutoAppliedDate/%1").arg(market);
    const QString lastAppliedDate = settings.value(key).toString();

    if (!BingClient::shouldApply(lastAppliedDate, date, manual))
        return; // automatic tick, nothing new — stay silent, don't re-apply/re-notify

    if (!WallpaperSetter::setWallpaper(path)) {
        notifyError(QStringLiteral("Failed to set wallpaper."));
        return;
    }

    settings.setValue("currentWallpaperPath", path);
    settings.setValue(key, date);
    m_lastCopyright = copyright;

    const bool isNewImage = date != lastAppliedDate;
    if (manual || isNewImage) {
        notify(QStringLiteral("Bing Wallpaper"),
               isNewImage ? (copyright.isEmpty() ? date : copyright)
                          : QStringLiteral("Already up to date"));
    }

    WallpaperLibrary::prune(BingClient::saveDir(), settings.value("retentionDays", 0).toInt());
}

void TrayController::onArchiveReady(const QList<BingClient::ImageInfo> &images)
{
    if (!m_archiveMenu)
        return;

    m_archiveCache = images;
    m_archiveCacheSecs = QDateTime::currentSecsSinceEpoch();

    QSettings settings;
    const QString currentPath = settings.value("currentWallpaperPath").toString();
    // The current setting is the right thing here (unlike onWallpaperReady's
    // gated branch above): this is just "does this archive entry match
    // what's on screen right now", not tied to any specific in-flight fetch.
    const QString currentMarket = settings.value("market", "en-US").toString();

    m_archiveMenu->clear();
    for (const BingClient::ImageInfo &info : images) {
        QString label = info.copyright.isEmpty() ? info.date : info.copyright;
        if (label.size() > 60)
            label = label.left(57) + QStringLiteral("...");
        QAction *action = m_archiveMenu->addAction(label);
        if (BingClient::expectedPath(info, currentMarket) == currentPath) {
            action->setCheckable(true);
            action->setChecked(true);
        }
        connect(action, &QAction::triggered, m_client, [this, info]() { m_client->useImage(info); });
    }
}

void TrayController::onError(const QString &message, bool userInitiated)
{
    if (userInitiated) {
        notifyError(message); // rate-limited tray bubble, e.g. a manual "Refresh Now" that failed
    } else {
        // A background due-check tick (or the gated startup fetch) failing —
        // e.g. offline, or a login that races Wi-Fi coming up — must never
        // pop a bubble every 15 minutes. Log only.
        qWarning() << message;
    }
}

void TrayController::copyDescription()
{
    // A legacy (pre-sidecar) cached wallpaper has no stored copyright, so
    // m_lastCopyright can genuinely be empty — don't clobber whatever the
    // user already had on their clipboard with nothing.
    if (!m_lastCopyright.isEmpty())
        QApplication::clipboard()->setText(m_lastCopyright);
}

void TrayController::toggleStartAtLogin(bool checked)
{
    Installer::setAutostartEnabled(checked);
}

void TrayController::showAbout()
{
    const QString version = QCoreApplication::applicationVersion();
    auto *box = new QMessageBox(QMessageBox::NoIcon, QStringLiteral("About Bing Wallpaper"),
        QStringLiteral(
            "<h3>Bing Wallpaper%1</h3>"
            "<p>Sets your desktop wallpaper to the daily Bing image.</p>"
            "<p><a href=\"https://github.com/pyprism/Bing-Wallpaper\">"
            "github.com/pyprism/Bing-Wallpaper</a></p>")
            .arg(version.isEmpty() ? QString() : QStringLiteral(" v%1").arg(version)),
        QMessageBox::Ok, nullptr);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->show();
    box->raise();
    box->activateWindow();
}
