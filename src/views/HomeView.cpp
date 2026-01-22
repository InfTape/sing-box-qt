#include "HomeView.h"
#include "utils/ThemeManager.h"
#include "views/components/TrafficChart.h"
#include <QButtonGroup>
#include <QCheckBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QSignalBlocker>
#include <QStyle>
#include <functional>

namespace {

class ClickableFrame : public QFrame
{
public:
    explicit ClickableFrame(QWidget *parent = nullptr)
        : QFrame(parent)
    {
        setCursor(Qt::PointingHandCursor);
    }

    std::function<void()> onClick;

protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && onClick) {
            onClick();
        }
        QFrame::mouseReleaseEvent(event);
    }
};

QString rgba(const QColor &color, double alpha)
{
    return QString("rgba(%1, %2, %3, %4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(alpha);
}

void polishWidget(QWidget *widget)
{
    if (!widget) return;
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

void setCardActive(QWidget *card, bool active)
{
    if (!card) return;
    card->setProperty("active", active);
    polishWidget(card);
}

} // namespace

HomeView::HomeView(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    updateStyle();

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &HomeView::updateStyle);
}

void HomeView::setupUI()
{
    QHBoxLayout *rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    QWidget *pageContainer = new QWidget;
    pageContainer->setObjectName("PageContainer");
    pageContainer->setMaximumWidth(1200);

    QVBoxLayout *mainLayout = new QVBoxLayout(pageContainer);
    mainLayout->setContentsMargins(24, 16, 24, 16);
    mainLayout->setSpacing(20);

    rootLayout->addStretch();
    rootLayout->addWidget(pageContainer, 1);
    rootLayout->addStretch();
    rootLayout->setAlignment(pageContainer, Qt::AlignTop);

    // Header
    QWidget *headerWidget = new QWidget;
    QHBoxLayout *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(12);

    QLabel *titleLabel = new QLabel(tr("首页"));
    titleLabel->setObjectName("PageTitle");

    m_statusBadge = new QWidget;
    m_statusBadge->setObjectName("StatusBadge");
    m_statusBadge->setProperty("status", "stopped");
    QHBoxLayout *badgeLayout = new QHBoxLayout(m_statusBadge);
    badgeLayout->setContentsMargins(8, 4, 8, 4);
    badgeLayout->setSpacing(6);

    m_statusDot = new QWidget;
    m_statusDot->setObjectName("StatusDot");
    m_statusDot->setFixedSize(8, 8);

    m_statusText = new QLabel(tr("已停止"));
    m_statusText->setObjectName("StatusText");

    badgeLayout->addWidget(m_statusDot);
    badgeLayout->addWidget(m_statusText);

    headerLayout->addWidget(titleLabel);
    headerLayout->addWidget(m_statusBadge);
    headerLayout->addStretch();

    m_restartBtn = new QPushButton(tr("重启"));
    m_restartBtn->setObjectName("RestartButton");
    m_restartBtn->setCursor(Qt::PointingHandCursor);
    m_restartBtn->setFixedHeight(40);
    m_restartBtn->setProperty("danger", false);
    headerLayout->addWidget(m_restartBtn);

    mainLayout->addWidget(headerWidget);

    // Main grid
    QGridLayout *gridLayout = new QGridLayout;
    gridLayout->setHorizontalSpacing(24);
    gridLayout->setVerticalSpacing(24);

    QWidget *statsSection = new QWidget;
    QVBoxLayout *statsLayout = new QVBoxLayout(statsSection);
    statsLayout->setContentsMargins(0, 0, 0, 0);
    statsLayout->setSpacing(24);

    QGridLayout *statsRow = new QGridLayout;
    statsRow->setHorizontalSpacing(24);
    statsRow->setVerticalSpacing(24);

    QWidget *uploadCard = createStatCard("UP", "success", tr("上传"), &m_uploadValue, &m_uploadTotal);
    QWidget *downloadCard = createStatCard("DOWN", "primary", tr("下载"), &m_downloadValue, &m_downloadTotal);
    QWidget *connectionsCard = createStatCard("CONN", "warning", tr("连接"), &m_connectionsValue, &m_memoryLabel);

    m_uploadValue->setText(tr("0 B/s"));
    m_downloadValue->setText(tr("0 B/s"));
    m_connectionsValue->setText("0");
    m_uploadTotal->setText(tr("总计: 0 B"));
    m_downloadTotal->setText(tr("总计: 0 B"));
    m_memoryLabel->setText(tr("内存占用: 0 B"));

    statsRow->addWidget(uploadCard, 0, 0);
    statsRow->addWidget(downloadCard, 0, 1);
    statsRow->addWidget(connectionsCard, 0, 2);
    statsRow->setColumnStretch(0, 1);
    statsRow->setColumnStretch(1, 1);
    statsRow->setColumnStretch(2, 1);

    statsLayout->addLayout(statsRow);

    QFrame *chartCard = new QFrame;
    chartCard->setObjectName("ChartCard");
    QVBoxLayout *chartLayout = new QVBoxLayout(chartCard);
    chartLayout->setContentsMargins(12, 12, 12, 12);
    chartLayout->setSpacing(0);

    m_trafficChart = new TrafficChart;
    chartLayout->addWidget(m_trafficChart);
    statsLayout->addWidget(chartCard);

    gridLayout->addWidget(statsSection, 0, 0, 1, 2);

    // Flow mode section
    QWidget *flowSection = new QWidget;
    QVBoxLayout *flowLayout = new QVBoxLayout(flowSection);
    flowLayout->setContentsMargins(0, 0, 0, 0);
    flowLayout->setSpacing(12);

    QLabel *flowTitle = new QLabel(tr("流量代理模式"));
    flowTitle->setObjectName("SectionTitle");
    flowLayout->addWidget(flowTitle);

    m_systemProxySwitch = new QCheckBox;
    m_systemProxySwitch->setObjectName("ModeSwitch");
    m_systemProxySwitch->setCursor(Qt::PointingHandCursor);
    m_systemProxySwitch->setFocusPolicy(Qt::NoFocus);
    m_systemProxySwitch->setText(QString());
    m_systemProxySwitch->setFixedSize(40, 22);

    m_tunModeSwitch = new QCheckBox;
    m_tunModeSwitch->setObjectName("ModeSwitch");
    m_tunModeSwitch->setCursor(Qt::PointingHandCursor);
    m_tunModeSwitch->setFocusPolicy(Qt::NoFocus);
    m_tunModeSwitch->setText(QString());
    m_tunModeSwitch->setFixedSize(40, 22);

    m_systemProxyCard = createModeItem("SYS", "primary",
                                       tr("系统代理"),
                                       tr("自动设置系统代理"),
                                       m_systemProxySwitch);
    m_tunModeCard = createModeItem("TUN", "primary",
                                   tr("TUN 模式"),
                                   tr("使用 TUN 进行系统全局代理"),
                                   m_tunModeSwitch);

    flowLayout->addWidget(m_systemProxyCard);
    flowLayout->addWidget(m_tunModeCard);

    gridLayout->addWidget(flowSection, 1, 0);

    // Node mode section
    QWidget *nodeSection = new QWidget;
    QVBoxLayout *nodeLayout = new QVBoxLayout(nodeSection);
    nodeLayout->setContentsMargins(0, 0, 0, 0);
    nodeLayout->setSpacing(12);

    QLabel *nodeTitle = new QLabel(tr("节点代理模式"));
    nodeTitle->setObjectName("SectionTitle");
    nodeLayout->addWidget(nodeTitle);

    m_globalModeSwitch = new QCheckBox;
    m_globalModeSwitch->setObjectName("ModeSwitch");
    m_globalModeSwitch->setCursor(Qt::PointingHandCursor);
    m_globalModeSwitch->setFocusPolicy(Qt::NoFocus);
    m_globalModeSwitch->setText(QString());
    m_globalModeSwitch->setFixedSize(40, 22);
    m_globalModeSwitch->setProperty("exclusiveSwitch", true);

    m_ruleModeSwitch = new QCheckBox;
    m_ruleModeSwitch->setObjectName("ModeSwitch");
    m_ruleModeSwitch->setCursor(Qt::PointingHandCursor);
    m_ruleModeSwitch->setFocusPolicy(Qt::NoFocus);
    m_ruleModeSwitch->setText(QString());
    m_ruleModeSwitch->setFixedSize(40, 22);
    m_ruleModeSwitch->setProperty("exclusiveSwitch", true);

    QButtonGroup *modeGroup = new QButtonGroup(this);
    modeGroup->setExclusive(true);
    modeGroup->addButton(m_globalModeSwitch);
    modeGroup->addButton(m_ruleModeSwitch);

    m_globalModeCard = createModeItem("GLB", "primary",
                                      tr("全局模式"),
                                      tr("全部流量走代理"),
                                      m_globalModeSwitch);
    m_ruleModeCard = createModeItem("RULE", "primary",
                                    tr("规则模式"),
                                    tr("根据规则智能分流"),
                                    m_ruleModeSwitch);

    nodeLayout->addWidget(m_globalModeCard);
    nodeLayout->addWidget(m_ruleModeCard);

    gridLayout->addWidget(nodeSection, 1, 1);

    mainLayout->addLayout(gridLayout);
    mainLayout->addStretch();

    // Default state
    m_ruleModeSwitch->setChecked(true);
    setCardActive(m_ruleModeCard, true);
    setCardActive(m_globalModeCard, false);
    setCardActive(m_systemProxyCard, false);
    setCardActive(m_tunModeCard, false);

    // Signals
    connect(m_restartBtn, &QPushButton::clicked, this, &HomeView::restartClicked);
    connect(m_systemProxySwitch, &QCheckBox::toggled, this, &HomeView::onSystemProxyToggled);
    connect(m_tunModeSwitch, &QCheckBox::toggled, this, &HomeView::onTunModeToggled);
    connect(m_globalModeSwitch, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked) {
            onGlobalModeClicked();
        } else if (m_ruleModeSwitch && !m_ruleModeSwitch->isChecked()) {
            m_globalModeSwitch->setChecked(true);
        }
    });
    connect(m_ruleModeSwitch, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked) {
            onRuleModeClicked();
        } else if (m_globalModeSwitch && !m_globalModeSwitch->isChecked()) {
            m_ruleModeSwitch->setChecked(true);
        }
    });
}

QWidget* HomeView::createStatCard(const QString &iconText, const QString &accentKey,
                                 const QString &title, QLabel **valueLabel, QLabel **subLabel)
{
    QFrame *card = new QFrame;
    card->setObjectName("StatCard");
    card->setProperty("accent", accentKey);
    card->setMinimumHeight(96);

    QHBoxLayout *cardLayout = new QHBoxLayout(card);
    cardLayout->setContentsMargins(16, 16, 16, 16);
    cardLayout->setSpacing(14);

    QFrame *iconFrame = new QFrame;
    iconFrame->setObjectName("StatIcon");
    iconFrame->setProperty("accent", accentKey);
    iconFrame->setFixedSize(40, 40);
    QVBoxLayout *iconLayout = new QVBoxLayout(iconFrame);
    iconLayout->setContentsMargins(0, 0, 0, 0);

    QLabel *iconLabel = new QLabel(iconText);
    iconLabel->setObjectName("StatIconLabel");
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLayout->addWidget(iconLabel);

    QVBoxLayout *textLayout = new QVBoxLayout;
    textLayout->setSpacing(4);

    QLabel *titleLabel = new QLabel(title);
    titleLabel->setObjectName("StatTitle");

    QLabel *value = new QLabel("0");
    value->setObjectName("StatValue");
    value->setProperty("accent", accentKey);

    QLabel *desc = new QLabel("--");
    desc->setObjectName("StatDesc");

    textLayout->addWidget(titleLabel);
    textLayout->addWidget(value);
    textLayout->addWidget(desc);

    cardLayout->addWidget(iconFrame);
    cardLayout->addLayout(textLayout);
    cardLayout->addStretch();

    if (valueLabel) *valueLabel = value;
    if (subLabel) *subLabel = desc;

    return card;
}

QWidget* HomeView::createModeItem(const QString &iconText, const QString &accentKey,
                                 const QString &title, const QString &desc, QWidget *control)
{
    auto *card = new ClickableFrame;
    card->setObjectName("ModeCard");
    card->setProperty("active", false);
    card->setProperty("accent", accentKey);

    QHBoxLayout *layout = new QHBoxLayout(card);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(12);

    QFrame *iconFrame = new QFrame;
    iconFrame->setObjectName("ModeIcon");
    iconFrame->setProperty("accent", accentKey);
    iconFrame->setFixedSize(40, 40);
    QVBoxLayout *iconLayout = new QVBoxLayout(iconFrame);
    iconLayout->setContentsMargins(0, 0, 0, 0);

    QLabel *iconLabel = new QLabel(iconText);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setObjectName("ModeIconLabel");
    iconLayout->addWidget(iconLabel);

    QVBoxLayout *textLayout = new QVBoxLayout;
    textLayout->setSpacing(2);

    QLabel *titleLabel = new QLabel(title);
    titleLabel->setObjectName("ModeTitle");

    QLabel *descLabel = new QLabel(desc);
    descLabel->setObjectName("ModeDesc");
    descLabel->setWordWrap(true);

    textLayout->addWidget(titleLabel);
    textLayout->addWidget(descLabel);

    layout->addWidget(iconFrame);
    layout->addLayout(textLayout, 1);
    layout->addStretch();

    if (control) {
        layout->addWidget(control);
    }

    if (auto checkbox = qobject_cast<QCheckBox*>(control)) {
        const bool exclusive = checkbox->property("exclusiveSwitch").toBool();
        if (exclusive) {
            card->onClick = [checkbox]() {
                checkbox->setChecked(true);
            };
        } else {
            card->onClick = [checkbox]() {
                checkbox->toggle();
            };
        }
    }

    return card;
}

void HomeView::updateStyle()
{
    ThemeManager &tm = ThemeManager::instance();

    QColor primary = tm.getColor("primary");
    QColor success = tm.getColor("success");
    QColor warning = tm.getColor("warning");
    QColor error = tm.getColor("error");
    QColor primaryHover = primary.lighter(110);
    QColor errorHover = error.lighter(110);

    const QString style = QString(R"(
        #PageTitle {
            font-size: 32px;
            font-weight: 700;
            color: %1;
        }
        #StatusBadge {
            background-color: %2;
            color: %3;
            border-radius: 14px;
        }
        #StatusBadge[status="running"] {
            background-color: %4;
            color: %5;
        }
        #StatusBadge[status="pending"] {
            background-color: %6;
            color: %7;
        }
        #StatusBadge[status="stopped"] {
            background-color: %8;
            color: %9;
        }
        #StatusDot {
            min-width: 8px;
            max-width: 8px;
            min-height: 8px;
            max-height: 8px;
            border-radius: 4px;
            background-color: %3;
        }
        #StatusBadge[status="running"] #StatusDot { background-color: %5; }
        #StatusBadge[status="pending"] #StatusDot { background-color: %7; }
        #StatusBadge[status="stopped"] #StatusDot { background-color: %9; }
        #StatusText {
            font-size: 13px;
            font-weight: 600;
        }
        #RestartButton {
            border: none;
            border-radius: 12px;
            padding: 8px 20px;
            font-weight: 600;
            color: white;
        }
        #RestartButton[danger="true"] { background-color: %10; }
        #RestartButton[danger="true"]:hover { background-color: %11; }
        #RestartButton[danger="false"] { background-color: %12; }
        #RestartButton[danger="false"]:hover { background-color: %13; }

        #StatCard {
            background-color: %14;
            border: 1px solid %15;
            border-radius: 16px;
        }
        #StatCard:hover { border-color: %16; }
        #StatTitle {
            font-size: 12px;
            color: %17;
        }
        #StatValue {
            font-size: 20px;
            font-weight: 700;
            color: %1;
        }
        #StatDesc {
            font-size: 12px;
            color: %18;
        }
        #StatIcon {
            background-color: %26;
            border-radius: 10px;
        }
        #StatIconLabel {
            font-size: 10px;
            font-weight: 600;
        }
        #StatIcon[accent="success"] {
            background-color: %19;
            color: %20;
        }
        #StatIcon[accent="success"] #StatIconLabel { color: %20; }
        #StatValue[accent="success"] { color: %20; }
        #StatIcon[accent="primary"] {
            background-color: %22;
            color: %21;
        }
        #StatIcon[accent="primary"] #StatIconLabel { color: %21; }
        #StatValue[accent="primary"] { color: %21; }
        #StatIcon[accent="warning"] {
            background-color: %24;
            color: %23;
        }
        #StatIcon[accent="warning"] #StatIconLabel { color: %23; }
        #StatValue[accent="warning"] { color: %23; }

        #ChartCard {
            background-color: %14;
            border: 1px solid %15;
            border-radius: 16px;
        }
        #SectionTitle {
            font-size: 13px;
            font-weight: 600;
            color: %18;
            letter-spacing: 0.06em;
        }
        #ModeCard {
            background-color: %14;
            border: 1px solid %15;
            border-radius: 16px;
        }
        #ModeCard:hover { border-color: %16; }
        #ModeCard[active="true"] {
            border-color: %21;
            background-color: %25;
        }
        #ModeIcon {
            background-color: %26;
            color: %17;
            border-radius: 10px;
        }
        #ModeIconLabel {
            font-size: 11px;
            font-weight: 600;
            color: %17;
        }
        #ModeCard[active="true"] #ModeIconLabel { color: white; }
        #ModeCard[active="true"] #ModeIcon {
            background-color: %21;
            color: white;
        }
        #ModeTitle {
            font-size: 14px;
            font-weight: 600;
            color: %1;
        }
        #ModeDesc {
            font-size: 12px;
            color: %17;
        }
        #ModeSwitch { spacing: 0; outline: none; }
        #ModeSwitch:focus { outline: none; }
        #ModeSwitch::indicator {
            width: 36px;
            height: 20px;
            border-radius: 10px;
            background-color: %26;
            outline: none;
            image: none;
        }
        #ModeSwitch::indicator:checked { background-color: %21; image: none; }
        #ModeRadio { spacing: 0; outline: none; }
        #ModeRadio:focus { outline: none; }
        #ModeRadio::indicator {
            width: 18px;
            height: 18px;
            border-radius: 9px;
            border: 2px solid %15;
            background: transparent;
            outline: none;
            image: none;
        }
        #ModeRadio::indicator:focus { outline: none; }
        #ModeRadio::indicator:checked {
            border: 2px solid %21;
            background-color: %21;
            image: none;
        }
    )")
        .arg(tm.getColorString("text-primary"))
        .arg(tm.getColorString("bg-tertiary"))
        .arg(tm.getColorString("text-secondary"))
        .arg(rgba(success, 0.12))
        .arg(success.name())
        .arg(rgba(warning, 0.12))
        .arg(warning.name())
        .arg(rgba(error, 0.12))
        .arg(error.name())
        .arg(error.name())
        .arg(errorHover.name())
        .arg(primary.name())
        .arg(primaryHover.name())
        .arg(tm.getColorString("panel-bg"))
        .arg(tm.getColorString("border"))
        .arg(tm.getColorString("border-hover"))
        .arg(tm.getColorString("text-secondary"))
        .arg(tm.getColorString("text-tertiary"))
        .arg(rgba(success, 0.18))
        .arg(success.name())
        .arg(primary.name())
        .arg(rgba(primary, 0.18))
        .arg(warning.name())
        .arg(rgba(warning, 0.18))
        .arg(rgba(primary, 0.06))
        .arg(tm.getColorString("bg-tertiary"));

    setStyleSheet(style);

    if (m_statusDot) {
        const QColor dotColor = m_isRunning ? success : error;
        m_statusDot->setStyleSheet(QString("background-color: %1; border-radius: 4px;")
                                       .arg(dotColor.name()));
    }

    if (m_trafficChart) {
        m_trafficChart->updateStyle();
    }

    m_restartBtn->setProperty("danger", m_isRunning);
    polishWidget(m_restartBtn);
    polishWidget(m_statusBadge);
}

bool HomeView::isSystemProxyEnabled() const
{
    return m_systemProxySwitch && m_systemProxySwitch->isChecked();
}

void HomeView::setSystemProxyEnabled(bool enabled)
{
    if (!m_systemProxySwitch) return;
    QSignalBlocker blocker(m_systemProxySwitch);
    m_systemProxySwitch->setChecked(enabled);
    setCardActive(m_systemProxyCard, enabled);
}

bool HomeView::isTunModeEnabled() const
{
    return m_tunModeSwitch && m_tunModeSwitch->isChecked();
}

void HomeView::setTunModeEnabled(bool enabled)
{
    if (!m_tunModeSwitch) return;
    QSignalBlocker blocker(m_tunModeSwitch);
    m_tunModeSwitch->setChecked(enabled);
    setCardActive(m_tunModeCard, enabled);
}

void HomeView::setProxyMode(const QString &mode)
{
    const QString normalized = mode.trimmed().toLower();
    const bool useGlobal = (normalized == "global");

    if (m_globalModeSwitch) {
        QSignalBlocker blocker(m_globalModeSwitch);
        m_globalModeSwitch->setChecked(useGlobal);
    }
    if (m_ruleModeSwitch) {
        QSignalBlocker blocker(m_ruleModeSwitch);
        m_ruleModeSwitch->setChecked(!useGlobal);
    }

    setCardActive(m_globalModeCard, useGlobal);
    setCardActive(m_ruleModeCard, !useGlobal);
}

void HomeView::updateStatus(bool running)
{
    m_isRunning = running;
    m_statusText->setText(running ? tr("运行中") : tr("已停止"));
    m_statusBadge->setProperty("status", running ? "running" : "stopped");
    m_restartBtn->setProperty("danger", running);
    if (m_statusDot) {
        ThemeManager &tm = ThemeManager::instance();
        const QColor dotColor = running ? tm.getColor("success") : tm.getColor("error");
        m_statusDot->setStyleSheet(QString("background-color: %1; border-radius: 4px;")
                                       .arg(dotColor.name()));
    }
    polishWidget(m_statusBadge);
    polishWidget(m_restartBtn);

    if (!running) {
        m_totalUpload = 0;
        m_totalDownload = 0;
        m_trafficTimer.invalidate();
        if (m_trafficChart) m_trafficChart->clear();
        if (m_uploadValue) m_uploadValue->setText(tr("0 B/s"));
        if (m_downloadValue) m_downloadValue->setText(tr("0 B/s"));
        if (m_uploadTotal) m_uploadTotal->setText(tr("总计: 0 B"));
        if (m_downloadTotal) m_downloadTotal->setText(tr("总计: 0 B"));
    }
}

void HomeView::updateTraffic(qint64 upload, qint64 download)
{
    if (m_uploadValue) m_uploadValue->setText(formatBytes(upload) + "/s");
    if (m_downloadValue) m_downloadValue->setText(formatBytes(download) + "/s");
    if (m_trafficChart) m_trafficChart->updateData(upload, download);

    if (!m_trafficTimer.isValid()) {
        m_trafficTimer.start();
    } else {
        const qint64 elapsedMs = m_trafficTimer.restart();
        const double seconds = elapsedMs / 1000.0;
        if (seconds > 0) {
            m_totalUpload += static_cast<qint64>(upload * seconds);
            m_totalDownload += static_cast<qint64>(download * seconds);
        }
    }

    if (m_uploadTotal) m_uploadTotal->setText(tr("总计: %1").arg(formatBytes(m_totalUpload)));
    if (m_downloadTotal) m_downloadTotal->setText(tr("总计: %1").arg(formatBytes(m_totalDownload)));
}

void HomeView::updateUptime(int seconds)
{
    if (!m_statusBadge) return;
    if (seconds <= 0) {
        m_statusBadge->setToolTip(QString());
        return;
    }
    m_statusBadge->setToolTip(tr("运行时间: %1").arg(formatDuration(seconds)));
}

void HomeView::updateConnections(int count, qint64 memoryUsage)
{
    if (m_connectionsValue) m_connectionsValue->setText(QString::number(count));
    if (m_memoryLabel) m_memoryLabel->setText(tr("内存占用: %1").arg(formatBytes(memoryUsage)));
}

void HomeView::onSystemProxyToggled(bool checked)
{
    setCardActive(m_systemProxyCard, checked);
    emit systemProxyChanged(checked);
}

void HomeView::onTunModeToggled(bool checked)
{
    setCardActive(m_tunModeCard, checked);
    emit tunModeChanged(checked);
}

void HomeView::onGlobalModeClicked()
{
    setCardActive(m_globalModeCard, true);
    setCardActive(m_ruleModeCard, false);
    emit proxyModeChanged("global");
}

void HomeView::onRuleModeClicked()
{
    setCardActive(m_ruleModeCard, true);
    setCardActive(m_globalModeCard, false);
    emit proxyModeChanged("rule");
}

QString HomeView::formatBytes(qint64 bytes) const
{
    if (bytes <= 0) return "0 B";
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = bytes;

    while (size >= 1024 && unitIndex < 4) {
        size /= 1024;
        unitIndex++;
    }

    return QString::number(size, 'f', unitIndex > 0 ? 2 : 0) + " " + units[unitIndex];
}

QString HomeView::formatDuration(int seconds) const
{
    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs = seconds % 60;

    if (hours > 0) {
        return QString("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0'));
    }
    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(secs, 2, 10, QChar('0'));
}
