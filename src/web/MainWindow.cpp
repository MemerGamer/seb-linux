#include "MainWindow.h"
#include "RequestInterceptor.h"
#include "SecureWebEnginePage.h"
#include "../core/Config.h"
#include "../core/IdleInhibitor.h"
#include <QtWebEngineWidgets/QWebEngineView>
#include <QtWebEngineCore/QWebEngineProfile>
#include <QtWebEngineCore/QWebEngineDownloadRequest>
#include <QtWebEngineCore/QWebEngineSettings>
#include <QtCore/QUrl>
#include <QtCore/QDebug>
#include <QtGui/QKeyEvent>
#include <QtGui/QCloseEvent>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMessageBox>

namespace seb {
namespace web {

MainWindow::MainWindow(const core::Policy& policy, const QString& quitPassword, QWidget* parent)
    : QMainWindow(parent)
    , m_webView(nullptr)
    , m_profile(nullptr)
    , m_interceptor(nullptr)
    , m_policy(policy)
    , m_idleInhibitor(nullptr)
    , m_quitPassword(quitPassword)
    , m_passwordVerified(false)
    , m_isX11(false)
{
    // Detect X11 session
    m_isX11 = isX11Session();
    if (m_isX11) {
        qDebug() << "X11 session detected - enabling shortcut suppression";
    }
    
    if (!m_quitPassword.isEmpty()) {
        qDebug() << "Quit password protection enabled";
    }
    
    // Create idle inhibitor
    m_idleInhibitor = new core::IdleInhibitor(this);
    
    // Set window flags for fullscreen and frameless
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    
    // Create dedicated profile
    m_profile = new QWebEngineProfile("SEBProfile", this);
    
    // Setup web engine
    setupWebEngine();
    
    // Load start URL
    loadStartUrl();
    
    // Start idle inhibition
    m_idleInhibitor->start();
    
    // Show fullscreen
    showFullScreen();
    
    // Setup X11 key grabs if on X11
    if (m_isX11) {
        setupX11KeyGrabs();
    }
}

MainWindow::~MainWindow() {
    // Qt will clean up child objects
}

void MainWindow::setupWebEngine() {
    // Set user agent with suffix if configured
    if (!m_policy.userAgentSuffix.isEmpty()) {
        QString currentUA = m_profile->httpUserAgent();
        QString newUA = currentUA + " " + m_policy.userAgentSuffix;
        m_profile->setHttpUserAgent(newUA);
        qDebug() << "User-Agent set to:" << newUA;
    }
    
    // Create request interceptor
    m_interceptor = new RequestInterceptor(m_policy, this);
    m_profile->setUrlRequestInterceptor(m_interceptor);
    
    // Connect to download signal on profile to block downloads
    connect(m_profile, &QWebEngineProfile::downloadRequested, this, [](QWebEngineDownloadRequest* download) {
        qWarning() << "Download blocked:" << download->url().toString();
        download->cancel();
    });
    
    // Create secure web page with the profile
    SecureWebEnginePage* page = new SecureWebEnginePage(m_profile, 
                                                          m_policy.allowedDomains,
                                                          m_policy.startUrl,
                                                          this);
    
    // Create web view
    m_webView = new QWebEngineView(this);
    m_webView->setPage(page);
    
    // Disable context menu on the view
    m_webView->setContextMenuPolicy(Qt::NoContextMenu);
    
    // Get settings and disable features
    QWebEngineSettings* settings = m_webView->settings();
    
    // Disable specified features
    settings->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, false);
    settings->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard, false);
    settings->setAttribute(QWebEngineSettings::LocalStorageEnabled, false);
    settings->setAttribute(QWebEngineSettings::PluginsEnabled, false);
    settings->setAttribute(QWebEngineSettings::PdfViewerEnabled, false);
    settings->setAttribute(QWebEngineSettings::ScreenCaptureEnabled, false);
    
    // Keep JavaScript enabled for basic functionality, but lock down features
    settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    
    // Set as central widget
    setCentralWidget(m_webView);
    
    // Install event filter to block Ctrl+P (print)
    m_webView->installEventFilter(this);
}

void MainWindow::loadStartUrl() {
    if (!m_policy.isValid()) {
        qWarning() << "Invalid policy, cannot load start URL";
        return;
    }
    
    QUrl url(m_policy.startUrl);
    qDebug() << "Loading start URL:" << url.toString();
    m_webView->setUrl(url);
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        
        // Block Ctrl+P (print) and Ctrl+S (save) on all platforms
        if (keyEvent->modifiers() & Qt::ControlModifier) {
            if (keyEvent->key() == Qt::Key_P) {
                qWarning() << "Print blocked (Ctrl+P)";
                return true; // Block the event
            }
            if (keyEvent->key() == Qt::Key_S) {
                qWarning() << "Save blocked (Ctrl+S)";
                return true; // Block the event
            }
        }
        
        // X11-specific shortcut blocking
        if (m_isX11) {
            Qt::KeyboardModifiers mods = keyEvent->modifiers();
            int key = keyEvent->key();
            
            // Ctrl+L (address bar)
            if ((mods & Qt::ControlModifier) && key == Qt::Key_L) {
                qWarning() << "Shortcut blocked (Ctrl+L)";
                return true;
            }
            
            // Ctrl+T (new tab)
            if ((mods & Qt::ControlModifier) && key == Qt::Key_T) {
                qWarning() << "Shortcut blocked (Ctrl+T)";
                return true;
            }
            
            // Ctrl+N (new window)
            if ((mods & Qt::ControlModifier) && key == Qt::Key_N) {
                qWarning() << "Shortcut blocked (Ctrl+N)";
                return true;
            }
            
            // Ctrl+W (close tab/window)
            if ((mods & Qt::ControlModifier) && key == Qt::Key_W) {
                qWarning() << "Shortcut blocked (Ctrl+W)";
                return true;
            }
            
            // Ctrl+Shift+I (developer tools)
            if ((mods & (Qt::ControlModifier | Qt::ShiftModifier)) == (Qt::ControlModifier | Qt::ShiftModifier) && key == Qt::Key_I) {
                qWarning() << "Shortcut blocked (Ctrl+Shift+I)";
                return true;
            }
            
            // F11 (fullscreen toggle)
            if (key == Qt::Key_F11) {
                qWarning() << "Shortcut blocked (F11)";
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

bool MainWindow::isX11Session() const {
    QString sessionType = qEnvironmentVariable("XDG_SESSION_TYPE");
    return sessionType.toLower() == "x11";
}

void MainWindow::closeEvent(QCloseEvent* event) {
    // If quit password is set, require it to close (unless already verified)
    if (!m_quitPassword.isEmpty() && !m_passwordVerified) {
        if (!promptQuitPassword()) {
            event->ignore(); // Prevent closing
            return;
        }
        m_passwordVerified = true; // Mark as verified to avoid double prompt
    }
    
    event->accept();
    QMainWindow::closeEvent(event);
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
    Qt::KeyboardModifiers mods = event->modifiers();
    int key = event->key();
    
    // Handle quit password for Esc and Ctrl+Q
    if (!m_quitPassword.isEmpty()) {
        // Esc key
        if (key == Qt::Key_Escape) {
            if (promptQuitPassword()) {
                m_passwordVerified = true; // Mark as verified
                close(); // Will trigger closeEvent, but password already verified
            }
            event->accept();
            return;
        }
        
        // Ctrl+Q
        if ((mods & Qt::ControlModifier) && key == Qt::Key_Q) {
            if (promptQuitPassword()) {
                m_passwordVerified = true; // Mark as verified
                close(); // Will trigger closeEvent, but password already verified
            }
            event->accept();
            return;
        }
    }
    
    // X11-specific shortcut blocking at window level
    if (m_isX11) {
        // Ctrl+L (address bar)
        if ((mods & Qt::ControlModifier) && key == Qt::Key_L) {
            qWarning() << "Shortcut blocked (Ctrl+L)";
            event->accept();
            return;
        }
        
        // Ctrl+T (new tab)
        if ((mods & Qt::ControlModifier) && key == Qt::Key_T) {
            qWarning() << "Shortcut blocked (Ctrl+T)";
            event->accept();
            return;
        }
        
        // Ctrl+N (new window)
        if ((mods & Qt::ControlModifier) && key == Qt::Key_N) {
            qWarning() << "Shortcut blocked (Ctrl+N)";
            event->accept();
            return;
        }
        
        // Ctrl+W (close tab/window)
        if ((mods & Qt::ControlModifier) && key == Qt::Key_W) {
            qWarning() << "Shortcut blocked (Ctrl+W)";
            event->accept();
            return;
        }
        
        // Ctrl+Shift+I (developer tools)
        if ((mods & (Qt::ControlModifier | Qt::ShiftModifier)) == (Qt::ControlModifier | Qt::ShiftModifier) && key == Qt::Key_I) {
            qWarning() << "Shortcut blocked (Ctrl+Shift+I)";
            event->accept();
            return;
        }
        
        // F11 (fullscreen toggle)
        if (key == Qt::Key_F11) {
            qWarning() << "Shortcut blocked (F11)";
            event->accept();
            return;
        }
    }
    
    QMainWindow::keyPressEvent(event);
}

bool MainWindow::promptQuitPassword() const {
    if (m_quitPassword.isEmpty()) {
        return true; // No password required
    }
    
    bool ok = false;
    QString password = QInputDialog::getText(
        const_cast<MainWindow*>(this),
        "Quit Password Required",
        "Enter password to quit the application:",
        QLineEdit::Password,
        QString(),
        &ok
    );
    
    if (!ok) {
        // User cancelled the dialog
        return false;
    }
    
    if (password != m_quitPassword) {
        QMessageBox::warning(
            const_cast<MainWindow*>(this),
            "Incorrect Password",
            "The password you entered is incorrect. The application will not close."
        );
        return false;
    }
    
    return true;
}

void MainWindow::setupX11KeyGrabs() {
    // XGrabKey implementation can be added here for stronger blocking
    // This would require libX11 and the window to be visible first
    // For now, the eventFilter and keyPressEvent provide sufficient blocking
    qDebug() << "X11 key grabs - using eventFilter and keyPressEvent for shortcut blocking";
}

} // namespace web
} // namespace seb

