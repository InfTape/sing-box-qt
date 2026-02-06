#ifndef RUNTIMEUIBINDER_H
#define RUNTIMEUIBINDER_H
#include <memory>
class ProxyRuntimeController;
class HomeView;
class ConnectionsView;
class ProxyView;
class RulesView;
class LogView;
class QPushButton;

class RuntimeUiBinder {
 public:
  RuntimeUiBinder(ProxyRuntimeController* runtime, HomeView* home, ConnectionsView* connections, ProxyView* proxy,
                  RulesView* rules, LogView* log, QPushButton* startStopBtn);
  ~RuntimeUiBinder() = default;
  void bind();

 private:
  ProxyRuntimeController* m_runtime;
  HomeView*               m_home;
  ConnectionsView*        m_connections;
  ProxyView*              m_proxy;
  RulesView*              m_rules;
  LogView*                m_log;
  QPushButton*            m_startStopBtn;
};
#endif  // RUNTIMEUIBINDER_H
