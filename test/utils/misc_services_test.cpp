#include "../unit_test_shared.h"

class MiscServicesTests : public QObject {
  Q_OBJECT

 private slots:
  void initTestCase();
  void crypto_shouldEncodeDecodeAndHash();
  void settingsHelpers_shouldMapModesAndNormalizeText();
  void subscriptionHelpers_shouldDetectSingleManualNode();
  void kernelPlatform_shouldBuildUrlsAndFilename();
  void kernelPlatform_shouldHandlePathUtilities();
  void dataUsageTracker_shouldTrackGlobalTotals();
};

void MiscServicesTests::initTestCase() {
  (void)DatabaseService::instance().init();
}



void MiscServicesTests::crypto_shouldEncodeDecodeAndHash() {
  const QByteArray raw("hello/world+=");
  const QString    b64 = Crypto::base64Encode(raw);
  QCOMPARE(Crypto::base64Decode(b64), raw);

  const QString b64Url = Crypto::base64UrlEncode(raw);
  QVERIFY(!b64Url.contains('+'));
  QVERIFY(!b64Url.contains('/'));
  QVERIFY(!b64Url.contains('='));
  QCOMPARE(Crypto::base64UrlDecode(b64Url), raw);

  QCOMPARE(Crypto::sha256(QStringLiteral("abc")),
           QString("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));

  const QString uuid = Crypto::generateUUID();
  QVERIFY(QRegularExpression(
              "^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$")
              .match(uuid)
              .hasMatch());
}



void MiscServicesTests::settingsHelpers_shouldMapModesAndNormalizeText() {
  QCOMPARE(SettingsHelpers::themeIndexFromMode(ThemeService::ThemeMode::Dark),
           0);
  QCOMPARE(SettingsHelpers::themeIndexFromMode(ThemeService::ThemeMode::Light),
           1);
  QCOMPARE(SettingsHelpers::themeIndexFromMode(ThemeService::ThemeMode::Auto),
           2);

  QCOMPARE(SettingsHelpers::themeModeFromIndex(0), ThemeService::ThemeMode::Dark);
  QCOMPARE(SettingsHelpers::themeModeFromIndex(1),
           ThemeService::ThemeMode::Light);
  QCOMPARE(SettingsHelpers::themeModeFromIndex(2), ThemeService::ThemeMode::Auto);
  QCOMPARE(SettingsHelpers::themeModeFromIndex(99),
           ThemeService::ThemeMode::Dark);

  QCOMPARE(SettingsHelpers::normalizeBypassText("a\r\nb\nc"), QString("a;b;c"));
  QCOMPARE(SettingsHelpers::resolveTextOrDefault(nullptr, "fallback"),
           QString("fallback"));
}



void MiscServicesTests::subscriptionHelpers_shouldDetectSingleManualNode() {
  SubscriptionInfo info;
  info.isManual          = false;
  info.useOriginalConfig = false;
  info.manualContent     = R"([{"type":"vmess","server":"a.com"}])";
  QVERIFY(!SubscriptionHelpers::isSingleManualNode(info, nullptr));

  info.isManual          = true;
  info.useOriginalConfig = true;
  QVERIFY(!SubscriptionHelpers::isSingleManualNode(info, nullptr));

  info.useOriginalConfig = false;
  QJsonObject outNode;
  QVERIFY(SubscriptionHelpers::isSingleManualNode(info, &outNode));
  QCOMPARE(outNode.value("type").toString(), QString("vmess"));
  QCOMPARE(outNode.value("server").toString(), QString("a.com"));

  info.manualContent = R"({"type":"trojan","server":"b.com"})";
  outNode            = QJsonObject();
  QVERIFY(SubscriptionHelpers::isSingleManualNode(info, &outNode));
  QCOMPARE(outNode.value("type").toString(), QString("trojan"));

  info.manualContent = R"({"type":"trojan"})";
  QVERIFY(!SubscriptionHelpers::isSingleManualNode(info, nullptr));

  info.manualContent = R"([{"type":"a","server":"s1"},{"type":"b","server":"s2"}])";
  QVERIFY(!SubscriptionHelpers::isSingleManualNode(info, nullptr));
}



void MiscServicesTests::kernelPlatform_shouldBuildUrlsAndFilename() {
  const QString arch = KernelPlatform::getKernelArch();
  QVERIFY(arch == "amd64" || arch == "arm64" || arch == "386");

  const QString filename = KernelPlatform::buildKernelFilename("v1.2.3");
  QVERIFY(filename.contains("1.2.3"));
  QVERIFY(filename.contains("windows"));
  QVERIFY(filename.endsWith(".zip"));
  QVERIFY(!filename.contains("v1.2.3"));

  const QStringList urls =
      KernelPlatform::buildDownloadUrls("1.2.3", "sing-box-1.2.3-windows-amd64.zip");
  QCOMPARE(urls.size(), 6);
  QVERIFY(urls[0].startsWith("https://ghproxy.net/"));
  QVERIFY(urls[1].startsWith("https://gh-proxy.org/"));
  QVERIFY(urls[2].startsWith("https://v6.gh-proxy.org/"));
  QVERIFY(urls[3].startsWith("https://hk.gh-proxy.org/"));
  QVERIFY(urls[4].startsWith("https://cdn.gh-proxy.org/"));
  QVERIFY(urls[5].startsWith("https://edgeone.gh-proxy.org/"));
  QVERIFY(urls[1].contains("/download/v1.2.3/"));
  QVERIFY(urls[1].contains("sing-box-1.2.3-windows-amd64.zip"));
}



void MiscServicesTests::kernelPlatform_shouldHandlePathUtilities() {
  QCOMPARE(KernelPlatform::queryKernelVersion(""), QString());
  QCOMPARE(KernelPlatform::queryKernelVersion("C:/definitely/not/exist.exe"),
           QString());

  QTemporaryDir tmpDir;
  QVERIFY(tmpDir.isValid());
  const QString nestedDir = tmpDir.filePath("a/b");
  QVERIFY(QDir().mkpath(nestedDir));
  const QString exePath = nestedDir + "/my-kernel.exe";
  QFile         f(exePath);
  QVERIFY(f.open(QIODevice::WriteOnly));
  f.write("dummy");
  f.close();

  const QString found =
      KernelPlatform::findExecutableInDir(tmpDir.path(), "my-kernel.exe");
  QCOMPARE(QDir::fromNativeSeparators(found),
           QDir::fromNativeSeparators(exePath));

  QString err;
  QVERIFY(!KernelPlatform::extractZipArchive(tmpDir.filePath("missing.zip"),
                                             tmpDir.filePath("out"),
                                             &err));
  QVERIFY(!err.isEmpty());
}



void MiscServicesTests::dataUsageTracker_shouldTrackGlobalTotals() {
  DataUsageTracker tracker;
  tracker.reset();
  // Build a fake connections JSON with two connections
  QJsonArray conns;
  {
    QJsonObject meta;
    meta.insert("sourceIP", "192.168.1.1");
    meta.insert("host", "example.com");
    meta.insert("process", "firefox.exe");
    QJsonObject c;
    c.insert("id", "conn-1");
    c.insert("upload", 1000);
    c.insert("download", 2000);
    c.insert("metadata", meta);
    QJsonArray chains;
    chains.append("proxy-out");
    c.insert("chains", chains);
    conns.append(c);
  }
  {
    QJsonObject meta;
    meta.insert("sourceIP", "192.168.1.2");
    meta.insert("host", "google.com");
    meta.insert("process", "chrome.exe");
    QJsonObject c;
    c.insert("id", "conn-2");
    c.insert("upload", 500);
    c.insert("download", 3000);
    c.insert("metadata", meta);
    QJsonArray chains;
    chains.append("direct");
    c.insert("chains", chains);
    conns.append(c);
  }
  QJsonObject connections;
  connections.insert("connections", conns);
  tracker.updateFromConnections(connections);

  // Verify globalTotals()
  const auto gt = tracker.globalTotals();
  QCOMPARE(gt.upload, qint64(1500));    // 1000 + 500
  QCOMPARE(gt.download, qint64(5000));  // 2000 + 3000

  // Verify snapshot contains matching globalTotals
  const QJsonObject snap   = tracker.snapshot();
  const QJsonObject gtSnap = snap.value("globalTotals").toObject();
  QCOMPARE(gtSnap.value("upload").toString().toLongLong(), qint64(1500));
  QCOMPARE(gtSnap.value("download").toString().toLongLong(), qint64(5000));

  // Verify reset clears totals
  tracker.reset();
  const auto gtAfterReset = tracker.globalTotals();
  QCOMPARE(gtAfterReset.upload, qint64(0));
  QCOMPARE(gtAfterReset.download, qint64(0));
}



int runMiscServicesTests(int argc, char* argv[]) {
  MiscServicesTests tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "misc_services_test.moc"
