#ifndef SEB_CORE_IDLE_INHIBITOR_H
#define SEB_CORE_IDLE_INHIBITOR_H

#include <QtCore/QObject>
#include <QtCore/QTimer>

namespace seb {
namespace core {

class IdleInhibitor : public QObject {
    Q_OBJECT

public:
    explicit IdleInhibitor(QObject* parent = nullptr);
    ~IdleInhibitor() override;

    bool isInhibiting() const { return m_isInhibiting; }
    void start();
    void stop();

private slots:
    void keepAlive();

private:
    void inhibitIdle();
    void uninhibitIdle();
    bool tryDBusInhibit();
    void fallbackTimer();

    bool m_isInhibiting;
    QTimer* m_keepAliveTimer;
    
#ifdef Q_OS_LINUX
    uint m_inhibitCookie;
    bool m_usingDBus;
#endif
};

} // namespace core
} // namespace seb

#endif // SEB_CORE_IDLE_INHIBITOR_H

