#include <QtTest>
#include "BingClient.h"

class TestBingClient : public QObject
{
    Q_OBJECT

private slots:
    void parseSingleImage();
    void parseEmptyImages();
    void parseArchive();
    void buildImageUrl_uhd();
    void buildImageUrl_fallback();
    void saveDirIsUnderHomeAndNamedCorrectly();
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

void TestBingClient::saveDirIsUnderHomeAndNamedCorrectly()
{
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

QTEST_APPLESS_MAIN(TestBingClient)
#include "tst_BingClient.moc"
