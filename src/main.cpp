#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QKeySequence>
#include <QIcon>

#include <KAboutData>
#include <KLocalizedString>
#include <KGlobalAccel>
#include <KActionCollection>

#include "ConfigManager.h"
#include "BackgroundProcessor.h"
#include "SettingsDialog.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("knowbridge");
    app.setQuitOnLastWindowClosed(false);
    app.setApplicationName(QStringLiteral("knowbridge"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("accessories-text-editor")));

    KAboutData about(QStringLiteral("knowbridge"),
                     i18n("Knowbridge"),
                     QStringLiteral("2.0"),
                     i18n("Modify text with an LLM via a global shortcut."),
                     KAboutLicense::LGPL_V3,
                     QStringLiteral("(c) 2025 Nikita Selin"));
    KAboutData::setApplicationData(about);

    ConfigManager cfg;
    BackgroundProcessor proc(&cfg);

    /* --- Tray icon ------------------------------------------------------ */
    QSystemTrayIcon tray(QIcon::fromTheme(QStringLiteral("accessories-text-editor")));
    QMenu trayMenu;
    QAction* actSettings = trayMenu.addAction(i18n("Settings…"));
    trayMenu.addSeparator();
    QAction* actQuit     = trayMenu.addAction(i18n("Quit"));
    tray.setContextMenu(&trayMenu);
    tray.show();

    QObject::connect(actSettings, &QAction::triggered, [&]{
        auto* dlg = new SettingsDialog(nullptr, &cfg);
        dlg->show();
    });
    QObject::connect(actQuit, &QAction::triggered,
                     &app, &QApplication::quit);

    /* --- Global shortcut ------------------------------------------------ */
    KActionCollection ac(&app);
    QAction* act = ac.addAction(QStringLiteral("process_text"));
    act->setText(i18n("Modify Text (AI)"));
    act->setIcon(QIcon::fromTheme(QStringLiteral("document-edit")));
    QObject::connect(act, &QAction::triggered,
                     &proc, &BackgroundProcessor::onShortcutActivated);
    KGlobalAccel::self()->setShortcut(act,
                                      { QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Space) });

    return app.exec();
}
