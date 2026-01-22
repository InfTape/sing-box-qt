#ifndef RULESVIEW_H
#define RULESVIEW_H

#include <QVector>
#include <QWidget>

class QComboBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QFrame;
class ProxyService;

class RulesView : public QWidget
{
    Q_OBJECT

public:
    explicit RulesView(QWidget *parent = nullptr);
    void setProxyService(ProxyService *service);
    void refresh();

public slots:
    void updateStyle();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onSearchChanged();
    void onFilterChanged();
    void onRefreshClicked();
    void onEmptyActionClicked();

private:
    struct RuleItem {
        QString type;
        QString payload;
        QString proxy;
    };

    void setupUI();
    void applyFilters();
    void updateFilterOptions();
    void rebuildGrid();
    void updateEmptyState();
    QWidget* createRuleCard(const RuleItem &rule, int index);
    QString normalizeProxyValue(const QString &proxy) const;
    QString displayProxyLabel(const QString &proxy) const;
    QString ruleTagType(const QString &type) const;
    QString proxyTagType(const QString &proxy) const;

    ProxyService *m_proxyService = nullptr;

    QLabel *m_titleLabel = nullptr;
    QLabel *m_subtitleLabel = nullptr;
    QPushButton *m_refreshBtn = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QComboBox *m_typeFilter = nullptr;
    QComboBox *m_proxyFilter = nullptr;

    QScrollArea *m_scrollArea = nullptr;
    QWidget *m_gridContainer = nullptr;
    QGridLayout *m_gridLayout = nullptr;

    QFrame *m_emptyState = nullptr;
    QLabel *m_emptyTitle = nullptr;
    QPushButton *m_emptyAction = nullptr;

    QVector<RuleItem> m_rules;
    QVector<RuleItem> m_filteredRules;
    bool m_loading = false;
    int m_columnCount = 0;
};

#endif // RULESVIEW_H
