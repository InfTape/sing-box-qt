#ifndef SYSTEMPROXY_H
#define SYSTEMPROXY_H

#include <QObject>
#include <QString>

class SystemProxy : public QObject
{
    Q_OBJECT

public:
    explicit SystemProxy(QObject *parent = nullptr);
    
    // 设置系统代理
    static bool setProxy(const QString &host, int port);
    static bool clearProxy();
    
    // 获取当前代理状态
    static bool isProxyEnabled();
    static QString getProxyHost();
    static int getProxyPort();
    
    // PAC 代理
    static bool setPacProxy(const QString &pacUrl);
    
    // 刷新系统代理设置
    static void refreshSettings();
};

#endif // SYSTEMPROXY_H
