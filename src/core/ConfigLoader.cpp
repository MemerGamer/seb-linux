#include "ConfigLoader.h"
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QUrl>
#include <QtCore/QDebug>

namespace seb {
namespace core {

ConfigLoadResult ConfigLoader::loadFromFile(const QString& filePath) {
    Policy policy;

    // Open and read the file
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return ConfigLoadResult(QString("Failed to open config file: %1").arg(filePath));
    }

    QByteArray fileData = file.readAll();
    file.close();

    // Parse JSON
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(fileData, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        return ConfigLoadResult(QString("JSON parse error at offset %1: %2")
                                .arg(parseError.offset)
                                .arg(parseError.errorString()));
    }

    if (!doc.isObject()) {
        return ConfigLoadResult("Root JSON element is not an object");
    }

    QJsonObject root = doc.object();

    // Load startUrl (required, must be valid HTTPS URL)
    if (!root.contains("startUrl")) {
        return ConfigLoadResult("Missing required field: startUrl");
    }
    
    if (!root["startUrl"].isString()) {
        return ConfigLoadResult("Field 'startUrl' must be a string");
    }
    
    QString urlString = root["startUrl"].toString();
    if (urlString.isEmpty()) {
        return ConfigLoadResult("Field 'startUrl' cannot be empty");
    }
    
    QUrl url(urlString);
    if (!url.isValid()) {
        return ConfigLoadResult(QString("Invalid URL format: %1").arg(urlString));
    }
    
    if (url.scheme() != "https") {
        return ConfigLoadResult(QString("startUrl must use HTTPS scheme, got: %1")
                                .arg(url.scheme()));
    }
    
    policy.startUrl = urlString;

    // Load allowedDomains (optional array of strings)
    if (root.contains("allowedDomains")) {
        if (!root["allowedDomains"].isArray()) {
            return ConfigLoadResult("Field 'allowedDomains' must be an array");
        }
        QJsonArray domainsArray = root["allowedDomains"].toArray();
        for (const QJsonValue& value : domainsArray) {
            if (value.isString()) {
                policy.allowedDomains.append(value.toString());
            }
        }
    }

    // Load userAgentSuffix (optional string)
    if (root.contains("userAgentSuffix")) {
        if (!root["userAgentSuffix"].isString()) {
            return ConfigLoadResult("Field 'userAgentSuffix' must be a string");
        }
        policy.userAgentSuffix = root["userAgentSuffix"].toString();
    }

    // Load clientVersion (optional string)
    if (root.contains("clientVersion")) {
        if (!root["clientVersion"].isString()) {
            return ConfigLoadResult("Field 'clientVersion' must be a string");
        }
        policy.clientVersion = root["clientVersion"].toString();
    }

    // Load clientType (optional string)
    if (root.contains("clientType")) {
        if (!root["clientType"].isString()) {
            return ConfigLoadResult("Field 'clientType' must be a string");
        }
        policy.clientType = root["clientType"].toString();
    }

    // Load sendConfigKey (optional boolean, default true)
    if (root.contains("sendConfigKey")) {
        if (!root["sendConfigKey"].isBool()) {
            return ConfigLoadResult("Field 'sendConfigKey' must be a boolean");
        }
        policy.sendConfigKey = root["sendConfigKey"].toBool();
    }

    return ConfigLoadResult(policy);
}

Policy ConfigLoader::loadFromFileLegacy(const QString& filePath) {
    ConfigLoadResult result = loadFromFile(filePath);
    return result.policy;
}

} // namespace core
} // namespace seb

