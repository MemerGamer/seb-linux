#ifndef SEB_WEB_MAIN_WINDOW_H
#define SEB_WEB_MAIN_WINDOW_H

#include <QtWidgets/QMainWindow>
#include <QtWebEngineWidgets/QWebEngineView>
#include <QtWebEngineCore/QWebEngineProfile>
#include "../core/Config.h"

namespace seb {
namespace core {
    struct Policy;
    class IdleInhibitor;
}

namespace web {

class RequestInterceptor;
class SecureWebEnginePage;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(const core::Policy& policy, const QString& quitPassword = QString(), QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void setupWebEngine();
    void loadStartUrl();
    bool isX11Session() const;
    void setupX11KeyGrabs();

    bool promptQuitPassword() const;

    QWebEngineView* m_webView;
    QWebEngineProfile* m_profile;
    RequestInterceptor* m_interceptor;
    core::Policy m_policy;
    core::IdleInhibitor* m_idleInhibitor;
    QString m_quitPassword;
    bool m_passwordVerified;
    bool m_isX11;
};

} // namespace web
} // namespace seb

#endif // SEB_WEB_MAIN_WINDOW_H

