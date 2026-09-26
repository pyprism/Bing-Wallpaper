#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include "BingClient.h"

class TestBingClient : public QObject
{
    Q_OBJECT

private slots:
    void parseSingleImage();
    void parseEmptyImages();
    void parseArchive();
    void parseMalformedJsonYieldsNoImages();
    void buildImageUrl_uhd();
    void buildImageUrl_fallback();
    void fallbackImageUrl_usesPlainField();
    void imageId_extractsOhrId();
    void imageId_emptyWhenNoMatch();
    void sanitizeForFilename_replacesUnsafeChars();
    void fileBaseName_isMarketAware();
    void saveDirIsUnderHomeAndNamedCorrectly();
    void expectedPathRespectsDirOverride();
    void sidecarRoundTrips();
    void sidecarMissingReadsEmpty();
    void shouldApply_manualAlwaysApplies();
    void shouldApply_emptyLastAppliedAlwaysApplies();
    void shouldApply_autoOnlyWhenNewer();
};

void TestBingClient::parseSingleImage()
{
    const QByteArray json = R"JSON({
        "images": [
            {"url": "/th?id=OHR.TestImage_EN-US1234567890_1920x1080.jpg&rf=Test_1920x1080.jpg&pid=hp",
             "startdate": "20260301",
             "copyright": "Test Image (© Someone)"}
        ]
    })JSON";

    const auto images = BingClient::parseApiResponse(json);
    QCOMPARE(images.size(), 1);
    QCOMPARE(images[0].date, QStringLiteral("20260301"));
    QCOMPARE(images[0].copyright, QString::fromUtf8("Test Image (\xC2\xA9 Someone)"));
    QVERIFY(images[0].url.startsWith(QStringLiteral("/th?id=OHR.TestImage")));
}

void TestBingClient::parseEmptyImages()
{
    const QByteArray json = R"({"images": []})";
    const auto images = BingClient::parseApiResponse(json);
    QCOMPARE(images.size(), 0);
}

void TestBingClient::parseArchive()
{
    const QByteArray json = R"({
        "images": [
            {"url": "/th?id=OHR.Day0_EN-US1_1920x1080.jpg", "startdate": "20260704", "copyright": "Day 0"},
            {"url": "/th?id=OHR.Day1_EN-US2_1920x1080.jpg", "startdate": "20260703", "copyright": "Day 1"}
        ]
    })";
    const auto images = BingClient::parseApiResponse(json);
    QCOMPARE(images.size(), 2);
    QCOMPARE(images[1].date, QStringLiteral("20260703"));
}

void TestBingClient::parseMalformedJsonYieldsNoImages()
{
    const QByteArray notJson = R"(this is not json at all { [ )";
    const auto images = BingClient::parseApiResponse(notJson);
    QCOMPARE(images.size(), 0);

    const QByteArray wrongShape = R"({"images": "not an array"})";
    QCOMPARE(BingClient::parseApiResponse(wrongShape).size(), 0);
}

void TestBingClient::buildImageUrl_uhd()
{
    // Real shape returned by HPImageArchive.aspx — the resolution suffix is
    // followed by more query-string junk, not the end of the field.
    const QString field = QStringLiteral(
        "/th?id=OHR.LibertyHall_EN-US2562041614_1920x1080.jpg&rf=LaDigue_1920x1080.jpg&pid=hp");
    const QString result = BingClient::buildImageUrl(field);
    QCOMPARE(result, QStringLiteral("https://www.bing.com/th?id=OHR.LibertyHall_EN-US2562041614_UHD.jpg"));
}

void TestBingClient::buildImageUrl_fallback()
{
    const QString field = QStringLiteral("/some/other/path.jpg");
    const QString result = BingClient::buildImageUrl(field);
    QCOMPARE(result, QStringLiteral("https://www.bing.com/some/other/path.jpg"));
}

void TestBingClient::fallbackImageUrl_usesPlainField()
{
    const QString field = QStringLiteral(
        "/th?id=OHR.LibertyHall_EN-US2562041614_1920x1080.jpg&rf=LaDigue_1920x1080.jpg&pid=hp");
    QCOMPARE(BingClient::fallbackImageUrl(field), QStringLiteral("https://www.bing.com") + field);
}

void TestBingClient::imageId_extractsOhrId()
{
    const QString field = QStringLiteral(
        "/th?id=OHR.LibertyHall_EN-US2562041614_1920x1080.jpg&rf=LaDigue_1920x1080.jpg&pid=hp");
    QCOMPARE(BingClient::imageId(field), QStringLiteral("OHR.LibertyHall_EN-US2562041614"));
}

void TestBingClient::imageId_emptyWhenNoMatch()
{
    QVERIFY(BingClient::imageId(QStringLiteral("/some/other/path.jpg")).isEmpty());
}

void TestBingClient::sanitizeForFilename_replacesUnsafeChars()
{
    QCOMPARE(BingClient::sanitizeForFilename(QStringLiteral("OHR.Test/Weird?Name")),
              QStringLiteral("OHR.Test_Weird_Name"));
}

void TestBingClient::fileBaseName_isMarketAware()
{
    const QString a = BingClient::fileBaseName(QStringLiteral("20260704"), QStringLiteral("en-US"),
                                                QStringLiteral("OHR.Test1234"));
    const QString b = BingClient::fileBaseName(QStringLiteral("20260704"), QStringLiteral("de-DE"),
                                                QStringLiteral("OHR.Test1234"));
    QVERIFY(a != b); // two markets on the same date must not collide
    QCOMPARE(a, QStringLiteral("20260704_en-US_OHR.Test1234"));
}

void TestBingClient::saveDirIsUnderHomeAndNamedCorrectly()
{
    qunsetenv("BING_WALLPAPER_DIR");
    const QString dir = BingClient::computeSaveDir();
    QVERIFY(dir.contains(QStringLiteral("bing-wallpapers")));
#if defined(Q_OS_MAC)
    QVERIFY(dir.endsWith(QStringLiteral("Library/Application Support/bing-wallpapers")));
#elif defined(Q_OS_WIN)
    QVERIFY(dir.endsWith(QStringLiteral("bing-wallpapers")));
#else
    QVERIFY(dir.endsWith(QStringLiteral(".local/share/bing-wallpapers")));
#endif
}

void TestBingClient::expectedPathRespectsDirOverride()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("BING_WALLPAPER_DIR", dir.path().toUtf8());

    BingClient::ImageInfo info;
    info.url = QStringLiteral("/th?id=OHR.Test_EN-US123_1920x1080.jpg");
    info.date = QStringLiteral("20260704");
    const QString path = BingClient::expectedPath(info, QStringLiteral("en-US"));
    QVERIFY(path.startsWith(dir.path()));
    QVERIFY(path.endsWith(QStringLiteral(".jpg")));

    qunsetenv("BING_WALLPAPER_DIR");
}

void TestBingClient::sidecarRoundTrips()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString imagePath = dir.filePath("20260704_en-US_OHR.Test.jpg");

    // fromUtf8(), not QStringLiteral(): "\xC2\xA9" here is a `const char*`
    // (narrow) literal, so these are genuinely the two raw UTF-8 bytes for
    // U+00A9 (©), which fromUtf8() decodes correctly into one code point —
    // matching what QJsonDocument does with the real Bing API response.
    // (A prior version of this test used QStringLiteral(), a char16_t/u""
    // literal, where the same escape is mojibake instead — same bug this
    // was written to catch, just in the test rather than the code under
    // test, so it never actually exercised the title split below.)
    const QString copyright = QString::fromUtf8("A Title (\xC2\xA9 Someone)");
    BingClient::writeSidecar(imagePath, QStringLiteral("20260704"), QStringLiteral("en-US"),
                              copyright, QStringLiteral("/th?id=x"));

    QCOMPARE(BingClient::readSidecarCopyright(imagePath), copyright);

    QFile sidecar(dir.filePath("20260704_en-US_OHR.Test.json"));
    QVERIFY(sidecar.open(QIODevice::ReadOnly));
    const QJsonObject obj = QJsonDocument::fromJson(sidecar.readAll()).object();
    QCOMPARE(obj.value("title").toString(), QStringLiteral("A Title"));
}

void TestBingClient::sidecarMissingReadsEmpty()
{
    QVERIFY(BingClient::readSidecarCopyright(QStringLiteral("/does/not/exist.jpg")).isEmpty());
}

void TestBingClient::shouldApply_manualAlwaysApplies()
{
    QVERIFY(BingClient::shouldApply(QStringLiteral("20260901"), QStringLiteral("20260801"), /*manual=*/true));
}

void TestBingClient::shouldApply_emptyLastAppliedAlwaysApplies()
{
    QVERIFY(BingClient::shouldApply(QString(), QStringLiteral("20260101"), /*manual=*/false));
}

void TestBingClient::shouldApply_autoOnlyWhenNewer()
{
    QVERIFY(BingClient::shouldApply(QStringLiteral("20260801"), QStringLiteral("20260802"), false));
    QVERIFY(!BingClient::shouldApply(QStringLiteral("20260801"), QStringLiteral("20260801"), false));
    QVERIFY(!BingClient::shouldApply(QStringLiteral("20260801"), QStringLiteral("20260701"), false));
}

QTEST_APPLESS_MAIN(TestBingClient)
#include "tst_BingClient.moc"
