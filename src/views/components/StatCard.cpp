#include "StatCard.h"
#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>
#include <QVBoxLayout>
#include "app/interfaces/ThemeService.h"
namespace {
QPixmap svgIconPixmap(const QString& resourcePath, int box, const QColor& color) {
  const qreal  dpr  = qApp->devicePixelRatio();
  const int    size = static_cast<int>(box * dpr);
  QSvgRenderer renderer(resourcePath);
  QPixmap      base(size, size);
  base.fill(Qt::transparent);
  {
    QPainter p(&base);
    p.setRenderHint(QPainter::Antialiasing);
    const QSizeF def     = renderer.defaultSize();
    qreal        w       = def.width() > 0 ? def.width() : box;
    qreal        h       = def.height() > 0 ? def.height() : box;
    const qreal  ratio   = (h > 0) ? w / h : 1.0;
    qreal        renderW = size;
    qreal        renderH = size;
    if (ratio > 1.0) {
      renderH = renderH / ratio;
    } else if (ratio < 1.0) {
      renderW = renderW * ratio;
    }
    const QRectF target((size - renderW) / 2.0, (size - renderH) / 2.0, renderW, renderH);
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
QString resolveIconPath(const QString& iconText) {
  if (iconText.compare("UP", Qt::CaseInsensitive) == 0) {
    return ":/icons/arrowup.svg";
  }
  if (iconText.compare("DOWN", Qt::CaseInsensitive) == 0) {
    return ":/icons/arrowdown.svg";
  }
  if (iconText.compare("CONN", Qt::CaseInsensitive) == 0) {
    return ":/icons/connect.svg";
  }
  return QString();
}
}  // namespace
StatCard::StatCard(const QString& iconText, const QString& accentKey, const QString& title, ThemeService* themeService,
                   QWidget* parent)
    : QFrame(parent),
      m_iconText(iconText),
      m_accentKey(accentKey),
      m_iconPath(resolveIconPath(iconText)),
      m_themeService(themeService) {
  setupUi(title);
  updateStyle();
}
void StatCard::setupUi(const QString& title) {
  setObjectName("StatCard");
  setProperty("accent", m_accentKey);
  setMinimumHeight(96);
  auto* cardLayout = new QHBoxLayout(this);
  cardLayout->setContentsMargins(16, 16, 16, 16);
  cardLayout->setSpacing(14);
  auto* iconFrame = new QFrame(this);
  iconFrame->setObjectName("StatIcon");
  iconFrame->setProperty("accent", m_accentKey);
  iconFrame->setFixedSize(40, 40);
  auto* iconLayout = new QVBoxLayout(iconFrame);
  iconLayout->setContentsMargins(0, 0, 0, 0);
  m_iconLabel = new QLabel(m_iconText, iconFrame);
  m_iconLabel->setObjectName("StatIconLabel");
  m_iconLabel->setAlignment(Qt::AlignCenter);
  iconLayout->addWidget(m_iconLabel);
  auto* textLayout = new QVBoxLayout;
  textLayout->setSpacing(4);
  auto* titleLabel = new QLabel(title, this);
  titleLabel->setObjectName("StatTitle");
  m_valueLabel = new QLabel("0", this);
  m_valueLabel->setObjectName("StatValue");
  m_valueLabel->setProperty("accent", m_accentKey);
  m_subLabel = new QLabel("--", this);
  m_subLabel->setObjectName("StatDesc");
  textLayout->addWidget(titleLabel);
  textLayout->addWidget(m_valueLabel);
  textLayout->addWidget(m_subLabel);
  cardLayout->addWidget(iconFrame);
  cardLayout->addLayout(textLayout);
  cardLayout->addStretch();
}
void StatCard::applyIcon() {
  if (!m_iconLabel) return;
  if (m_iconPath.isEmpty()) {
    m_iconLabel->setText(m_iconText);
    return;
  }
  QColor iconColor = m_themeService ? m_themeService->color("text-primary") : QColor();
  if (m_accentKey == "success") {
    iconColor = m_themeService ? m_themeService->color("success") : QColor();
  } else if (m_accentKey == "primary") {
    iconColor = m_themeService ? m_themeService->color("primary") : QColor();
  } else if (m_accentKey == "warning") {
    iconColor = m_themeService ? m_themeService->color("warning") : QColor();
  }
  m_iconLabel->setPixmap(svgIconPixmap(m_iconPath, 20, iconColor));
}
void StatCard::updateStyle() {
  applyIcon();
}
void StatCard::setValueText(const QString& text) {
  if (m_valueLabel) m_valueLabel->setText(text);
}
void StatCard::setSubText(const QString& text) {
  if (m_subLabel) m_subLabel->setText(text);
}
