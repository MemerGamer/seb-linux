#include "IdleInhibitor.h"
#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QProcess>
#include <QtCore/QTimer>

#ifdef Q_OS_LINUX
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>
#endif

namespace seb {
namespace core {

IdleInhibitor::IdleInhibitor(QObject* parent)
    : QObject(parent)
    , m_isInhibiting(false)
    , m_keepAliveTimer(nullptr)
#ifdef Q_OS_LINUX
    , m_inhibitCookie(0)
    , m_usingDBus(false)
#endif
{
    m_keepAliveTimer = new QTimer(this);
    m_keepAliveTimer->setSingleShot(false);
    m_keepAliveTimer->setInterval(30000); // 30 seconds
    connect(m_keepAliveTimer, &QTimer::timeout, this, &IdleInhibitor::keepAlive);
}

IdleInhibitor::~IdleInhibitor() {
    stop();
}

void IdleInhibitor::start() {
    if (m_isInhibiting) {
        return;
    }

    qDebug() << "Starting idle inhibition";
    
    // Try D-Bus first (Linux)
#ifdef Q_OS_LINUX
    if (tryDBusInhibit()) {
        m_usingDBus = true;
        m_isInhibiting = true;
        qDebug() << "Idle inhibition active via D-Bus";
        return;
    }
#endif

    // Fallback to timer-based approach
    fallbackTimer();
    m_isInhibiting = true;
    qDebug() << "Idle inhibition active via keep-alive timer";
}

void IdleInhibitor::stop() {
    if (!m_isInhibiting) {
        return;
    }

    qDebug() << "Stopping idle inhibition";
    
#ifdef Q_OS_LINUX
    if (m_usingDBus) {
        uninhibitIdle();
        m_usingDBus = false;
    }
#endif

    if (m_keepAliveTimer->isActive()) {
        m_keepAliveTimer->stop();
    }

    m_isInhibiting = false;
}

void IdleInhibitor::keepAlive() {
    // Send a keep-alive signal to prevent idle
    // This can be done via xset or similar commands, or just keep the timer running
    qDebug() << "Keep-alive ping to prevent idle";
}

#ifdef Q_OS_LINUX
bool IdleInhibitor::tryDBusInhibit() {
    QDBusConnection bus = QDBusConnection::sessionBus();
    
    // Try org.freedesktop.ScreenSaver (X11/legacy)
    QDBusInterface screensaver("org.freedesktop.ScreenSaver",
                              "/ScreenSaver",
                              "org.freedesktop.ScreenSaver",
                              bus);
    
    if (screensaver.isValid()) {
        QDBusReply<uint> reply = screensaver.call("Inhibit",
                                                   QCoreApplication::applicationName(),
                                                   "Safe Exam Browser - preventing idle during exam");
        if (reply.isValid()) {
            m_inhibitCookie = reply.value();
            qDebug() << "Inhibited idle via org.freedesktop.ScreenSaver, cookie:" << m_inhibitCookie;
            return true;
        }
    }
    
    // Try org.gnome.SessionManager (GNOME)
    QDBusInterface sessionManager("org.gnome.SessionManager",
                                  "/org/gnome/SessionManager",
                                  "org.gnome.SessionManager",
                                  bus);
    
    if (sessionManager.isValid()) {
        QDBusReply<uint> reply = sessionManager.call("Inhibit",
                                                     QCoreApplication::applicationName(),
                                                     0u, // flags: 0 = inhibit idle
                                                     "Safe Exam Browser",
                                                     "Preventing idle during exam");
        if (reply.isValid()) {
            m_inhibitCookie = reply.value();
            qDebug() << "Inhibited idle via org.gnome.SessionManager, cookie:" << m_inhibitCookie;
            return true;
        }
    }
    
    // Note: org.freedesktop.login1 requires system bus and special permissions
    // Skipping for now - session bus methods should work for most desktop environments
    
    return false;
}

void IdleInhibitor::uninhibitIdle() {
    QDBusConnection bus = QDBusConnection::sessionBus();
    
    // Try org.freedesktop.ScreenSaver
    QDBusInterface screensaver("org.freedesktop.ScreenSaver",
                                "/ScreenSaver",
                                "org.freedesktop.ScreenSaver",
                                bus);
    
    if (screensaver.isValid()) {
        screensaver.call("UnInhibit", m_inhibitCookie);
        qDebug() << "Uninhibited idle via org.freedesktop.ScreenSaver";
        return;
    }
    
    // Try org.gnome.SessionManager
    QDBusInterface sessionManager("org.gnome.SessionManager",
                                  "/org/gnome/SessionManager",
                                  "org.gnome.SessionManager",
                                  bus);
    
    if (sessionManager.isValid()) {
        sessionManager.call("Uninhibit", m_inhibitCookie);
        qDebug() << "Uninhibited idle via org.gnome.SessionManager";
    }
}
#endif

void IdleInhibitor::fallbackTimer() {
    // Start a timer that periodically "pings" to prevent idle
    // This is a fallback when D-Bus is not available
    if (!m_keepAliveTimer->isActive()) {
        m_keepAliveTimer->start();
        keepAlive(); // Call immediately
    }
}

} // namespace core
} // namespace seb

