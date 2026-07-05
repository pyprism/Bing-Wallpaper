#include "TrayController.h"
#include "WallpaperSetter.h"
#include "Installer.h"

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
    connect(m_client, &BingClient::archiveReady, this, &TrayController::onArchiveReady);
    connect(m_client, &BingClient::errorOccurred, this, &TrayController::onError);
}

TrayController::~TrayController()
{
    delete m_menu;
}

void TrayController::buildMenu()
{
    QAction *setNow = m_menu->addAction(QStringLiteral("Set New Wallpaper Now"));
    connect(setNow, &QAction::triggered, this, &TrayController::triggerUpdate);

    QAction *browse = m_menu->addAction(QStringLiteral("Browse Saved Images"));
    connect(browse, &QAction::triggered, this, &TrayController::browseSavedImages);

    QAction *copyDesc = m_menu->addAction(QStringLiteral("Copy Description"));
    connect(copyDesc, &QAction::triggered, this, &TrayController::copyDescription);

    m_archiveMenu = m_menu->addMenu(QStringLiteral("Previous Wallpapers"));
    connect(m_archiveMenu, &QMenu::aboutToShow, m_client, [this]() { m_client->fetchArchive(8); });

    m_menu->addSeparator();

    QMenu *marketMenu = m_menu->addMenu(QStringLiteral("Market"));
    populateMarketMenu(marketMenu);

    QMenu *intervalMenu = m_menu->addMenu(QStringLiteral("Refresh Interval"));
    populateIntervalMenu(intervalMenu);

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
        QAction *action = menu->addAction(QStringLiteral("%1 (%2)").arg(QString::fromUtf8(opt.label), code));
        action->setCheckable(true);
        action->setChecked(current == code);
        group->addAction(action);
        connect(action, &QAction::triggered, this, [code]() {
            QSettings s;
            s.setValue("market", code);
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

void TrayController::show()
{
    m_trayIcon->show();
}

void TrayController::triggerUpdate()
{
    m_client->fetchAndUpdate();
}

void TrayController::onWallpaperReady(const QString &path, const QString &copyright, const QString &date)
{
    WallpaperSetter::setWallpaper(path);
    m_lastCopyright = copyright;

    QSettings settings;
    settings.setValue("lastUpdate", QDateTime::currentSecsSinceEpoch());

    if (m_trayIcon->supportsMessages()) {
        m_trayIcon->showMessage(QStringLiteral("Bing Wallpaper"),
                                 copyright.isEmpty() ? date : copyright,
                                 QSystemTrayIcon::Information, 5000);
    }
}

void TrayController::onArchiveReady(const QList<BingClient::ImageInfo> &images)
{
    if (!m_archiveMenu)
        return;

    m_archiveMenu->clear();
    for (const BingClient::ImageInfo &info : images) {
        QString label = info.copyright.isEmpty() ? info.date : info.copyright;
        if (label.size() > 60)
            label = label.left(57) + QStringLiteral("...");
        QAction *action = m_archiveMenu->addAction(label);
        connect(action, &QAction::triggered, m_client, [this, info]() { m_client->useImage(info); });
    }
}

void TrayController::onError(const QString &message)
{
    qWarning() << message;
}

void TrayController::browseSavedImages()
{
    WallpaperSetter::openDir(BingClient::saveDir());
}

void TrayController::copyDescription()
{
    QApplication::clipboard()->setText(m_lastCopyright);
}

void TrayController::toggleStartAtLogin(bool checked)
{
    Installer::setAutostartEnabled(checked);
}

void TrayController::showAbout()
{
    QMessageBox::about(nullptr, QStringLiteral("About Bing Wallpaper"),
        QStringLiteral(
            "<h3>Bing Wallpaper</h3>"
            "<p>Sets your desktop wallpaper to the daily Bing image.</p>"
            "<p><a href=\"https://github.com/pyprism/Bing-Wallpaper\">"
            "github.com/pyprism/Bing-Wallpaper</a></p>"));
}
