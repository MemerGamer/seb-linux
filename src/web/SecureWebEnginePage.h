#ifndef SEB_WEB_SECURE_WEB_ENGINE_PAGE_H
#define SEB_WEB_SECURE_WEB_ENGINE_PAGE_H

#include <QtWebEngineCore/QWebEnginePage>
#include <QtWebEngineCore/QWebEngineProfile>
#include <QtCore/QStringList>
#include <QtCore/QString>

namespace seb {
namespace web {

class SecureWebEnginePage : public QWebEnginePage {
    Q_OBJECT

public:
    explicit SecureWebEnginePage(QWebEngineProfile* profile, 
                                   const QStringList& allowedDomains,
                                   const QString& startUrl,
                                   QObject* parent = nullptr);

protected:
    // Override context menu event to suppress it
    bool acceptNavigationRequest(const QUrl& url, NavigationType type, bool isMainFrame) override;
    
    // Create window for popups - return null to block
    QWebEnginePage* createWindow(WebWindowType type) override;

private slots:
    // Block printing
    void handlePrintRequested();

private:
    void suppressContextMenu();
    void showBlockPage(const QString& blockedUrl);
    bool isDomainAllowed(const QString& host) const;
    QString generateBlockPageHtml(const QString& blockedUrl) const;
    
    QStringList m_allowedDomains;
    QString m_startUrl;
};

} // namespace web
} // namespace seb

#endif // SEB_WEB_SECURE_WEB_ENGINE_PAGE_H

