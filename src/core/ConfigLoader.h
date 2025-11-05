#ifndef SEB_CORE_CONFIG_LOADER_H
#define SEB_CORE_CONFIG_LOADER_H

#include "Config.h"
#include <QtCore/QString>

namespace seb {
namespace core {

struct ConfigLoadResult {
    Policy policy;
    bool success;
    QString errorMessage;
    
    ConfigLoadResult() : success(false) {}
    ConfigLoadResult(const Policy& p) : policy(p), success(true) {}
    ConfigLoadResult(const QString& error) : success(false), errorMessage(error) {}
};

class ConfigLoader {
public:
    // Load policy from a JSON file
    // Returns ConfigLoadResult with success status and error message
    static ConfigLoadResult loadFromFile(const QString& filePath);
    
    // Legacy method for backward compatibility
    static Policy loadFromFileLegacy(const QString& filePath);
};

} // namespace core
} // namespace seb

#endif // SEB_CORE_CONFIG_LOADER_H

