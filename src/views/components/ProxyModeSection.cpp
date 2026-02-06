#include "ProxyModeSection.h"

#include <QApplication>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QSignalBlocker>
#include <QStyle>
#include <QSvgRenderer>
#include <QVBoxLayout>

#include "app/interfaces/ThemeService.h"
#include "widgets/common/ToggleSwitch.h"

namespace {
QPixmap svgIconPixmap(const QString& resourcePath, int box, const QColor& color) {
  const qreal dpr  = qApp->devicePixelRatio();
  const int   size = static_cast<int>(box * dpr);

  QSvgRenderer renderer(resourcePath);

  QPixmap base(size, size);
  base.fill(Qt::transparent);
  {
    QPainter p(&base);
    p.setRenderHint(QPainter::Antialiasing);
    QSizeF def     = renderer.defaultSize();
    qreal  w       = def.width() > 0 ? def.width() : box;
    qreal  h       = def.height() > 0 ? def.height() : box;
    qreal  ratio   = (h > 0) ? w / h : 1.0;
    qreal  renderW = size;
    qreal  renderH = size;
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

void polishWidget(QWidget* widget) {
  if (!widget) return;
  widget->style()->unpolish(widget);
  widget->style()->polish(widget);
  widget->update();
}
}  // namespace

ProxyModeSection::ProxyModeSection(ThemeService* themeService, QWidget* parent)
    : QWidget(parent), m_themeService(themeService) {
  setupUi();
}

void ProxyModeSection::setupUi() {
  auto* rootLayout = new QGridLayout(this);
  rootLayout->setContentsMargins(0, 0, 0, 0);
  rootLayout->setHorizontalSpacing(24);
  rootLayout->setVerticalSpacing(12);

  // Flow mode section
  auto* flowSection = new QWidget(this);
  auto* flowLayout  = new QVBoxLayout(flowSection);
  flowLayout->setContentsMargins(0, 0, 0, 0);
  flowLayout->setSpacing(12);

  auto* flowTitle = new QLabel(tr("Traffic Proxy Mode"), flowSection);
  flowTitle->setObjectName("SectionTitle");
  flowLayout->addWidget(flowTitle);

  m_systemProxySwitch = new ToggleSwitch(this, m_themeService);
  m_systemProxySwitch->setObjectName("ModeSwitch");
  m_systemProxySwitch->setCursor(Qt::PointingHandCursor);
  m_systemProxySwitch->setFixedSize(m_systemProxySwitch->sizeHint());

  m_tunModeSwitch = new ToggleSwitch(this, m_themeService);
  m_tunModeSwitch->setObjectName("ModeSwitch");
  m_tunModeSwitch->setCursor(Qt::PointingHandCursor);
  m_tunModeSwitch->setFixedSize(m_tunModeSwitch->sizeHint());

  m_systemProxyCard =
      createModeItem("SYS", "primary", tr("System Proxy"), tr("Auto-set system proxy"), m_systemProxySwitch);
  m_tunModeCard =
      createModeItem("TUN", "primary", tr("TUN Mode"), tr("Use TUN for system-wide proxy"), m_tunModeSwitch);

  flowLayout->addWidget(m_systemProxyCard);
  flowLayout->addWidget(m_tunModeCard);

  // Node mode section
  auto* nodeSection = new QWidget(this);
  auto* nodeLayout  = new QVBoxLayout(nodeSection);
  nodeLayout->setContentsMargins(0, 0, 0, 0);
  nodeLayout->setSpacing(12);

  auto* nodeTitle = new QLabel(tr("Node Proxy Mode"), nodeSection);
  nodeTitle->setObjectName("SectionTitle");
  nodeLayout->addWidget(nodeTitle);

  m_globalModeSwitch = new ToggleSwitch(this, m_themeService);
  m_globalModeSwitch->setObjectName("ModeSwitch");
  m_globalModeSwitch->setCursor(Qt::PointingHandCursor);
  m_globalModeSwitch->setProperty("exclusiveSwitch", true);
  m_globalModeSwitch->setFixedSize(m_globalModeSwitch->sizeHint());

  m_ruleModeSwitch = new ToggleSwitch(this, m_themeService);
  m_ruleModeSwitch->setObjectName("ModeSwitch");
  m_ruleModeSwitch->setCursor(Qt::PointingHandCursor);
  m_ruleModeSwitch->setProperty("exclusiveSwitch", true);
  m_ruleModeSwitch->setFixedSize(m_ruleModeSwitch->sizeHint());

  m_globalModeCard =
      createModeItem("GLB", "primary", tr("Global Mode"), tr("All traffic via proxy"), m_globalModeSwitch);
  m_ruleModeCard = createModeItem("RULE", "primary", tr("Rule Mode"), tr("Smart routing by rules"), m_ruleModeSwitch);

  nodeLayout->addWidget(m_globalModeCard);
  nodeLayout->addWidget(m_ruleModeCard);

  rootLayout->addWidget(flowSection, 0, 0);
  rootLayout->addWidget(nodeSection, 0, 1);
  rootLayout->setColumnStretch(0, 1);
  rootLayout->setColumnStretch(1, 1);

  m_ruleModeSwitch->setChecked(true);
  setCardActive(m_ruleModeCard, true);
  setCardActive(m_globalModeCard, false);
  setCardActive(m_systemProxyCard, false);
  setCardActive(m_tunModeCard, false);

  connect(m_systemProxySwitch, &ToggleSwitch::toggled, this, [this](bool checked) {
    setCardActive(m_systemProxyCard, checked);
    emit systemProxyChanged(checked);
  });

  connect(m_tunModeSwitch, &ToggleSwitch::toggled, this, [this](bool checked) {
    setCardActive(m_tunModeCard, checked);
    emit tunModeChanged(checked);
  });

  connect(m_globalModeSwitch, &ToggleSwitch::toggled, this, [this](bool checked) {
    if (checked) {
      if (m_ruleModeSwitch && m_ruleModeSwitch->isChecked()) {
        QSignalBlocker blocker(m_ruleModeSwitch);
        m_ruleModeSwitch->setChecked(false);
        setCardActive(m_ruleModeCard, false);
      }
      setCardActive(m_globalModeCard, true);
      emit proxyModeChanged("global");
    } else if (m_ruleModeSwitch && !m_ruleModeSwitch->isChecked()) {
      m_globalModeSwitch->setChecked(true);
    }
  });

  connect(m_ruleModeSwitch, &ToggleSwitch::toggled, this, [this](bool checked) {
    if (checked) {
      if (m_globalModeSwitch && m_globalModeSwitch->isChecked()) {
        QSignalBlocker blocker(m_globalModeSwitch);
        m_globalModeSwitch->setChecked(false);
        setCardActive(m_globalModeCard, false);
      }
      setCardActive(m_ruleModeCard, true);
      emit proxyModeChanged("rule");
    } else if (m_globalModeSwitch && !m_globalModeSwitch->isChecked()) {
      m_ruleModeSwitch->setChecked(true);
    }
  });
}

QWidget* ProxyModeSection::createModeItem(const QString& iconText, const QString& accentKey, const QString& title,
                                          const QString& desc, QWidget* control) {
  auto* card = new QFrame;
  card->setObjectName("ModeCard");
  card->setProperty("active", false);
  card->setProperty("accent", accentKey);

  auto* layout = new QHBoxLayout(card);
  layout->setContentsMargins(16, 14, 16, 14);
  layout->setSpacing(12);

  auto* iconFrame = new QFrame;
  iconFrame->setObjectName("ModeIcon");
  iconFrame->setProperty("accent", accentKey);
  iconFrame->setFixedSize(40, 40);
  auto* iconLayout = new QVBoxLayout(iconFrame);
  iconLayout->setContentsMargins(0, 0, 0, 0);

  auto* iconLabel = new QLabel(iconText);
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
    QColor iconColor = m_themeService ? m_themeService->color("primary") : QColor();
    iconLabel->setPixmap(svgIconPixmap(iconPath, 20, iconColor));
    iconLabel->setProperty("iconPath", iconPath);
    iconLabel->setProperty("iconSize", 20);
  } else {
    iconLabel->setText(iconText);
  }
  iconLayout->addWidget(iconLabel);

  auto* textLayout = new QVBoxLayout;
  textLayout->setSpacing(2);

  auto* titleLabel = new QLabel(title);
  titleLabel->setObjectName("ModeTitle");

  auto* descLabel = new QLabel(desc);
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

  return card;
}

void ProxyModeSection::setCardActive(QWidget* card, bool active) {
  if (!card) return;
  card->setProperty("active", active);

  for (auto* label : card->findChildren<QLabel*>()) {
    const QVariant pathVar = label->property("iconPath");
    if (!pathVar.isValid()) continue;
    const QString iconPath = pathVar.toString();
    const int     iconSize = label->property("iconSize").toInt();
    const QColor  color = active ? QColor(Qt::white) : (m_themeService ? m_themeService->color("primary") : QColor());
    label->setPixmap(svgIconPixmap(iconPath, iconSize > 0 ? iconSize : 20, color));
  }

  polishWidget(card);
  for (auto* child : card->findChildren<QWidget*>()) {
    polishWidget(child);
  }
}

bool ProxyModeSection::isSystemProxyEnabled() const {
  return m_systemProxySwitch && m_systemProxySwitch->isChecked();
}

void ProxyModeSection::setSystemProxyEnabled(bool enabled) {
  if (!m_systemProxySwitch) return;
  QSignalBlocker blocker(m_systemProxySwitch);
  m_systemProxySwitch->setChecked(enabled);
  setCardActive(m_systemProxyCard, enabled);
}

bool ProxyModeSection::isTunModeEnabled() const {
  return m_tunModeSwitch && m_tunModeSwitch->isChecked();
}

void ProxyModeSection::setTunModeEnabled(bool enabled) {
  if (!m_tunModeSwitch) return;
  QSignalBlocker blocker(m_tunModeSwitch);
  m_tunModeSwitch->setChecked(enabled);
  setCardActive(m_tunModeCard, enabled);
}

void ProxyModeSection::setProxyMode(const QString& mode) {
  const QString normalized = mode.trimmed().toLower();
  const bool    useGlobal  = (normalized == "global");

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

void ProxyModeSection::updateStyle() {
  setCardActive(m_systemProxyCard, isSystemProxyEnabled());
  setCardActive(m_tunModeCard, isTunModeEnabled());

  const bool useGlobal = m_globalModeSwitch && m_globalModeSwitch->isChecked();
  setCardActive(m_globalModeCard, useGlobal);
  setCardActive(m_ruleModeCard, !useGlobal);
}
