#include <QtTest>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QSettings>
#include "WallpaperLibrary.h"

using namespace WallpaperLibrary;

class TestWallpaperLibrary : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void scanReadsSidecarMetadata();
    void scanSelfHealsTitleEqualToCopyright();
    void deriveTitle_splitsOnCopyrightMark();
    void deriveTitle_fallsBackToWholeStringWhenNoMatch();
    void scanFallsBackToLegacyDateFromBareName();
    void scanSkipsNonJpgAndUndateableNoise();
    void scanSortsNewestFirst();
    void entriesOlderThanFiltersByCutoff();
    void totalSizeSumsEntries();
    void removeDeletesImageAndSidecar_hardDelete();
    void parseLegacyDateHandlesBothSchemes();
    void skipListPureHelpers();
    void isDeletedRoundTripsThroughSettings();

private:
    QTemporaryDir m_dir;
    QTemporaryDir m_settingsDir;
};

void TestWallpaperLibrary::initTestCase()
{
    QVERIFY(m_settingsDir.isValid());
    // WallpaperLibrary::isDeleted()/markDeleted() go through the default
    // QSettings(), which on macOS's NativeFormat writes to the *real*
    // ~/Library/Preferences regardless of organization/application name.
    // Redirect to a throwaway IniFormat file under a QTemporaryDir so the
    // skip-list tests can never touch (or be polluted by) the user's actual
    // settings.
    QCoreApplication::setOrganizationName(QStringLiteral("bing-wallpaper-test"));
    QCoreApplication::setApplicationName(QStringLiteral("BingWallpaperTest"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());
}

void TestWallpaperLibrary::init()
{
    QSettings settings;
    settings.clear();
}

namespace {
void writeFile(const QString &path, const QByteArray &content = "x")
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(content);
}

void writeSidecar(const QString &jsonPath, const QString &date, const QString &market,
                   const QString &title, const QString &copyright)
{
    const QByteArray json = QStringLiteral(
        "{\"date\":\"%1\",\"market\":\"%2\",\"title\":\"%3\",\"copyright\":\"%4\",\"sourceUrl\":\"\"}")
        .arg(date, market, title, copyright)
        .toUtf8();
    writeFile(jsonPath, json);
}
}

void TestWallpaperLibrary::scanReadsSidecarMetadata()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeFile(dir.filePath("20260704_en-US_OHR.Test.jpg"));
    writeSidecar(dir.filePath("20260704_en-US_OHR.Test.json"), "20260704", "en-US", "A Title",
                 "A Title (\xC2\xA9 Someone)");

    const auto entries = scan(dir.path());
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries[0].date, QDate(2026, 7, 4));
    QCOMPARE(entries[0].market, QStringLiteral("en-US"));
    QCOMPARE(entries[0].title, QStringLiteral("A Title"));
    QVERIFY(entries[0].copyright.contains(QStringLiteral("Someone")));
}

void TestWallpaperLibrary::scanSelfHealsTitleEqualToCopyright()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeFile(dir.filePath("20260704_en-US_OHR.Test.jpg"));
    // Simulates a sidecar written by the once-buggy BingClient::writeSidecar
    // (fromUtf8, real "©", not the mojibake-producing QStringLiteral bug —
    // the point here is title == copyright, regardless of which bug wrote it).
    const QString copyright = QString::fromUtf8("A Title (\xC2\xA9 Someone)");
    writeSidecar(dir.filePath("20260704_en-US_OHR.Test.json"), "20260704", "en-US", copyright, copyright);

    const auto entries = scan(dir.path());
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries[0].title, QStringLiteral("A Title"));
    QCOMPARE(entries[0].copyright, copyright); // untouched
}

void TestWallpaperLibrary::deriveTitle_splitsOnCopyrightMark()
{
    const QString copyright = QString::fromUtf8("A Title (\xC2\xA9 Someone)");
    QCOMPARE(deriveTitle(copyright), QStringLiteral("A Title"));
}

void TestWallpaperLibrary::deriveTitle_fallsBackToWholeStringWhenNoMatch()
{
    QCOMPARE(deriveTitle(QStringLiteral("No copyright mark here")),
              QStringLiteral("No copyright mark here"));
}

void TestWallpaperLibrary::scanFallsBackToLegacyDateFromBareName()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeFile(dir.filePath("20260301.jpg")); // legacy pre-market-aware naming, no sidecar

    const auto entries = scan(dir.path());
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries[0].date, QDate(2026, 3, 1));
    QVERIFY(entries[0].market.isEmpty());
}

void TestWallpaperLibrary::scanSkipsNonJpgAndUndateableNoise()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeFile(dir.filePath("readme.txt"));
    writeFile(dir.filePath("not-a-date.jpg"));
    writeFile(dir.filePath("20260101.jpg"));

    const auto entries = scan(dir.path());
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries[0].date, QDate(2026, 1, 1));
}

void TestWallpaperLibrary::scanSortsNewestFirst()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeFile(dir.filePath("20260101.jpg"));
    writeFile(dir.filePath("20260301.jpg"));
    writeFile(dir.filePath("20260201.jpg"));

    const auto entries = scan(dir.path());
    QCOMPARE(entries.size(), 3);
    QCOMPARE(entries[0].date, QDate(2026, 3, 1));
    QCOMPARE(entries[1].date, QDate(2026, 2, 1));
    QCOMPARE(entries[2].date, QDate(2026, 1, 1));
}

void TestWallpaperLibrary::entriesOlderThanFiltersByCutoff()
{
    QList<Entry> entries;
    Entry a; a.date = QDate(2026, 1, 1); entries << a;
    Entry b; b.date = QDate(2026, 6, 1); entries << b;
    Entry c; c.date = QDate(2026, 9, 1); entries << c;

    const auto old = entriesOlderThan(entries, QDate(2026, 7, 1));
    QCOMPARE(old.size(), 2);
    QCOMPARE(old[0].date, QDate(2026, 1, 1));
    QCOMPARE(old[1].date, QDate(2026, 6, 1));
}

void TestWallpaperLibrary::totalSizeSumsEntries()
{
    QList<Entry> entries;
    Entry a; a.size = 100; entries << a;
    Entry b; b.size = 250; entries << b;
    QCOMPARE(totalSize(entries), 350);
}

void TestWallpaperLibrary::removeDeletesImageAndSidecar_hardDelete()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString imgPath = dir.filePath("20260401.jpg");
    const QString sidecarPath = dir.filePath("20260401.json");
    writeFile(imgPath);
    writeFile(sidecarPath);

    Entry e;
    e.path = imgPath;
    e.baseName = QStringLiteral("20260401");

    // useTrash=false so this is deterministic and safe under headless CI —
    // QFile::moveToTrash() on a throwaway QTemporaryDir file would otherwise
    // land in the *real* ~/.Trash and may not even be available headlessly.
    QVERIFY(remove(e, /*useTrash=*/false));
    QVERIFY(!QFile::exists(imgPath));
    QVERIFY(!QFile::exists(sidecarPath));
}

void TestWallpaperLibrary::parseLegacyDateHandlesBothSchemes()
{
    QCOMPARE(parseLegacyDate(QStringLiteral("20260704")), QDate(2026, 7, 4));
    QCOMPARE(parseLegacyDate(QStringLiteral("20260704_en-US_OHR.Test")), QDate(2026, 7, 4));
    QVERIFY(!parseLegacyDate(QStringLiteral("not-a-date")).isValid());
}

void TestWallpaperLibrary::skipListPureHelpers()
{
    QStringList ids;
    QVERIFY(!containsId(ids, QStringLiteral("a")));
    ids = withId(ids, QStringLiteral("a"));
    QVERIFY(containsId(ids, QStringLiteral("a")));
    // Adding the same id twice doesn't duplicate it.
    ids = withId(ids, QStringLiteral("a"));
    QCOMPARE(ids.size(), 1);
}

void TestWallpaperLibrary::isDeletedRoundTripsThroughSettings()
{
    QVERIFY(!isDeleted(QStringLiteral("20260704_en-US_OHR.Test")));
    markDeleted(QStringLiteral("20260704_en-US_OHR.Test"));
    QVERIFY(isDeleted(QStringLiteral("20260704_en-US_OHR.Test")));
    QVERIFY(!isDeleted(QStringLiteral("some-other-id")));
}

QTEST_APPLESS_MAIN(TestWallpaperLibrary)
#include "tst_WallpaperLibrary.moc"
