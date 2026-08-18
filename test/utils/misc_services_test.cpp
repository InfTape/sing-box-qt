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
  void linuxDesktopIntegration_shouldManageProxyAndAutoStart();
  void runtimeConfigResolver_shouldPreferExistingPersistedConfig();
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
#ifdef Q_OS_WIN
  QVERIFY(filename.contains("windows"));
  QVERIFY(filename.endsWith(".zip"));
#elif defined(Q_OS_LINUX)
  QVERIFY(filename.contains("linux"));
  QVERIFY(filename.endsWith(".tar.gz"));
#endif
  QVERIFY(!filename.contains("v1.2.3"));

  const QStringList urls =
      KernelPlatform::buildDownloadUrls("1.2.3", "sing-box-1.2.3-windows-amd64.zip");
  QCOMPARE(urls.size(), 7);
  QVERIFY(urls[0].startsWith("https://github.com/SagerNet/sing-box/releases/download/"));
  QVERIFY(urls[1].startsWith("https://ghproxy.net/"));
  QVERIFY(urls[2].startsWith("https://gh-proxy.org/"));
  QVERIFY(urls[3].startsWith("https://v6.gh-proxy.org/"));
  QVERIFY(urls[4].startsWith("https://hk.gh-proxy.org/"));
  QVERIFY(urls[5].startsWith("https://cdn.gh-proxy.org/"));
  QVERIFY(urls[6].startsWith("https://edgeone.gh-proxy.org/"));
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
  QVERIFY(!KernelPlatform::extractArchive(tmpDir.filePath("missing.zip"),
                                          tmpDir.filePath("out"),
                                          &err));
  QVERIFY(!err.isEmpty());

#ifdef Q_OS_LINUX
  const QString archiveRoot = tmpDir.filePath("archive-root/sing-box-test");
  QVERIFY(QDir().mkpath(archiveRoot));
  QFile archivedKernel(QDir(archiveRoot).filePath("sing-box"));
  QVERIFY(archivedKernel.open(QIODevice::WriteOnly));
  archivedKernel.write("test-kernel");
  archivedKernel.close();
  const QString archivePath = tmpDir.filePath("kernel.tar.gz");
  QProcess      tar;
  tar.start("tar",
            {"-czf",
             archivePath,
             "-C",
             tmpDir.filePath("archive-root"),
             "sing-box-test"});
  QVERIFY(tar.waitForFinished(10000));
  QCOMPARE(tar.exitCode(), 0);
  const QString extractPath = tmpDir.filePath("extracted");
  QVERIFY2(KernelPlatform::extractArchive(archivePath, extractPath, &err),
           qPrintable(err));
  QCOMPARE(KernelPlatform::findExecutableInDir(extractPath, "sing-box"),
           QDir(extractPath).filePath("sing-box-test/sing-box"));
#endif
}

void MiscServicesTests::linuxDesktopIntegration_shouldManageProxyAndAutoStart() {
#ifndef Q_OS_LINUX
  QSKIP("Linux-only desktop integration test");
#else
  QTemporaryDir tmpDir;
  QVERIFY(tmpDir.isValid());

  const QString proxyConfig = tmpDir.filePath("kioslaverc");
  qputenv("SING_BOX_QT_KDE_PROXY_CONFIG", proxyConfig.toUtf8());
  QVERIFY(SystemProxy::setProxy("127.0.0.1", 2080));
  QVERIFY(SystemProxy::isProxyEnabled());
  QCOMPARE(SystemProxy::getProxyHost(), QString("127.0.0.1"));
  QCOMPARE(SystemProxy::getProxyPort(), 2080);

  const QString kreadconfig = QStandardPaths::findExecutable("kreadconfig6");
  QVERIFY(!kreadconfig.isEmpty());
  auto readKConfig = [&](const QString& key) {
    QProcess kread;
    kread.start(kreadconfig,
                {"--file",
                 proxyConfig,
                 "--group",
                 "Proxy Settings",
                 "--key",
                 key});
    if (!kread.waitForFinished(5000) || kread.exitCode() != 0) {
      return QString();
    }
    return QString::fromUtf8(kread.readAllStandardOutput()).trimmed();
  };
  QCOMPARE(readKConfig("ProxyType"), QString("1"));
  QCOMPARE(readKConfig("httpProxy"), QString("http://127.0.0.1:2080"));
  QCOMPARE(readKConfig("httpsProxy"), QString("http://127.0.0.1:2080"));
  QCOMPARE(readKConfig("socksProxy"), QString("socks://127.0.0.1:2080"));

  QFile proxyFile(proxyConfig);
  QVERIFY(proxyFile.open(QIODevice::ReadOnly | QIODevice::Text));
  const QString proxyText = QString::fromUtf8(proxyFile.readAll());
  QVERIFY(proxyText.contains("[Proxy Settings]"));
  QVERIFY(!proxyText.contains("[Proxy%20Settings]"));
  proxyFile.close();

  QVERIFY(SystemProxy::clearProxy());
  QCOMPARE(readKConfig("ProxyType"), QString("0"));
  QVERIFY(readKConfig("httpProxy").isEmpty());
  QVERIFY(readKConfig("httpsProxy").isEmpty());
  QVERIFY(readKConfig("socksProxy").isEmpty());
  qunsetenv("SING_BOX_QT_KDE_PROXY_CONFIG");

  const QString autostartFile = tmpDir.filePath("sing-box-qt.desktop");
  qputenv("SING_BOX_QT_AUTOSTART_FILE", autostartFile.toUtf8());
  QVERIFY(AutoStart::setEnabled(true, "Sing-Box Qt"));
  QVERIFY(AutoStart::isEnabled("Sing-Box Qt"));
  QFile entry(autostartFile);
  QVERIFY(entry.open(QIODevice::ReadOnly | QIODevice::Text));
  const QString entryText = QString::fromUtf8(entry.readAll());
  QVERIFY(entryText.contains("Exec="));
  QVERIFY(entryText.contains(" --hide"));
  QVERIFY(AutoStart::setEnabled(false, "Sing-Box Qt"));
  QVERIFY(!QFile::exists(autostartFile));
  qunsetenv("SING_BOX_QT_AUTOSTART_FILE");
#endif
}

void MiscServicesTests::
    runtimeConfigResolver_shouldPreferExistingPersistedConfig() {
  QTemporaryDir tmpDir;
  QVERIFY(tmpDir.isValid());
  const QString activePath   = tmpDir.filePath("active.json");
  const QString fallbackPath = tmpDir.filePath("config.json");
  for (const QString& path : {activePath, fallbackPath}) {
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{}");
  }

  QCOMPARE(RuntimeConfigResolver::selectConfigPath(activePath, fallbackPath),
           QDir::cleanPath(QFileInfo(activePath).absoluteFilePath()));

  DatabaseService& db = DatabaseService::instance();
  struct ActiveConfigPathGuard {
    DatabaseService& db;
    QString          originalPath;
    ~ActiveConfigPathGuard() { db.saveActiveConfigPath(originalPath); }
  } guard{db, db.getActiveConfigPath()};
  QVERIFY(db.saveActiveConfigPath(activePath));
  QCOMPARE(RuntimeConfigResolver::resolveConfigPath(),
           QDir::cleanPath(QFileInfo(activePath).absoluteFilePath()));

  QVERIFY(QFile::remove(activePath));
  QCOMPARE(RuntimeConfigResolver::selectConfigPath(activePath, fallbackPath),
           QDir::cleanPath(QFileInfo(fallbackPath).absoluteFilePath()));
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
