#include <QtTest>
#include "WallpaperSetter.h"

// Mirrors the original Go platform_*_test.go files: guarded by #ifdef so only
// the tests relevant to the OS actually building the code compile and run.
class TestWallpaperSetter : public QObject
{
    Q_OBJECT

private slots:
#if defined(Q_OS_MAC)
    void systemEventsScriptContainsPath();
    void finderScriptContainsPath();
#elif defined(Q_OS_WIN)
    void wallpaperStyleIsFill();
#else
    void candidateCommandsTriesGsettingsFirst();
    void candidateCommandsIncludesFehFallback();
#endif
};

#if defined(Q_OS_MAC)

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

#elif defined(Q_OS_WIN)

void TestWallpaperSetter::wallpaperStyleIsFill()
{
    QCOMPARE(WallpaperSetter::wallpaperStyleValue(), QStringLiteral("10"));
}

#else

void TestWallpaperSetter::candidateCommandsTriesGsettingsFirst()
{
    const auto cmds = WallpaperSetter::candidateCommands(QStringLiteral("/tmp/test.jpg"));
    QVERIFY(!cmds.isEmpty());
    QCOMPARE(cmds.first().first(), QStringLiteral("gsettings"));
}

void TestWallpaperSetter::candidateCommandsIncludesFehFallback()
{
    const auto cmds = WallpaperSetter::candidateCommands(QStringLiteral("/tmp/test.jpg"));
    bool hasFeh = false;
    for (const auto &cmd : cmds) {
        if (cmd.first() == QStringLiteral("feh"))
            hasFeh = true;
    }
    QVERIFY(hasFeh);
}

#endif

QTEST_APPLESS_MAIN(TestWallpaperSetter)
#include "tst_WallpaperSetter.moc"
