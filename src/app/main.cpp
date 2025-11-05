#include <QtWidgets/QApplication>
#include <QtCore/QCommandLineParser>
#include <QtCore/QCommandLineOption>
#include <QtCore/QDebug>
#include "../web/MainWindow.h"
#include "../core/ConfigLoader.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("seb-linux");
    app.setApplicationVersion("1.0.0");

    // Detect session type (Wayland/X11)
    QString sessionType = qEnvironmentVariable("XDG_SESSION_TYPE");
    if (sessionType.isEmpty()) {
        qDebug() << "Session type: unknown (XDG_SESSION_TYPE not set)";
    } else {
        sessionType = sessionType.toLower();
        if (sessionType == "wayland" || sessionType == "x11") {
            qDebug() << "Session type:" << sessionType;
        } else {
            qDebug() << "Session type:" << sessionType << "(unexpected value)";
        }
    }

    // Setup command-line parser
    QCommandLineParser parser;
    parser.setApplicationDescription("Safe Exam Browser for Linux");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption configOption(QStringList() << "c" << "config",
                                     "Path to JSON configuration file",
                                     "config-file");
    parser.addOption(configOption);

    QCommandLineOption quitPasswordOption("quit-password",
                                          "Password required to quit the application",
                                          "password");
    parser.addOption(quitPasswordOption);
    
    parser.process(app);

    // Get config file path
    QString configPath = parser.value(configOption);
    if (configPath.isEmpty()) {
        qCritical() << "Error: --config option is required";
        qCritical() << parser.helpText();
        return 1;
    }

    // Load configuration
    seb::core::ConfigLoadResult result = seb::core::ConfigLoader::loadFromFile(configPath);
    if (!result.success) {
        qCritical() << "Error: Failed to load configuration from:" << configPath;
        qCritical() << "Error details:" << result.errorMessage;
        return 1;
    }

    seb::core::Policy policy = result.policy;
    if (!policy.isValid()) {
        qCritical() << "Error: Configuration validation failed";
        qCritical() << "The startUrl field is invalid or missing";
        return 1;
    }

    qDebug() << "Configuration loaded successfully";
    qDebug() << "Start URL:" << policy.startUrl;
    qDebug() << "Allowed domains:" << policy.allowedDomains;
    if (!policy.clientVersion.isEmpty()) {
        qDebug() << "Client version:" << policy.clientVersion;
    }
    if (!policy.clientType.isEmpty()) {
        qDebug() << "Client type:" << policy.clientType;
    }

    // Get quit password if provided
    QString quitPassword = parser.value(quitPasswordOption);
    
    // Create and show main window
    seb::web::MainWindow window(policy, quitPassword);
    window.show();

    return app.exec();
}

