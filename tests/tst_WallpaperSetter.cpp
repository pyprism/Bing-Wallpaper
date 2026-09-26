#include <QtTest>
#include "WallpaperSetter.h"

// The pure/string-building helpers are exposed and tested unconditionally
// (not #ifdef-guarded per OS) so this suite actually exercises the macOS,
// Windows *and* Linux logic wherever ctest happens to run, instead of only
// ever running the host OS's own branch and leaving the others "written,
// unverified" forever. Only the OS-guarded slots below (which call the real,
// executing setWallpaper()) are still guarded, since those genuinely can't
// run anywhere else.
class TestWallpaperSetter : public QObject
{
    Q_OBJECT

private slots:
    // macOS: AppleScript string building.
    void systemEventsScriptContainsPath();
    void finderScriptContainsPath();
    void escapeAppleScriptString_escapesQuotesAndBackslashes();
    void systemEventsScript_embedsEscapedPath();

    // Windows.
    void wallpaperStyleIsFill();

    // Linux/freedesktop: pure command-building helpers.
    void encodeFileUri_percentEncodesSpacesAndHash();
    void commandsForGroup_gnomeSetsLightAndDarkUri();
    void commandsForGroup_kdeUsesPlasmaApply();
    void commandsForGroup_unknownGroupIsEmpty();
    void desktopGroupOrder_detectsGnomeFirst();
    void desktopGroupOrder_detectsKdeFromPlasma();
    void desktopGroupOrder_unknownDesktopStillListsGenericFallback();
};

void TestWallpaperSetter::systemEventsScriptContainsPath()
{
    const QString script = WallpaperSetter::systemEventsScript(QStringLiteral("/tmp/test.jpg"));
    QVERIFY(script.contains(QStringLiteral("/tmp/test.jpg")));
    QVERIFY(script.contains(QStringLiteral("System Events")));
}

void TestWallpaperSetter::finderScriptContainsPath()
{
    const QString script = WallpaperSetter::finderScript(QStringLiteral("/tmp/test.jpg"));
    QVERIFY(script.contains(QStringLiteral("/tmp/test.jpg")));
    QVERIFY(script.contains(QStringLiteral("Finder")));
}

void TestWallpaperSetter::escapeAppleScriptString_escapesQuotesAndBackslashes()
{
    const QString in = QStringLiteral("/tmp/weird \"name\" \\ here.jpg");
    const QString out = WallpaperSetter::escapeAppleScriptString(in);
    QVERIFY(out.contains(QStringLiteral("\\\"")));
    QVERIFY(out.contains(QStringLiteral("\\\\")));
}

void TestWallpaperSetter::systemEventsScript_embedsEscapedPath()
{
    const QString path = QStringLiteral("/tmp/a \"quoted\" file.jpg");
    const QString script = WallpaperSetter::systemEventsScript(path);
    QVERIFY(script.contains(QStringLiteral("\\\"quoted\\\"")));
}

void TestWallpaperSetter::wallpaperStyleIsFill()
{
    QCOMPARE(WallpaperSetter::wallpaperStyleValue(), QStringLiteral("10"));
}

void TestWallpaperSetter::encodeFileUri_percentEncodesSpacesAndHash()
{
    const QString uri = WallpaperSetter::encodeFileUri(QStringLiteral("/tmp/my photo #1.jpg"));
    QVERIFY(uri.startsWith(QStringLiteral("file:///tmp/")));
    QVERIFY(!uri.contains(QLatin1Char(' ')));
    QVERIFY(uri.contains(QStringLiteral("%20")));
    QVERIFY(uri.contains(QStringLiteral("%23"))); // '#'
}

void TestWallpaperSetter::commandsForGroup_gnomeSetsLightAndDarkUri()
{
    const auto cmds = WallpaperSetter::commandsForGroup(QStringLiteral("gnome"), QStringLiteral("/tmp/x.jpg"));
    QCOMPARE(cmds.size(), 2);
    QVERIFY(cmds[0].contains(QStringLiteral("picture-uri")));
    QVERIFY(cmds[1].contains(QStringLiteral("picture-uri-dark")));
}

void TestWallpaperSetter::commandsForGroup_kdeUsesPlasmaApply()
{
    const auto cmds = WallpaperSetter::commandsForGroup(QStringLiteral("kde"), QStringLiteral("/tmp/x.jpg"));
    QCOMPARE(cmds.size(), 1);
    QCOMPARE(cmds.first().first(), QStringLiteral("plasma-apply-wallpaperimage"));
}

void TestWallpaperSetter::commandsForGroup_unknownGroupIsEmpty()
{
    QVERIFY(WallpaperSetter::commandsForGroup(QStringLiteral("not-a-real-de"), QStringLiteral("/tmp/x.jpg")).isEmpty());
}

void TestWallpaperSetter::desktopGroupOrder_detectsGnomeFirst()
{
    const auto order = WallpaperSetter::desktopGroupOrder(QStringLiteral("ubuntu:GNOME"));
    QVERIFY(!order.isEmpty());
    QCOMPARE(order.first(), QStringLiteral("gnome"));
    // Every known group must still be present as a fallback.
    QVERIFY(order.contains(QStringLiteral("kde")));
    QVERIFY(order.contains(QStringLiteral("generic")));
}

void TestWallpaperSetter::desktopGroupOrder_detectsKdeFromPlasma()
{
    const auto order = WallpaperSetter::desktopGroupOrder(QStringLiteral("KDE"));
    QCOMPARE(order.first(), QStringLiteral("kde"));
}

void TestWallpaperSetter::desktopGroupOrder_unknownDesktopStillListsGenericFallback()
{
    const auto order = WallpaperSetter::desktopGroupOrder(QStringLiteral(""));
    QVERIFY(order.contains(QStringLiteral("generic")));
    QVERIFY(order.contains(QStringLiteral("gnome")));
}

QTEST_APPLESS_MAIN(TestWallpaperSetter)
#include "tst_WallpaperSetter.moc"
