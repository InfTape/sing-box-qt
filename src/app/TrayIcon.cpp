#include "TrayIcon.h"
#include <QApplication>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <utility>
#include "app/ProxyUiController.h"
#include "app/interfaces/ThemeService.h"
#include "core/KernelService.h"
#include "widgets/common/RoundedMenu.h"

TrayIcon::TrayIcon(ProxyUiController*    proxyUiController,
                   KernelService*        kernelService,
                   ThemeService*         themeService,
                   std::function<void()> showWindow,
                   QObject*              parent)
    : QSystemTrayIcon(parent),
      m_proxyUiController(proxyUiController),
      m_kernelService(kernelService),
      m_showWindow(std::move(showWindow)),
      m_themeService(themeService) {
  setIcon(QIcon(":/icons/app.png"));
  setToolTip(tr("Sing-Box - Stopped"));
  setupMenu();
  connect(this, &QSystemTrayIcon::activated, this, &TrayIcon::onActivated);
  if (m_kernelService) {
    connect(m_kernelService, &KernelService::statusChanged, this, [this](bool) {
      updateProxyStatus();
    });
  }
  if (m_proxyUiController) {
    connect(m_proxyUiController,
            &ProxyUiController::proxyModeChanged,
            this,
            [this](const QString&) { updateProxyStatus(); });
  }
}

TrayIcon::~TrayIcon() {
  delete m_menu;
}

void TrayIcon::setupMenu() {
  auto* menu = new RoundedMenu;
  menu->setObjectName("TrayMenu");
  ThemeService* ts = m_themeService;
  if (ts) {
    menu->setThemeColors(ts->color("bg-secondary"), ts->color("primary"));
    connect(ts, &ThemeService::themeChanged, menu, [menu, ts]() {
      menu->setThemeColors(ts->color("bg-secondary"), ts->color("primary"));
    });
  }
  m_menu       = menu;
  m_showAction = m_menu->addAction(tr("Show Window"));
  connect(m_showAction, &QAction::triggered, this, &TrayIcon::onShowWindow);
  m_menu->addSeparator();
  m_globalAction = m_menu->addAction(tr("Global Mode"));
  m_ruleAction   = m_menu->addAction(tr("Rule Mode"));
  m_globalAction->setCheckable(true);
  m_ruleAction->setCheckable(true);
  connect(m_globalAction, &QAction::triggered, this, &TrayIcon::onSelectGlobal);
  connect(m_ruleAction, &QAction::triggered, this, &TrayIcon::onSelectRule);
  m_menu->addSeparator();
  m_quitAction = m_menu->addAction(tr("Quit"));
  connect(m_quitAction, &QAction::triggered, this, &TrayIcon::onQuit);
  connect(m_menu, &QMenu::aboutToShow, this, &TrayIcon::updateProxyStatus);
  setContextMenu(m_menu);
}

void TrayIcon::onActivated(QSystemTrayIcon::ActivationReason reason) {
  switch (reason) {
    case QSystemTrayIcon::Trigger:
    case QSystemTrayIcon::DoubleClick:
      onShowWindow();
      break;
    default:
      break;
  }
}

void TrayIcon::onShowWindow() {
  if (m_showWindow) {
    m_showWindow();
  }
}

void TrayIcon::onSelectGlobal() {
  if (!m_proxyUiController) {
    return;
  }
  QString error;
  if (!m_proxyUiController->switchProxyMode("global", &error)) {
    QMessageBox::warning(
        nullptr,
        tr("Switch Mode Failed"),
        error.isEmpty() ? tr("Failed to switch proxy mode") : error);
    return;
  }
  updateProxyStatus();
}

void TrayIcon::onSelectRule() {
  if (!m_proxyUiController) {
    return;
  }
  QString error;
  if (!m_proxyUiController->switchProxyMode("rule", &error)) {
    QMessageBox::warning(
        nullptr,
        tr("Switch Mode Failed"),
        error.isEmpty() ? tr("Failed to switch proxy mode") : error);
    return;
  }
  updateProxyStatus();
}

void TrayIcon::onQuit() {
  if (m_proxyUiController) {
    m_proxyUiController->prepareForExit();
  }
  QApplication::quit();
}

void TrayIcon::updateProxyStatus() {
  const bool running = m_proxyUiController
                           ? m_proxyUiController->isKernelRunning()
                           : (m_kernelService && m_kernelService->isRunning());
  setToolTip(running ? tr("Sing-Box - Running") : tr("Sing-Box - Stopped"));
  QString mode = "rule";
  if (m_proxyUiController) {
    mode = m_proxyUiController->currentProxyMode();
  }
  m_globalAction->setChecked(mode == "global");
  m_ruleAction->setChecked(mode != "global");
}
