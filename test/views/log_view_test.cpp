#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QTextLayout>
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
#include "widgets/logs/LogTextSelection.h"
#include "widgets/common/RoundedMenu.h"
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
void dragText(QLabel* first, const QPoint& start, QLabel* last,
              const QPoint& end) {
  QTest::mousePress(first, Qt::LeftButton, Qt::NoModifier, start);
  const QPoint globalEnd = last->mapToGlobal(end);
  const QPoint localEnd = first->mapFromGlobal(globalEnd);
  QMouseEvent move(QEvent::MouseMove, QPointF(localEnd), QPointF(globalEnd),
                   Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(first, &move);
  QTest::mouseRelease(first, Qt::LeftButton, Qt::NoModifier, localEnd);
}

QPoint textPoint(QLabel* label, int offset) {
  const QFontMetrics metrics(label->font());
  return label->contentsRect().topLeft() +
         QPoint(metrics.horizontalAdvance(label->text().left(offset)),
                metrics.height() / 2);
}

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
  void selectsAcrossLogBodies_data() {
    QTest::addColumn<bool>("reverse");
    QTest::newRow("forward") << false;
    QTest::newRow("reverse") << true;
  }
  void selectsAcrossLogBodies() {
    QFETCH(bool, reverse);
    LogView view(nullptr);
    view.resize(1600, 700);
    view.show();
    view.clear();
    view.appendApiLog("info", "first alpha");
    view.appendApiLog("warning", "middle beta");
    view.appendApiLog("error", "last gamma");
    QTRY_COMPARE(view.findChildren<LogRowWidget*>().size(), 3);
    const auto labels = view.findChildren<QLabel*>("LogContent");
    if (reverse) {
      dragText(labels[2], textPoint(labels[2], 4),
               labels[0], textPoint(labels[0], 6));
    } else {
      dragText(labels[0], textPoint(labels[0], 6),
               labels[2], textPoint(labels[2], 4));
    }
    const QString expected = "alpha\nmiddle beta\nlast";
    auto* selection = view.findChild<LogTextSelection*>();
    QCOMPARE(selection->selectedText(), expected);
    QCOMPARE(labels[0]->selectedText(), QString("alpha"));
    QCOMPARE(labels[1]->selectedText(), QString("middle beta"));
    QCOMPARE(labels[2]->selectedText(), QString("last"));
    for (auto* badge : view.findChildren<QLabel*>("LogBadge")) {
      QVERIFY(!badge->hasSelectedText());
    }
    auto* area = view.findChild<QScrollArea*>();
    QTest::keyClick(area->widget(), Qt::Key_C, Qt::ControlModifier);
    QCOMPARE(QApplication::clipboard()->text(), expected);
    QApplication::clipboard()->clear();
    view.findChild<QPushButton*>("CopyBtn")->click();
    QCOMPARE(QApplication::clipboard()->text(), expected);

    bool foundMenu = false;
    QTimer::singleShot(0, &view, [&]() {
      auto* menu = qobject_cast<RoundedMenu*>(QApplication::activePopupWidget());
      if (!menu) return;
      if (auto* action = menu->findChild<QAction*>("edit-copy")) {
        foundMenu = action->isEnabled();
        action->trigger();
      }
      menu->close();
    });
    QApplication::clipboard()->clear();
    QContextMenuEvent context(QContextMenuEvent::Mouse, QPoint(5, 5),
                              labels[1]->mapToGlobal(QPoint(5, 5)));
    QApplication::sendEvent(labels[1], &context);
    QVERIFY(foundMenu);
    QCOMPARE(QApplication::clipboard()->text(), expected);
    QTest::keyClick(area->widget(), Qt::Key_Escape);
    QVERIFY(!selection->isActive());
    view.clear();
  }

  void selectsWrappedUnicodeTextWithoutBadges() {
    LogView view(nullptr);
    view.resize(1000, 500);
    view.show();
    view.clear();
    const QString wrapped = QString::fromUtf8("中文🙂 <tag> & text ").repeated(50);
    view.appendApiLog("info", wrapped);
    view.appendApiLog("info", "outbound/vless[JP]: outbound packet connection "
                              "to 1.1.1.1:53");
    QTRY_COMPARE(view.findChildren<LogRowWidget*>().size(), 2);
    auto* area = view.findChild<QScrollArea*>();
    const auto labels = view.findChildren<QLabel*>("LogContent");
    QTextLayout layout(labels[0]->text(), labels[0]->font());
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(option);
    layout.beginLayout();
    auto firstLine = layout.createLine();
    firstLine.setLineWidth(labels[0]->contentsRect().width());
    auto secondLine = layout.createLine();
    QVERIFY(secondLine.isValid());
    secondLine.setLineWidth(labels[0]->contentsRect().width());
    const int start = secondLine.textStart();
    const QPoint secondLineStart(0, qRound(firstLine.height() +
                                           secondLine.height() / 2));
    layout.endLayout();
    dragText(labels[0], secondLineStart, labels[1],
             textPoint(labels[1], labels[1]->text().size()));
    QCOMPARE(view.findChild<LogTextSelection*>()->selectedText(),
              wrapped.trimmed().mid(start) + "\nvless[JP] -> 1.1.1.1:53");
    QTest::keyClick(area->widget(), Qt::Key_A, Qt::ControlModifier);
    QTest::keyClick(area->widget(), Qt::Key_C, Qt::ControlModifier);
    QCOMPARE(QApplication::clipboard()->text(),
              wrapped.trimmed() + "\nvless[JP] -> 1.1.1.1:53");
    view.clear();
    QVERIFY(!view.findChild<LogTextSelection*>()->isActive());
  }

  void keepsSelectionStableDuringNewLogs() {
    LogView view(nullptr);
    view.resize(1600, 700);
    view.show();
    view.clear();
    for (int i = 0; i < 50; ++i) {
      view.appendApiLog("info", QString("message-%1").arg(i));
    }
    QTRY_COMPARE(view.findChildren<LogRowWidget*>().size(), 50);
    auto* area = view.findChild<QScrollArea*>();
    auto* bar = area->verticalScrollBar();
    QTRY_COMPARE(bar->value(), bar->maximum());
    const auto labels = view.findChildren<QLabel*>("LogContent");
    dragText(labels[48], textPoint(labels[48], 0),
             labels[49], textPoint(labels[49], labels[49]->text().size()));
    auto* selection = view.findChild<LogTextSelection*>();
    const QString expected = "message-48\nmessage-49";
    QCOMPARE(selection->selectedText(), expected);
    QPointer<QLabel> first = labels.first();
    for (int i = 0; i < 200; ++i) view.appendApiLog("info", "new arrival");
    QTRY_COMPARE(view.findChild<QLabel*>("TotalTag")->text(), QString("250 entries"));
    QCOMPARE(view.findChildren<LogRowWidget*>().size(), 50);
    QVERIFY(first);
    QCOMPARE(selection->selectedText(), expected);
    QTest::keyClick(area->widget(), Qt::Key_Escape);
    QTRY_VERIFY(first.isNull());
    QCOMPARE(view.findChildren<LogRowWidget*>().size(), 50);
    QTRY_COMPARE(bar->value(), bar->maximum());
    view.clear();
  }

  void dragSelectionScrollsWithoutResumingLiveUpdates() {
    LogView view(nullptr);
    view.resize(1600, 700);
    view.show();
    view.clear();
    for (int i = 0; i < 50; ++i) {
      view.appendApiLog("info", QString("drag-scroll-%1").arg(i));
    }
    QTRY_COMPARE(view.findChildren<LogRowWidget*>().size(), 50);
    auto* area = view.findChild<QScrollArea*>();
    auto* bar = area->verticalScrollBar();
    QTRY_VERIFY(bar->maximum() > 0);
    QTRY_COMPARE(bar->value(), bar->maximum());
    const auto labels = view.findChildren<QLabel*>("LogContent");
    QPointer<QLabel> first = labels.first();
    auto* last = labels.last();
    QTest::mousePress(last, Qt::LeftButton, Qt::NoModifier, textPoint(last, 3));
    const QPoint global = area->viewport()->mapToGlobal(QPoint(400, -5));
    QMouseEvent move(QEvent::MouseMove, QPointF(last->mapFromGlobal(global)),
                     QPointF(global), Qt::NoButton, Qt::LeftButton,
                     Qt::NoModifier);
    QApplication::sendEvent(last, &move);
    QTRY_VERIFY(bar->value() < bar->maximum());
    QTest::mouseRelease(last, Qt::LeftButton, Qt::NoModifier,
                        last->mapFromGlobal(global));
    auto* selection = view.findChild<LogTextSelection*>();
    QVERIFY(!selection->selectedText().isEmpty());
    QTest::keyClick(area->widget(), Qt::Key_Escape);
    const int readingPosition = bar->value();
    for (int i = 0; i < 100; ++i) view.appendApiLog("info", "new while reading");
    QTRY_COMPARE(view.findChild<QLabel*>("TotalTag")->text(), QString("150 entries"));
    QCOMPARE(bar->value(), readingPosition);
    QVERIFY(first);
    bar->triggerAction(QAbstractSlider::SliderToMaximum);
    QTRY_VERIFY(first.isNull());
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
