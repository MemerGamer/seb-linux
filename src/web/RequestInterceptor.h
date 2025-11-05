#ifndef SEB_WEB_REQUEST_INTERCEPTOR_H
#define SEB_WEB_REQUEST_INTERCEPTOR_H

#include <QtWebEngineCore/QWebEngineUrlRequestInterceptor>
#include <QtCore/QStringList>

namespace seb {
namespace core {
    struct Policy;
}

namespace web {

class RequestInterceptor : public QWebEngineUrlRequestInterceptor {
    Q_OBJECT

public:
    explicit RequestInterceptor(const core::Policy& policy, QObject* parent = nullptr);

    void interceptRequest(QWebEngineUrlRequestInfo& info) override;

private:
    bool isDomainAllowed(const QString& host) const;
    
    QStringList m_allowedDomains;
    QString m_configKey;
    QString m_clientVersion;
    QString m_clientType;
    bool m_sendConfigKey;
};

} // namespace web
} // namespace seb

#endif // SEB_WEB_REQUEST_INTERCEPTOR_H

