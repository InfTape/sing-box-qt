#ifndef LOGVIEW_H
#define LOGVIEW_H

#include <QDateTime>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QVBoxLayout;

class LogView : public QWidget
{
    Q_OBJECT

public:
    explicit LogView(QWidget *parent = nullptr);

    void appendLog(const QString &message);
    void clear();

public slots:
    void updateStyle();

private slots:
    void onFilterChanged();
    void onClearClicked();
    void onCopyClicked();
    void onExportClicked();

private:
    struct LogEntry {
        QString type;
        QString payload;
        QDateTime timestamp;
    };

    void setupUI();
    void rebuildList();
    void updateStats();
    QWidget* createLogRow(const LogEntry &entry);
    QString detectLogType(const QString &message) const;
    QString logTypeLabel(const QString &type) const;

    QLabel *m_titleLabel = nullptr;
    QLabel *m_subtitleLabel = nullptr;
    QLabel *m_totalTag = nullptr;
    QLabel *m_errorTag = nullptr;
    QLabel *m_warningTag = nullptr;

    QCheckBox *m_autoScroll = nullptr;
    QPushButton *m_clearBtn = nullptr;
    QPushButton *m_copyBtn = nullptr;
    QPushButton *m_exportBtn = nullptr;

    QLineEdit *m_searchEdit = nullptr;
    QComboBox *m_typeFilter = nullptr;

    QScrollArea *m_scrollArea = nullptr;
    QWidget *m_listContainer = nullptr;
    QVBoxLayout *m_listLayout = nullptr;
    QFrame *m_emptyState = nullptr;
    QLabel *m_emptyTitle = nullptr;

    QVector<LogEntry> m_logs;
    QVector<LogEntry> m_filtered;
};

#endif // LOGVIEW_H
