#ifndef SEB_CORE_CONFIG_H
#define SEB_CORE_CONFIG_H

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QUrl>

namespace seb {
namespace core {

struct Policy {
    QString startUrl;              // Required: HTTPS URL
    QStringList allowedDomains;    // List of allowed domains
    QString userAgentSuffix;       // Optional: User agent suffix
    QString clientVersion;         // Optional: Client version (defaults to "0.1.0")
    QString clientType;           // Optional: Client type (defaults to "SEB-Linux")
    bool sendConfigKey = true;     // Default: true

    bool isValid() const {
        if (startUrl.isEmpty()) {
            return false;
        }
        QUrl url(startUrl);
        return url.isValid() && url.scheme() == "https";
    }
    
    QString getClientVersion() const {
        return clientVersion.isEmpty() ? QString("0.1.0") : clientVersion;
    }
    
    QString getClientType() const {
        return clientType.isEmpty() ? QString("SEB-Linux") : clientType;
    }
};

} // namespace core
} // namespace seb

#endif // SEB_CORE_CONFIG_H

