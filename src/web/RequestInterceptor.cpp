#include "RequestInterceptor.h"
#include "../core/Config.h"
#include <QtWebEngineCore/QWebEngineUrlRequestInfo>
#include <QtCore/QUrl>
#include <QtCore/QDebug>

namespace seb {
namespace web {

RequestInterceptor::RequestInterceptor(const core::Policy& policy, QObject* parent)
    : QWebEngineUrlRequestInterceptor(parent)
    , m_allowedDomains(policy.allowedDomains)
    , m_configKey("stub-value")
    , m_clientVersion(policy.getClientVersion())
    , m_clientType(policy.getClientType())
    , m_sendConfigKey(policy.sendConfigKey)
{
}

void RequestInterceptor::interceptRequest(QWebEngineUrlRequestInfo& info) {
    QUrl url = info.requestUrl();
    QString host = url.host();

    // Check if domain is allowed
    if (!isDomainAllowed(host)) {
        qWarning() << "Blocking request to non-allowed domain:" << host;
        info.block(true);
        return;
    }

    // Inject SEB headers
    info.setHttpHeader("X-SafeExamBrowser", QByteArray("SEB-Linux-MVP"));
    
    // SEB standard headers (with values from config or defaults)
    info.setHttpHeader("X-SafeExamBrowser-RequestHash", QByteArray("placeholder-stub-request-hash"));
    info.setHttpHeader("X-SafeExamBrowser-ClientVersion", m_clientVersion.toUtf8());
    info.setHttpHeader("X-SafeExamBrowser-ClientType", m_clientType.toUtf8());
    info.setHttpHeader("X-SafeExamBrowser-ConfigVersion", QByteArray("2"));
    
    // Optional config key header (if enabled in policy)
    if (m_sendConfigKey) {
        info.setHttpHeader("X-SafeExamBrowser-ConfigKey", m_configKey.toUtf8());
    }
}

bool RequestInterceptor::isDomainAllowed(const QString& host) const {
    // Check exact match
    if (m_allowedDomains.contains(host)) {
        return true;
    }

    // Check subdomain match (e.g., "cdn.example.com" matches "example.com")
    for (const QString& allowedDomain : m_allowedDomains) {
        if (host == allowedDomain || host.endsWith("." + allowedDomain)) {
            return true;
        }
    }

    return false;
}

} // namespace web
} // namespace seb

