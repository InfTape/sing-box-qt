#include <QHeaderView>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QKeySequence>
#include <QTimer>
#include <QtTest/QtTest>
#include "core/ProxyService.h"
#include "views/connections/ConnectionsView.h"
#include "widgets/common/RoundedMenu.h"

namespace {
QJsonObject connection(const QString& id, const QString& start,
                       const QString& text, qint64 bytes) {
  return {{"id", id},
          {"start", start},
          {"rule", text},
          {"upload", bytes},
          {"download", bytes},
          {"metadata", QJsonObject{{"sourceIP", text},
                                   {"host", text},
                                   {"network", text}}}};
}

QJsonArray connections() {
  // Arrival order differs from start order; timestamps include a UTC offset.
  return {connection("middle", "2026-09-05T08:00:02+08:00", "c", 900),
          connection("new", "2026-09-05T00:00:03Z", "b", 5000000000LL),
          connection("old", "2026-09-05T00:00:01Z", "a", 100)};
}

QStringList ids(QTableWidget* table) {
  QStringList result;
  for (int row = 0; row < table->rowCount(); ++row) {
    result.append(table->item(row, 0)->data(Qt::UserRole).toString());
  }
  return result;
}

void clickHeader(QTableWidget* table, int column) {
  auto* header = table->horizontalHeader();
  QTest::mouseClick(header->viewport(), Qt::LeftButton, Qt::NoModifier,
                    QPoint(header->sectionViewportPosition(column) +
                               header->sectionSize(column) / 2,
                           header->height() / 2));
}
}  // namespace

class ConnectionsViewTest : public QObject {
  Q_OBJECT
 private slots:
  void copiesFullCellTextDuringRefresh_data() {
    QTest::addColumn<int>("column");
    for (int column = 0; column < 6; ++column) {
      QTest::newRow(qPrintable(QString::number(column))) << column;
    }
  }

  void copiesFullCellTextDuringRefresh() {
    QFETCH(int, column);
    ProxyService service;
    ConnectionsView view(nullptr);
    view.setProxyService(&service);
    view.setAutoRefreshEnabled(true);
    view.resize(1000, 500);
    view.show();
    auto* table = view.findChild<QTableWidget*>();
    const QString longText = QString(160, 'x') + ".example.test";
    const auto conn = connection("copy-target", "", longText, 102400);
    emit service.connectionsReceived({{"connections", QJsonArray{conn}}});
    auto* cell = table->item(0, column);
    const QString expected = cell->text();
    table->scrollToItem(cell);
    QCoreApplication::processEvents();
    const QPoint position = table->visualItemRect(cell).center();
    bool foundMenu = false;
    bool foundCopy = false;
    QString copyText;
    QKeySequence copyShortcut;
    bool showsShortcut = false;
    QTimer::singleShot(0, &view, [&]() {
      auto* popup = qobject_cast<QMenu*>(QApplication::activePopupWidget());
      foundMenu = qobject_cast<RoundedMenu*>(popup) != nullptr;
      if (!popup) return;
      // Delete the clicked item while the menu is open, as a closed connection
      // would do. Copy must still use the text that was right-clicked.
      emit service.connectionsReceived({{"connections", QJsonArray{}}});
      for (auto* action : popup->actions()) {
        if (action->objectName() == "edit-copy") {
          foundCopy = true;
          copyText = action->text();
          copyShortcut = action->shortcut();
          showsShortcut = action->isShortcutVisibleInContextMenu();
          action->trigger();
          break;
        }
      }
      popup->close();
    });
    QContextMenuEvent event(QContextMenuEvent::Mouse, position,
                           table->viewport()->mapToGlobal(position));
    QApplication::sendEvent(table->viewport(), &event);
    QVERIFY(foundMenu);
    QVERIFY(foundCopy);
    QCOMPARE(copyText, QString("&Copy"));
    QCOMPARE(copyShortcut, QKeySequence(QKeySequence::Copy));
    QVERIFY(showsShortcut);
    QCOMPARE(QApplication::clipboard()->text(), expected);
    QCOMPARE(table->rowCount(), 0);
  }

  void emptySpaceDoesNotOpenCopyMenu() {
    ConnectionsView view(nullptr);
    view.resize(1000, 500);
    view.show();
    auto* table = view.findChild<QTableWidget*>();
    QApplication::clipboard()->setText("unchanged");
    bool openedMenu = false;
    QTimer::singleShot(0, &view, [&]() {
      if (auto* popup = qobject_cast<QMenu*>(QApplication::activePopupWidget())) {
        openedMenu = true;
        popup->close();
      }
    });
    const QPoint position(20, 100);
    QContextMenuEvent event(QContextMenuEvent::Mouse, position,
                           table->viewport()->mapToGlobal(position));
    QApplication::sendEvent(table->viewport(), &event);
    QCoreApplication::processEvents();
    QVERIFY(!openedMenu);
    QCOMPARE(QApplication::clipboard()->text(), QString("unchanged"));
  }

  void sortCycle_data() {
    QTest::addColumn<int>("column");
    for (int column = 0; column < 6; ++column) {
      QTest::newRow(qPrintable(QString::number(column))) << column;
    }
  }

  void sortCycle() {
    QFETCH(int, column);
    ProxyService service;
    ConnectionsView view(nullptr);
    view.setProxyService(&service);
    view.setAutoRefreshEnabled(true);
    view.resize(1000, 500);
    view.show();
    auto* table = view.findChild<QTableWidget*>();
    emit service.connectionsReceived({{"connections", connections()}});
    const QStringList newest{"new", "middle", "old"};
    const QStringList ascending = column < 4
                                      ? QStringList{"old", "new", "middle"}
                                      : QStringList{"old", "middle", "new"};
    const QStringList descending = column < 4
                                       ? QStringList{"middle", "new", "old"}
                                       : QStringList{"new", "middle", "old"};
    QCOMPARE(ids(table), newest);
    QVERIFY(!table->horizontalHeader()->isSortIndicatorShown());

    clickHeader(table, column);
    QCOMPARE(ids(table), ascending);
    QVERIFY(table->horizontalHeader()->isSortIndicatorShown());
    QCOMPARE(table->horizontalHeader()->sortIndicatorSection(), column);
    QCOMPARE(table->horizontalHeader()->sortIndicatorOrder(), Qt::AscendingOrder);
    // Refresh in a different API order must retain the requested sort.
    const auto data = connections();
    emit service.connectionsReceived(
        {{"connections", QJsonArray{data[2], data[0], data[1]}}});
    QCOMPARE(ids(table), ascending);
    clickHeader(table, column);
    QCOMPARE(ids(table), descending);
    QCOMPARE(table->horizontalHeader()->sortIndicatorOrder(), Qt::DescendingOrder);
    clickHeader(table, column);
    QCOMPARE(ids(table), newest);
    QVERIFY(!table->horizontalHeader()->isSortIndicatorShown());
    auto updated = connections();
    updated.append(connection("latest", "2026-09-05T00:00:04Z", "d", 200));
    emit service.connectionsReceived({{"connections", updated}});
    QCOMPARE(ids(table).first(), QString("latest"));
    clickHeader(table, column);
    QCOMPARE(table->horizontalHeader()->sortIndicatorOrder(), Qt::AscendingOrder);
    clickHeader(table, (column + 1) % 6);
    QCOMPARE(table->horizontalHeader()->sortIndicatorOrder(), Qt::AscendingOrder);
  }

  void trafficRefreshPreservesSelection() {
    ProxyService service;
    ConnectionsView view(nullptr);
    view.setProxyService(&service);
    view.setAutoRefreshEnabled(true);
    view.resize(1000, 500);
    view.show();
    auto* table = view.findChild<QTableWidget*>();
    emit service.connectionsReceived({{"connections", connections()}});
    table->selectRow(1);  // middle
    clickHeader(table, 5);
    clickHeader(table, 5);
    auto data = connections();
    auto middle = data[0].toObject();
    middle["download"] = 6000000000LL;
    data[0] = middle;
    emit service.connectionsReceived({{"connections", data}});
    QCOMPARE(ids(table), (QStringList{"middle", "new", "old"}));
    auto selected = table->selectionModel()->selectedRows();
    QCOMPARE(selected.size(), 1);
    QCOMPARE(selected.first().row(), 0);
    QCOMPARE(table->item(0, 1)->text(), QString("c"));
    QCOMPARE(table->item(0, 4)->text(), QString("0 KB"));
    QCOMPARE(table->item(0, 5)->text(), QString("5859375 KB"));
    data.removeAt(0);
    emit service.connectionsReceived({{"connections", data}});
    QVERIFY(table->selectionModel()->selectedRows().isEmpty());
    QCOMPARE(view.findChild<QPushButton*>()->text(), QString("Close All"));
    emit service.connectionsReceived({{"connections", QJsonArray{}}});
    QCOMPARE(table->rowCount(), 0);
    clickHeader(table, 5);
    QVERIFY(!table->horizontalHeader()->isSortIndicatorShown());
  }

  void sourceSortsByAddressThenNumericPort() {
    ProxyService service;
    ConnectionsView view(nullptr);
    view.setProxyService(&service);
    view.setAutoRefreshEnabled(true);
    view.resize(1000, 500);
    view.show();
    auto* table = view.findChild<QTableWidget*>();
    auto sourceConnection = [](const QString& id, const QString& ip,
                               const QJsonValue& port, bool snakeCase = false) {
      auto conn = connection(id, "2026-09-05T00:00:01Z", ip, 0);
      auto meta = conn["metadata"].toObject();
      meta[snakeCase ? "source_port" : "sourcePort"] = port;
      conn["metadata"] = meta;
      return conn;
    };
    QJsonArray data{
        sourceConnection("v4-port443", "10.0.0.2", "443"),
        sourceConnection("v6-ip16", "2001:db8::10", 1),
        sourceConnection("v4-ip10", "10.0.0.10", 1),
        sourceConnection("v4-port80", "10.0.0.2", 80),
        sourceConnection("v6-port80", "2001:db8:0:0:0:0:0:2", "80"),
        sourceConnection("v4-ip2", "2.0.0.1", 8000),
        sourceConnection("v4-port9", "10.0.0.2", "9", true),
        sourceConnection("v6-port9", "2001:db8::2", 9)};
    emit service.connectionsReceived({{"connections", data}});
    const QStringList newest = ids(table);
    const QStringList ascending{"v4-ip2", "v4-port9", "v4-port80",
                                "v4-port443", "v4-ip10", "v6-port9",
                                "v6-port80", "v6-ip16"};
    clickHeader(table, 0);
    QCOMPARE(ids(table), ascending);
    QCOMPARE(table->item(1, 0)->text(), QString("10.0.0.2:9"));
    emit service.connectionsReceived({{"connections", data}});
    QCOMPARE(ids(table), ascending);
    clickHeader(table, 0);
    QStringList descending;
    for (auto it = ascending.crbegin(); it != ascending.crend(); ++it) {
      descending.append(*it);
    }
    QCOMPARE(ids(table), descending);
    clickHeader(table, 0);
    QCOMPARE(ids(table), newest);
  }

  void textSortsAlphabeticallyIgnoringCase() {
    ProxyService service;
    ConnectionsView view(nullptr);
    view.setProxyService(&service);
    view.setAutoRefreshEnabled(true);
    view.resize(1000, 500);
    view.show();
    auto* table = view.findChild<QTableWidget*>();
    const QJsonArray data{connection("z", "", "Zebra", 0),
                          connection("a", "", "apple", 0),
                          connection("b", "", "Banana", 0)};
    emit service.connectionsReceived({{"connections", data}});
    for (int column = 1; column <= 3; ++column) {
      clickHeader(table, column);
      QCOMPARE(ids(table), (QStringList{"a", "b", "z"}));
      clickHeader(table, column);
      QCOMPARE(ids(table), (QStringList{"z", "b", "a"}));
    }
  }

  void missingStartUsesStableArrivalOrder() {
    ProxyService service;
    ConnectionsView view(nullptr);
    view.setProxyService(&service);
    view.setAutoRefreshEnabled(true);
    auto* table = view.findChild<QTableWidget*>();
    const auto first = connection("first", "", "a", 0);
    const auto second = connection("second", "invalid", "b", 0);
    emit service.connectionsReceived({{"connections", QJsonArray{first}}});
    emit service.connectionsReceived(
        {{"connections", QJsonArray{second, first}}});
    QCOMPARE(ids(table), (QStringList{"second", "first"}));
    emit service.connectionsReceived(
        {{"connections", QJsonArray{first, second}}});
    QCOMPARE(ids(table), (QStringList{"second", "first"}));
  }
};

QTEST_MAIN(ConnectionsViewTest)
#include "connections_view_test.moc"
