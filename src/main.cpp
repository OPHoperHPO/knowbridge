// --- File: src/main.cpp ---
#include <QApplication>
#include <QAction>
#include <QKeySequence>
#include <QIcon>
#include <QDebug>
#include <QTimer> // Include QTimer

// KDE Frameworks Headers
#include <KGlobalAccel>
#include <KActionCollection>
#include <KAboutData>
#include <KLocalizedString>
#include <QMenu>           // <-- ADDED
#include <QSystemTrayIcon> // <-- ADDED
#include <QMessageBox>     // <-- ADDED

// Local Project Headers
#include "BackgroundProcessor.h" // The core application logic

int main(int argc, char *argv[])
{
    // QApplication must be created before AT-SPI initialization attempts
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Knowbridge"));
    app.setApplicationVersion(QStringLiteral("1.1-atspi")); // Update version

    // --- KDE Application Setup ---
    KAboutData::setApplicationData(KAboutData(
            app.applicationName(), // Use name set on app
            QStringLiteral("Knowbridge (AT-SPI)"), // Update display name
            app.applicationVersion(),
            ki18n("Modifies text using OpenAI via global shortcut (uses AT-SPI).").toString(),
            KAboutLicense::LicenseKey::LGPL_V3,
            ki18n("(c) 2025 Nikita Selin").toString(),
            QString(),
            QStringLiteral("farvard34@gmail.com")
    ));

    // Needed for KNotifications ComponentData grouping
    // Use the component name from KAboutData for consistency
    app.setApplicationDisplayName(KAboutData::applicationData().componentName());


    QApplication::setQuitOnLastWindowClosed(false);

    // --- Create Core Processor ---
    // Initialization logic (AT-SPI, API Key) is now deferred
    // and triggered by a QTimer inside the BackgroundProcessor constructor.
    BackgroundProcessor processor;

    // The BackgroundProcessor::initialize slot will handle critical failures
    // and potentially call qApp->quit() if needed. No immediate check here.

    // --- Setup Global Shortcut Action ---
    KActionCollection actionCollection(&app);
    QAction *processAction = actionCollection.addAction(QStringLiteral("process_text_atspi")); // New ID?
    processAction->setText(ki18n("Modify Text (AI)").toString()); // Shorter text maybe
    processAction->setIcon(QIcon::fromTheme(QStringLiteral("document-edit"))); // More relevant icon?

    QObject::connect(processAction, &QAction::triggered, &processor, &BackgroundProcessor::onShortcutActivated);

    // Keep the same default shortcut or change if desired
    KGlobalAccel::self()->setShortcut(processAction, QList<QKeySequence>() << QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Space));

    // --- Setup System Tray Icon ---
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qWarning() << "System tray not available.";
        // Consider alternative ways to access settings/quit if tray fails
    }

    QSystemTrayIcon *trayIcon = new QSystemTrayIcon(&app);
    trayIcon->setIcon(app.windowIcon()); // Use the application icon
    trayIcon->setToolTip(ki18n("Knowbridge").toString());

    // Create Tray Menu
    QMenu *trayMenu = new QMenu();
  
    trayIcon->setContextMenu(trayMenu);
    trayIcon->show();

    qInfo() << "Knowbridge (AT-SPI) starting...";
    qInfo() << "Global shortcut registered for 'Modify Text (AI)'.";
    qInfo() << "Default shortcut: Ctrl+Alt+Space";
    qInfo() << "Configure in System Settings -> Shortcuts.";


    // --- Run Application Event Loop ---
    return app.exec();
}