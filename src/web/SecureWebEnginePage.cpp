#include "SecureWebEnginePage.h"
#include <QtCore/QDebug>
#include <QtCore/QUrl>

namespace seb {
namespace web {

SecureWebEnginePage::SecureWebEnginePage(QWebEngineProfile* profile,
                                         const QStringList& allowedDomains,
                                         const QString& startUrl,
                                         QObject* parent)
    : QWebEnginePage(profile, parent)
    , m_allowedDomains(allowedDomains)
    , m_startUrl(startUrl)
{
    // Connect to print signal to block printing
    connect(this, &QWebEnginePage::printRequested, this, &SecureWebEnginePage::handlePrintRequested);
    
    // Suppress context menu by disabling it in settings
    suppressContextMenu();
}

bool SecureWebEnginePage::acceptNavigationRequest(const QUrl& url, NavigationType type, bool isMainFrame) {
    // Only check main frame navigations
    if (!isMainFrame) {
        return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
    }
    
    // Check if domain is allowed
    QString host = url.host();
    if (!host.isEmpty() && !isDomainAllowed(host)) {
        qWarning() << "Blocking navigation to non-allowed domain:" << host;
        showBlockPage(url.toString());
        return false; // Block the navigation
    }
    
    // Allow navigation (RequestInterceptor also blocks at request level)
    return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
}

QWebEnginePage* SecureWebEnginePage::createWindow(WebWindowType type) {
    // Block all popup windows
    qWarning() << "Popup window blocked";
    Q_UNUSED(type);
    return nullptr;
}

void SecureWebEnginePage::handlePrintRequested() {
    // Block printing - do nothing
    qWarning() << "Print request blocked";
}

void SecureWebEnginePage::suppressContextMenu() {
    // Disable context menu via JavaScript
    runJavaScript(
        "document.addEventListener('contextmenu', function(e) { e.preventDefault(); return false; });"
        "document.addEventListener('selectstart', function(e) { e.preventDefault(); return false; });"
    );
}

void SecureWebEnginePage::showBlockPage(const QString& blockedUrl) {
    QString html = generateBlockPageHtml(blockedUrl);
    setHtml(html, QUrl("seb://blocked"));
}

bool SecureWebEnginePage::isDomainAllowed(const QString& host) const {
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

QString SecureWebEnginePage::generateBlockPageHtml(const QString& blockedUrl) const {
    // Simple HTML escaping
    QString escapedBlockedUrl = blockedUrl;
    escapedBlockedUrl.replace("&", "&amp;");
    escapedBlockedUrl.replace("<", "&lt;");
    escapedBlockedUrl.replace(">", "&gt;");
    escapedBlockedUrl.replace("\"", "&quot;");
    escapedBlockedUrl.replace("'", "&#39;");
    
    QString escapedStartUrl = m_startUrl;
    escapedStartUrl.replace("&", "&amp;");
    escapedStartUrl.replace("<", "&lt;");
    escapedStartUrl.replace(">", "&gt;");
    escapedStartUrl.replace("\"", "&quot;");
    escapedStartUrl.replace("'", "&#39;");
    
    return QString(R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Access Restricted</title>
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
            margin: 0;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: #333;
        }
        .container {
            background: white;
            border-radius: 12px;
            padding: 40px;
            max-width: 500px;
            box-shadow: 0 10px 40px rgba(0, 0, 0, 0.2);
            text-align: center;
        }
        .icon {
            font-size: 64px;
            margin-bottom: 20px;
        }
        h1 {
            margin: 0 0 16px 0;
            color: #2d3748;
            font-size: 28px;
            font-weight: 600;
        }
        p {
            margin: 0 0 32px 0;
            color: #718096;
            font-size: 16px;
            line-height: 1.6;
        }
        .blocked-url {
            background: #f7fafc;
            border: 1px solid #e2e8f0;
            border-radius: 6px;
            padding: 12px;
            margin: 20px 0;
            font-family: 'Monaco', 'Menlo', monospace;
            font-size: 14px;
            color: #c53030;
            word-break: break-all;
        }
        .back-button {
            background: #667eea;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 14px 32px;
            font-size: 16px;
            font-weight: 600;
            cursor: pointer;
            transition: background 0.2s;
            text-decoration: none;
            display: inline-block;
        }
        .back-button:hover {
            background: #5568d3;
        }
        .back-button:active {
            background: #4a5bc4;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="icon">🚫</div>
        <h1>Access Restricted</h1>
        <p>This domain is not allowed in the current exam session.</p>
        <div class="blocked-url">%1</div>
        <a href="%2" class="back-button" id="backButton">Return to Exam</a>
    </div>
    <script>
        document.getElementById('backButton').addEventListener('click', function(e) {
            e.preventDefault();
            window.location.href = '%2';
        });
    </script>
</body>
</html>
    )").arg(escapedBlockedUrl, escapedStartUrl);
}

} // namespace web
} // namespace seb

