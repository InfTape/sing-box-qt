#include <QApplication>
#include <QClipboard>
#include <QScrollArea>
#include <QScrollBar>
#include <QPointer>
#include <QTimer>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <memory>
#include <QtTest/QtTest>
#include "views/logs/LogView.h"
#include "widgets/logs/LogRowWidget.h"
#include "utils/ThemeManager.h"
#include "storage/DatabaseService.h"
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#endif

namespace {
qint64 privateMemoryBytes() {
#ifdef Q_OS_WIN
  PROCESS_MEMORY_COUNTERS_EX counters{};
  if (GetProcessMemoryInfo(GetCurrentProcess(),
                           reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                           sizeof(counters))) {
    return counters.PrivateUsage;
  }
#endif
  return 0;
}
}  // namespace

class LogViewTest : public QObject {
  Q_OBJECT
 private slots:
  void initTestCase() {
    m_previous = qgetenv("SING_BOX_QT_DATA_DIR");
    m_directory = std::make_unique<QTemporaryDir>();
    QVERIFY(m_directory->isValid());
    qputenv("SING_BOX_QT_DATA_DIR", m_directory->path().toUtf8());
  }
  void cleanupTestCase() {
    DatabaseService::instance().close();
    qputenv("SING_BOX_QT_DATA_DIR", m_previous);
  }

  void retainsSearchableHistoryBeyondVisibleRows() {
    LogView view(nullptr);
    view.resize(1600, 700);
    view.show();
    for (int i = 0; i < 250; ++i) {
      view.appendApiLog(i % 2 == 0 ? "info" : "error",
                        QString("message-%1-end").arg(i));
    }
    QTRY_COMPARE(view.findChildren<LogRowWidget*>().size(), 50);
    QTRY_COMPARE(view.findChild<QLabel*>("TotalTag")->text(), QString("250 entries"));
    QTRY_COMPARE(view.findChild<QLabel*>("ErrorTag")->text(), QString("Errors: 125"));
    view.findChild<QPushButton*>("CopyBtn")->click();
    const QString copied = QApplication::clipboard()->text();
    QVERIFY(!copied.contains("message-0-end"));
    QVERIFY(!copied.contains("message-199-end"));
    QVERIFY(copied.contains("message-200-end"));
    QVERIFY(copied.contains("message-249-end"));
    QCOMPARE(copied.split('\n').size(), 50);
    QVERIFY(!view.findChild<QPushButton*>("OlderBtn"));
    QVERIFY(!view.findChild<QPushButton*>("NewerBtn"));
    QVERIFY(!view.findChild<QPushButton*>("LatestBtn"));
    QVERIFY(!view.findChild<QLabel*>("PageStatus"));

    auto* search = view.findChild<QLineEdit*>("SearchInput");
    search->setText("message-0-end");
    QTRY_COMPARE(view.findChildren<LogRowWidget*>().size(), 1);
    QTRY_COMPARE(view.findChild<QLabel*>("TotalTag")->text(), QString("1 entries"));
    QTRY_COMPARE(view.findChild<QLabel*>("ErrorTag")->text(), QString("Errors: 0"));
    view.findChild<QPushButton*>("CopyBtn")->click();
    QVERIFY(QApplication::clipboard()->text().contains("message-0-end"));
    search->clear();
    QTRY_COMPARE(view.findChildren<LogRowWidget*>().size(), 50);
    QTRY_COMPARE(view.findChild<QLabel*>("ErrorTag")->text(), QString("Errors: 125"));
    view.findChild<QPushButton*>("ClearBtn")->click();
    QTRY_COMPARE(view.findChildren<LogRowWidget*>().size(), 0);
    QTRY_COMPARE(view.findChild<QLabel*>("TotalTag")->text(), QString("0 entries"));
    view.appendApiLog("warning", "one warning\na second warning");
    QTRY_COMPARE(view.findChild<QLabel*>("WarningTag")->text(), QString("Warnings: 2"));
  }
  void showsConnectionNetworkBadges() {
    LogView view(nullptr);
    view.show();
    view.clear();
    view.appendApiLog("info", "inbound/tun[tun-in]: inbound packet connection "
                              "from 172.19.0.1:55057");
    view.appendApiLog("info", "inbound/tun[tun-in]: inbound packet connection "
                              "to 104.18.95.41:443");
    view.appendApiLog("info", "outbound/vless[JP]: outbound packet connection "
                              "to 104.18.95.41:443");
    view.appendApiLog("info", "inbound/tun[tun-in]: inbound connection "
                              "from 172.19.0.1:55058");
    view.appendApiLog("info", "outbound/vless[JP]: outbound connection "
                              "to 104.18.95.41:443");
    QTRY_COMPARE(view.findChildren<LogRowWidget*>().size(), 5);
    QStringList directions;
    QStringList networks;
    QStringList contents;
    for (auto* row : view.findChildren<LogRowWidget*>()) {
      const auto badges = row->findChildren<QLabel*>("LogBadge");
      QCOMPARE(badges.size(), 3);
      directions.append(badges.at(1)->text());
      networks.append(badges.at(2)->text());
      contents.append(row->findChild<QLabel*>("LogContent")->text());
    }
    QCOMPARE(directions, QStringList({"Inbound", "Inbound", "Outbound",
                                      "Inbound", "Outbound"}));
    QCOMPARE(networks, QStringList({"UDP", "UDP", "UDP", "TCP", "TCP"}));
    QCOMPARE(contents, QStringList({"tun[tun-in] <- 172.19.0.1:55057",
                                    "tun[tun-in] <- 104.18.95.41:443",
                                    "vless[JP] -> 104.18.95.41:443",
                                    "tun[tun-in] <- 172.19.0.1:55058",
                                    "vless[JP] -> 104.18.95.41:443"}));
    view.clear();
  }
  void preservesReadingPosition() {
    LogView view(nullptr);
    view.resize(1600, 700);
    view.show();
    view.clear();
    QVERIFY(!view.findChild<QWidget*>("AutoScroll"));
    for (int i = 0; i < 250; ++i) {
      view.appendApiLog("info", QString("original-%1-end").arg(i));
    }
    QTRY_COMPARE(view.findChildren<LogRowWidget*>().size(), 50);
    auto* area = view.findChild<QScrollArea*>();
    auto* bar = area->verticalScrollBar();
    QTRY_VERIFY(bar->maximum() > 0);
    QTRY_COMPARE(bar->value(), bar->maximum());
    QTest::qWait(600);
    bar->setSliderPosition(bar->maximum() / 2);
    const int position = bar->value();
    auto contents = [&view]() {
      QString result;
      for (auto* label : view.findChildren<QLabel*>("LogContent")) {
        result += label->text() + '\n';
      }
      return result;
    };
    const QString before = contents();
    QPointer<QLabel> anchor;
    for (auto* label : view.findChildren<QLabel*>("LogContent")) {
      const int y = label->mapTo(area->viewport(), QPoint()).y();
      if (y >= 0 && y < area->viewport()->height()) {
        anchor = label;
        break;
      }
    }
    QVERIFY(anchor);
    const QPoint anchorPosition = anchor->mapTo(area->viewport(), QPoint());
    // Enough incoming rows to replace the entire visible window, with varying
    // heights to expose jumps that preserving only the scrollbar value misses.
    for (int i = 0; i < 600; ++i) {
      view.appendApiLog("info", QString("incoming-%1-end ").arg(i) +
                                    QString(i % 3 * 100, 'x'));
    }
    QTRY_COMPARE(view.findChild<QLabel*>("TotalTag")->text(), QString("850 entries"));
    QCOMPARE(contents(), before);
    QCOMPARE(view.findChildren<LogRowWidget*>().size(), 50);
    QCOMPARE(bar->value(), position);
    QVERIFY(anchor);
    QCOMPARE(anchor->mapTo(area->viewport(), QPoint()), anchorPosition);
    view.findChild<QPushButton*>("CopyBtn")->click();
    QVERIFY(QApplication::clipboard()->text().contains("original-249-end"));
    QVERIFY(!QApplication::clipboard()->text().contains("incoming-599-end"));
    QCOMPARE(contents(), before);
    QCOMPARE(view.findChildren<LogRowWidget*>().size(), 50);
    QCOMPARE(bar->value(), position);

    // Hold the thumb at the bottom: replacement must wait for release.
    bar->setSliderDown(true);
    bar->setSliderPosition(bar->maximum());
    QTest::qWait(700);
    QCOMPARE(contents(), before);
    bar->setSliderDown(false);
    QTRY_VERIFY(contents().contains("incoming-599-end"));
    QTRY_COMPARE(bar->value(), bar->maximum());
    QCOMPARE(view.findChildren<LogRowWidget*>().size(), 50);
    view.appendApiLog("info", "latest-after-resume");
    QTRY_VERIFY(contents().contains("latest-after-resume"));
    QTRY_COMPARE(bar->value(), bar->maximum());

    // An explicit search still replaces the frozen list with matching history.
    bar->setSliderPosition(bar->maximum() / 2);
    view.findChild<QLineEdit*>("SearchInput")->setText("original-0-end");
    QTRY_COMPARE(view.findChildren<LogRowWidget*>().size(), 1);
    QVERIFY(contents().contains("original-0-end"));
    view.clear();
    QCOMPARE(view.findChildren<LogRowWidget*>().size(), 0);
  }
  void repeatedHistorySwitchesReleaseRows() {
    QVERIFY(DatabaseService::instance().init());
    ThemeManager::instance().init();
    ThemeManager::instance().setThemeMode(ThemeManager::Light);
    LogView view(nullptr);
    view.setStyleSheet(ThemeManager::instance().getLogViewStyle());
    view.resize(1600, 700);
    view.show();
    view.clear();
    auto* store = view.findChild<LogStore*>();
    auto* bar = view.findChild<QScrollArea*>()->verticalScrollBar();
    QTimer* refresh = nullptr;
    for (auto* timer : view.findChildren<QTimer*>()) {
      if (timer->interval() == 500 && timer->isSingleShot()) refresh = timer;
    }
    QVERIFY(refresh);
    const int cycles = qMax(10, qEnvironmentVariableIntValue("SING_BOX_QT_MEMORY_CYCLES"));
    qsizetype expectedObjects = 0;
    for (int cycle = 0; cycle < cycles; ++cycle) {
      QVector<QPointer<LogRowWidget>> previous;
      for (auto* row : view.findChildren<LogRowWidget*>()) previous.append(row);
      if (!previous.isEmpty()) {
        QTRY_VERIFY(bar->maximum() > 0);
        bar->setSliderPosition(bar->maximum() / 2);
        QVERIFY(bar->value() < bar->maximum());
      }
      for (int i = 0; i < 300; ++i) {
        view.appendApiLog("info", QString("batch-%1-row-%2.example.test ")
                                      .arg(cycle).arg(i) + QString(100, 'x'));
      }
      QVERIFY(store->flush());
      QVERIFY(QMetaObject::invokeMethod(refresh, "timeout", Qt::DirectConnection));
      if (!previous.isEmpty()) {
        QCOMPARE(view.findChildren<LogRowWidget*>().size(), 50);
        for (const auto& row : previous) QVERIFY(row);
        bar->setSliderPosition(bar->maximum());
        QVERIFY(QMetaObject::invokeMethod(refresh, "timeout", Qt::DirectConnection));
      }
      QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
      QTest::qWait(50);
      view.repaint();
      QCOMPARE(view.findChildren<LogRowWidget*>().size(), 50);
      for (const auto& row : previous) QVERIFY(row.isNull());
      const auto objects = view.findChildren<QObject*>().size();
      if (cycle == 0) expectedObjects = objects;
      QCOMPARE(objects, expectedObjects);
      if (cycle == 0 || (cycle + 1) % 10 == 0) {
        qInfo("memory-probe cycles=%d rows=50 objects=%lld privateBytes=%lld",
              cycle + 1, qlonglong(objects), qlonglong(privateMemoryBytes()));
      }
    }
    view.clear();
    QCOMPARE(view.findChildren<LogRowWidget*>().size(), 0);
    qInfo("memory-probe cleared privateBytes=%lld", qlonglong(privateMemoryBytes()));
  }
  void keepsFollowingAfterDelayedLayoutChanges() {
    LogView view(nullptr);
    view.resize(1600, 700);
    view.show();
    view.clear();
    for (int i = 0; i < 50; ++i) {
      view.appendApiLog("info", QString("layout-%1 ").arg(i) + QString(200, 'x'));
    }
    auto* area = view.findChild<QScrollArea*>();
    auto* bar = area->verticalScrollBar();
    QTRY_COMPARE(view.findChildren<LogRowWidget*>().size(), 50);
    QTRY_VERIFY(bar->maximum() > 0);
    QTRY_COMPARE(bar->value(), bar->maximum());
    // Model a second layout pass after the previous scroll-to-bottom completed.
    bar->setMaximum(bar->maximum() + 7);
    QTRY_COMPARE_WITH_TIMEOUT(bar->value(), bar->maximum(), 1000);

    for (int i = 0; i < 4; ++i) {
      view.resize(i % 2 ? 1600 : 1100, i % 2 ? 700 : 550);
      view.appendApiLog("info", QString("resized-%1 ").arg(i) +
                                    QString(i % 2 ? 20 : 900, 'x'));
      QTRY_COMPARE(view.findChild<QLabel*>("TotalTag")->text(),
                   QString("%1 entries").arg(51 + i));
      QTRY_COMPARE(bar->value(), bar->maximum());
    }

    // An upward user action must cancel even a pending bottom adjustment.
    bar->setMaximum(bar->maximum() + 7);
    bar->triggerAction(QAbstractSlider::SliderPageStepSub);
    const int readingPosition = bar->value();
    QPointer<LogRowWidget> oldRow = view.findChildren<LogRowWidget*>().first();
    view.appendApiLog("info", "arrived-while-reading");
    QTRY_COMPARE(view.findChild<QLabel*>("TotalTag")->text(), QString("55 entries"));
    QCOMPARE(bar->value(), readingPosition);
    QVERIFY(oldRow);
    bar->triggerAction(QAbstractSlider::SliderToMaximum);
    QTRY_COMPARE(bar->value(), bar->maximum());
    QTRY_VERIFY(oldRow.isNull());
  }
 private:
  QByteArray m_previous;
  std::unique_ptr<QTemporaryDir> m_directory;
};

QTEST_MAIN(LogViewTest)
#include "log_view_test.moc"
