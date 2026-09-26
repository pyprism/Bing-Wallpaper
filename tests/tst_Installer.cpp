#include <QtTest>
#include <QDir>
#include "Installer.h"

class TestInstaller : public QObject
{
    Q_OBJECT

private slots:
    void xmlEscapeEscapesReservedCharacters();
    void xmlEscapeLeavesPlainPathsAlone();
    void isUnderApplicationsFolder_systemApplications();
    void isUnderApplicationsFolder_userApplications();
    void isUnderApplicationsFolder_rejectsVolumesAndDownloads();
    void quotedRegistryPath_wrapsInQuotes();
    void quotedRegistryPath_doesNotDoubleWrap();
};

void TestInstaller::xmlEscapeEscapesReservedCharacters()
{
    const QString in = QStringLiteral("/Applications/A & B <app>.app/\"quoted\"");
    const QString out = Installer::xmlEscape(in);
    QVERIFY(!out.contains(QLatin1Char('&') /* raw */) || out.contains(QStringLiteral("&amp;")));
    QVERIFY(out.contains(QStringLiteral("&amp;")));
    QVERIFY(out.contains(QStringLiteral("&lt;")));
    QVERIFY(out.contains(QStringLiteral("&gt;")));
    QVERIFY(out.contains(QStringLiteral("&quot;")));
}

void TestInstaller::xmlEscapeLeavesPlainPathsAlone()
{
    const QString in = QStringLiteral("/Applications/Bing Wallpaper.app/Contents/MacOS/bing-wallpaper");
    QCOMPARE(Installer::xmlEscape(in), in);
}

void TestInstaller::isUnderApplicationsFolder_systemApplications()
{
    QVERIFY(Installer::isUnderApplicationsFolder(
        QStringLiteral("/Applications/Bing Wallpaper.app/Contents/MacOS/bing-wallpaper")));
}

void TestInstaller::isUnderApplicationsFolder_userApplications()
{
    const QString home = QDir::homePath();
    QVERIFY(Installer::isUnderApplicationsFolder(
        home + QStringLiteral("/Applications/Bing Wallpaper.app/Contents/MacOS/bing-wallpaper")));
}

void TestInstaller::isUnderApplicationsFolder_rejectsVolumesAndDownloads()
{
    QVERIFY(!Installer::isUnderApplicationsFolder(
        QStringLiteral("/Volumes/Bing Wallpaper/Bing Wallpaper.app/Contents/MacOS/bing-wallpaper")));
    QVERIFY(!Installer::isUnderApplicationsFolder(
        QDir::homePath() + QStringLiteral("/Downloads/Bing Wallpaper.app/Contents/MacOS/bing-wallpaper")));
}

void TestInstaller::quotedRegistryPath_wrapsInQuotes()
{
    const QString out = Installer::quotedRegistryPath(QStringLiteral("C:/Program Files/bing-wallpaper/bing-wallpaper.exe"));
    QVERIFY(out.startsWith(QLatin1Char('"')));
    QVERIFY(out.endsWith(QLatin1Char('"')));
    QVERIFY(out.contains(QStringLiteral("Program Files")));
}

void TestInstaller::quotedRegistryPath_doesNotDoubleWrap()
{
    // Already native-separator-shaped so QDir::toNativeSeparators() (a no-op
    // outside Windows) can't change the comparison depending on host OS.
    const QString alreadyQuoted = QStringLiteral("\"C:\\already\\quoted.exe\"");
    QCOMPARE(Installer::quotedRegistryPath(alreadyQuoted), alreadyQuoted);
}

QTEST_APPLESS_MAIN(TestInstaller)
#include "tst_Installer.moc"
