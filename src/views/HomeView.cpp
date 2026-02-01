#include "HomeView.h"
#include "utils/ThemeManager.h"
#include "utils/home/HomeFormat.h"
#include "views/components/TrafficChart.h"
#include "widgets/ToggleSwitch.h"
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QMap>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QSignalBlocker>
#include <QStyle>
#include <QSvgRenderer>
#include <functional>

namespace {

QPixmap svgIconPixmap(const QString &resourcePath, int box, const QColor &color)
{
    const qreal dpr = qApp->devicePixelRatio();
    const int size = static_cast<int>(box * dpr);

    QSvgRenderer renderer(resourcePath);

    QPixmap base(size, size);
    base.fill(Qt::transparent);
    {
        QPainter p(&base);
        p.setRenderHint(QPainter::Antialiasing);
        QSizeF def = renderer.defaultSize();
        qreal w = def.width() > 0 ? def.width() : box;
        qreal h = def.height() > 0 ? def.height() : box;
        qreal ratio = (h > 0) ? w / h : 1.0;
        qreal renderW = size;
        qreal renderH = size;
        if (ratio > 1.0) {
            renderH = renderH / ratio;
        } else if (ratio < 1.0) {
            renderW = renderW * ratio;
        }
        QRectF target((size - renderW) / 2.0, (size - renderH) / 2.0, renderW, renderH);
        renderer.render(&p, target);
    }

    QPixmap tinted(size, size);
    tinted.fill(Qt::transparent);
    {
        QPainter p(&tinted);
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.drawPixmap(0, 0, base);
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(tinted.rect(), color);
    }
    tinted.setDevicePixelRatio(dpr);
    return tinted;
}


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
    // Recolor embedded svg icons if present.
    const QString accent = card->property("accent").toString();
    for (auto *label : card->findChildren<QLabel*>()) {
        const QVariant pathVar = label->property("iconPath");
        if (!pathVar.isValid()) continue;
        const QString iconPath = pathVar.toString();
        const int iconSize = label->property("iconSize").toInt();
        ThemeManager &tm = ThemeManager::instance();
        QColor color = active ? QColor(Qt::white) : tm.getColor("primary");
        label->setPixmap(svgIconPixmap(iconPath, iconSize > 0 ? iconSize : 20, color));
    }
    polishWidget(card);
    for (auto *child : card->findChildren<QWidget*>()) {
        polishWidget(child);
    }
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

    QLabel *titleLabel = new QLabel(tr("Home"));
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
    m_statusDot->setProperty("status", "stopped");

    m_statusText = new QLabel(tr("Stopped"));
    m_statusText->setObjectName("StatusText");

    badgeLayout->addWidget(m_statusDot);
    badgeLayout->addWidget(m_statusText);

    headerLayout->addWidget(titleLabel);
    headerLayout->addWidget(m_statusBadge);
    headerLayout->addStretch();



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

    QWidget *uploadCard = createStatCard("UP", "success", tr("Upload"), &m_uploadValue, &m_uploadTotal);
    QWidget *downloadCard = createStatCard("DOWN", "primary", tr("Download"), &m_downloadValue, &m_downloadTotal);
    QWidget *connectionsCard = createStatCard("CONN", "warning", tr("Connections"), &m_connectionsValue, &m_memoryLabel);

    m_uploadValue->setText(tr("0 B/s"));
    m_downloadValue->setText(tr("0 B/s"));
    m_connectionsValue->setText("0");
    m_uploadTotal->setText(tr("Total: 0 B"));
    m_downloadTotal->setText(tr("Total: 0 B"));
    m_memoryLabel->setText(tr("Memory usage: 0 B"));

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

    QLabel *flowTitle = new QLabel(tr("Traffic Proxy Mode"));
    flowTitle->setObjectName("SectionTitle");
    flowLayout->addWidget(flowTitle);

    m_systemProxySwitch = new ToggleSwitch;
    m_systemProxySwitch->setObjectName("ModeSwitch");
    m_systemProxySwitch->setCursor(Qt::PointingHandCursor);
    m_systemProxySwitch->setFixedSize(m_systemProxySwitch->sizeHint());

    m_tunModeSwitch = new ToggleSwitch;
    m_tunModeSwitch->setObjectName("ModeSwitch");
    m_tunModeSwitch->setCursor(Qt::PointingHandCursor);
    m_tunModeSwitch->setFixedSize(m_tunModeSwitch->sizeHint());

    m_systemProxyCard = createModeItem("SYS", "primary",
                                       tr("System Proxy"),
                                       tr("Auto-set system proxy"),
                                       m_systemProxySwitch);
    m_tunModeCard = createModeItem("TUN", "primary",
                                   tr("TUN Mode"),
                                   tr("Use TUN for system-wide proxy"),
                                   m_tunModeSwitch);

    flowLayout->addWidget(m_systemProxyCard);
    flowLayout->addWidget(m_tunModeCard);

    gridLayout->addWidget(flowSection, 1, 0);

    // Node mode section
    QWidget *nodeSection = new QWidget;
    QVBoxLayout *nodeLayout = new QVBoxLayout(nodeSection);
    nodeLayout->setContentsMargins(0, 0, 0, 0);
    nodeLayout->setSpacing(12);

    QLabel *nodeTitle = new QLabel(tr("Node Proxy Mode"));
    nodeTitle->setObjectName("SectionTitle");
    nodeLayout->addWidget(nodeTitle);

    m_globalModeSwitch = new ToggleSwitch;
    m_globalModeSwitch->setObjectName("ModeSwitch");
    m_globalModeSwitch->setCursor(Qt::PointingHandCursor);
    m_globalModeSwitch->setProperty("exclusiveSwitch", true);
    m_globalModeSwitch->setFixedSize(m_globalModeSwitch->sizeHint());

    m_ruleModeSwitch = new ToggleSwitch;
    m_ruleModeSwitch->setObjectName("ModeSwitch");
    m_ruleModeSwitch->setCursor(Qt::PointingHandCursor);
    m_ruleModeSwitch->setProperty("exclusiveSwitch", true);
    m_ruleModeSwitch->setFixedSize(m_ruleModeSwitch->sizeHint());

    m_globalModeCard = createModeItem("GLB", "primary",
                                      tr("Global Mode"),
                                      tr("All traffic via proxy"),
                                      m_globalModeSwitch);
    m_ruleModeCard = createModeItem("RULE", "primary",
                                    tr("Rule Mode"),
                                    tr("Smart routing by rules"),
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
    connect(m_systemProxySwitch, &ToggleSwitch::toggled, this, &HomeView::onSystemProxyToggled);
    connect(m_tunModeSwitch, &ToggleSwitch::toggled, this, &HomeView::onTunModeToggled);
    connect(m_globalModeSwitch, &ToggleSwitch::toggled, this, [this](bool checked) {
        if (checked) {
            onGlobalModeClicked();
        } else if (m_ruleModeSwitch && !m_ruleModeSwitch->isChecked()) {
            m_globalModeSwitch->setChecked(true);
        }
    });
    connect(m_ruleModeSwitch, &ToggleSwitch::toggled, this, [this](bool checked) {
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
    QString iconPath;
    if (iconText.compare("UP", Qt::CaseInsensitive) == 0) {
        iconPath = ":/icons/arrowup.svg";
    } else if (iconText.compare("DOWN", Qt::CaseInsensitive) == 0) {
        iconPath = ":/icons/arrowdown.svg";
    } else if (iconText.compare("CONN", Qt::CaseInsensitive) == 0) {
        iconPath = ":/icons/connect.svg";
    }
    if (!iconPath.isEmpty()) {
        ThemeManager &tm = ThemeManager::instance();
        QColor iconColor = tm.getColor("text-primary");
        if (accentKey == "success") iconColor = tm.getColor("success");
        else if (accentKey == "primary") iconColor = tm.getColor("primary");
        else if (accentKey == "warning") iconColor = tm.getColor("warning");
        iconLabel->setPixmap(svgIconPixmap(iconPath, 20, iconColor));
        iconLabel->setProperty("iconPath", iconPath);
        iconLabel->setProperty("iconSize", 20);
    } else {
        iconLabel->setText(iconText);
    }
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
    auto *card = new QFrame;
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
    QString iconPath;
    if (iconText.compare("TUN", Qt::CaseInsensitive) == 0) {
        iconPath = ":/icons/networktun.svg";
    } else if (iconText.compare("RULE", Qt::CaseInsensitive) == 0) {
        iconPath = ":/icons/arrowbranch.svg";
    } else if (iconText.compare("SYS", Qt::CaseInsensitive) == 0) {
        iconPath = ":/icons/network.svg";
    } else if (iconText.compare("GLB", Qt::CaseInsensitive) == 0) {
        iconPath = ":/icons/mappin.svg";
    }
    if (!iconPath.isEmpty()) {
        ThemeManager &tm = ThemeManager::instance();
        QColor iconColor = tm.getColor("primary");
        iconLabel->setPixmap(svgIconPixmap(iconPath, 20, iconColor));
        iconLabel->setProperty("iconPath", iconPath);
        iconLabel->setProperty("iconSize", 20);
    } else {
        iconLabel->setText(iconText);
    }
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

    if (auto toggle = qobject_cast<ToggleSwitch*>(control)) {
        connect(toggle, &ToggleSwitch::toggled, card, [card](bool checked) {
            setCardActive(card, checked);
        });
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

    QMap<QString, QString> extra;
    extra.insert("success-12", rgba(success, 0.12));
    extra.insert("warning-12", rgba(warning, 0.12));
    extra.insert("error-12", rgba(error, 0.12));
    extra.insert("success-18", rgba(success, 0.18));
    extra.insert("primary-18", rgba(primary, 0.18));
    extra.insert("warning-18", rgba(warning, 0.18));
    extra.insert("primary-06", rgba(primary, 0.06));

    setStyleSheet(tm.loadStyleSheet(":/styles/home_view.qss", extra));





    if (m_trafficChart) {
        m_trafficChart->updateStyle();
    }

    polishWidget(m_statusBadge);
    polishWidget(m_statusDot);
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
    m_statusText->setText(running ? tr("Running") : tr("Stopped"));
    m_statusBadge->setProperty("status", running ? "running" : "stopped");
    
    // Refresh style to update button color
    updateStyle();

    if (m_statusDot) {
        m_statusDot->setProperty("status", running ? "running" : "stopped");
        polishWidget(m_statusDot);
    }
    polishWidget(m_statusBadge);

    if (!running) {
        m_totalUpload = 0;
        m_totalDownload = 0;
        m_trafficTimer.invalidate();
        if (m_trafficChart) m_trafficChart->clear();
        if (m_uploadValue) m_uploadValue->setText(tr("0 B/s"));
        if (m_downloadValue) m_downloadValue->setText(tr("0 B/s"));
        if (m_uploadTotal) m_uploadTotal->setText(tr("Total: 0 B"));
        if (m_downloadTotal) m_downloadTotal->setText(tr("Total: 0 B"));
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

    if (m_uploadTotal) m_uploadTotal->setText(tr("Total: %1").arg(formatBytes(m_totalUpload)));
    if (m_downloadTotal) m_downloadTotal->setText(tr("Total: %1").arg(formatBytes(m_totalDownload)));
}

void HomeView::updateUptime(int seconds)
{
    if (!m_statusBadge) return;
    if (seconds <= 0) {
        m_statusBadge->setToolTip(QString());
        return;
    }
    m_statusBadge->setToolTip(tr("Uptime: %1").arg(formatDuration(seconds)));
}

void HomeView::updateConnections(int count, qint64 memoryUsage)
{
    if (m_connectionsValue) m_connectionsValue->setText(QString::number(count));
    if (m_memoryLabel) m_memoryLabel->setText(tr("Memory usage: %1").arg(formatBytes(memoryUsage)));
}

void HomeView::onSystemProxyToggled(bool checked)
{
    if (checked && m_tunModeSwitch && m_tunModeSwitch->isChecked()) {
        QSignalBlocker blocker(m_tunModeSwitch);
        m_tunModeSwitch->setChecked(false);
        setCardActive(m_tunModeCard, false);
        emit tunModeChanged(false);
    }

    setCardActive(m_systemProxyCard, checked);
    emit systemProxyChanged(checked);
}

void HomeView::onTunModeToggled(bool checked)
{
    if (checked && m_systemProxySwitch && m_systemProxySwitch->isChecked()) {
        QSignalBlocker blocker(m_systemProxySwitch);
        m_systemProxySwitch->setChecked(false);
        setCardActive(m_systemProxyCard, false);
        emit systemProxyChanged(false);
    }

    setCardActive(m_tunModeCard, checked);
    emit tunModeChanged(checked);
}

void HomeView::onGlobalModeClicked()
{
    if (m_ruleModeSwitch && m_ruleModeSwitch->isChecked()) {
        QSignalBlocker blocker(m_ruleModeSwitch);
        m_ruleModeSwitch->setChecked(false);
        setCardActive(m_ruleModeCard, false);
    }

    setCardActive(m_globalModeCard, true);
    emit proxyModeChanged("global");
}

void HomeView::onRuleModeClicked()
{
    if (m_globalModeSwitch && m_globalModeSwitch->isChecked()) {
        QSignalBlocker blocker(m_globalModeSwitch);
        m_globalModeSwitch->setChecked(false);
        setCardActive(m_globalModeCard, false);
    }

    setCardActive(m_ruleModeCard, true);
    emit proxyModeChanged("rule");
}

QString HomeView::formatBytes(qint64 bytes) const
{
    return HomeFormat::bytes(bytes);
}

QString HomeView::formatDuration(int seconds) const
{
    return HomeFormat::duration(seconds);
}
