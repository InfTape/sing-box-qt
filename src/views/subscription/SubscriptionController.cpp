#include "SubscriptionController.h"
#include <QObject>
#include "network/SubscriptionService.h"

SubscriptionController::SubscriptionController(SubscriptionService* service)
    : m_service(service) {}

QList<SubscriptionInfo> SubscriptionController::subscriptions() const {
  return m_service ? m_service->getSubscriptions() : QList<SubscriptionInfo>{};
}

int SubscriptionController::activeIndex() const {
  return m_service ? m_service->getActiveIndex() : -1;
}

QString SubscriptionController::activeConfigPath() const {
  return m_service ? m_service->getActiveConfigPath() : QString();
}

QString SubscriptionController::currentConfig() const {
  return m_service ? m_service->getCurrentConfig() : QString();
}

void SubscriptionController::addUrl(const QString& url,
                                    const QString& name,
                                    bool           useOriginalConfig,
                                    int            autoUpdateIntervalMinutes,
                                    bool           applyRuntime,
                                    bool           enableSharedRules,
                                    const QStringList& ruleSets) {
  if (!m_service) {
    return;
  }
  m_service->addUrlSubscription(url,
                                name,
                                useOriginalConfig,
                                autoUpdateIntervalMinutes,
                                applyRuntime,
                                enableSharedRules,
                                ruleSets);
}

void SubscriptionController::addManual(const QString&     content,
                                       const QString&     name,
                                       bool               useOriginalConfig,
                                       bool               isUriList,
                                       bool               applyRuntime,
                                       bool               enableSharedRules,
                                       const QStringList& ruleSets) {
  if (!m_service) {
    return;
  }
  m_service->addManualSubscription(content,
                                   name,
                                   useOriginalConfig,
                                   isUriList,
                                   applyRuntime,
                                   enableSharedRules,
                                   ruleSets);
}

void SubscriptionController::refresh(const QString& id, bool applyRuntime) {
  if (!m_service) {
    return;
  }
  m_service->refreshSubscription(id, applyRuntime);
}

bool SubscriptionController::rollback(const QString& id) {
  if (!m_service) {
    return false;
  }
  return m_service->rollbackSubscriptionConfig(id);
}

void SubscriptionController::remove(const QString& id) {
  if (!m_service) {
    return;
  }
  m_service->removeSubscription(id);
}

bool SubscriptionController::addToActiveGroup(const QString& id,
                                              QString*       error) {
  if (!m_service) {
    if (error) {
      *error = QObject::tr("Subscription service unavailable.");
    }
    return false;
  }
  return m_service->addSubscriptionNodesToActiveGroup(id, error);
}

void SubscriptionController::updateSubscription(const QString& id,
                                                const QString& name,
                                                const QString& url,
                                                bool           isManual,
                                                const QString& content,
                                                bool useOriginalConfig,
                                                int  autoUpdateIntervalMinutes,
                                                bool enableSharedRules,
                                                const QStringList& ruleSets) {
  if (!m_service) {
    return;
  }
  m_service->updateSubscriptionMeta(id,
                                    name,
                                    url,
                                    isManual,
                                    content,
                                    useOriginalConfig,
                                    autoUpdateIntervalMinutes,
                                    enableSharedRules,
                                    ruleSets);
}

bool SubscriptionController::saveCurrentConfig(const QString& content,
                                               bool           applyRuntime) {
  if (!m_service) {
    return false;
  }
  return m_service->saveCurrentConfig(content, applyRuntime);
}
