// --- File: src/BackgroundProcessor.h ---
#ifndef BACKGROUNDPROCESSOR_H
#define BACKGROUNDPROCESSOR_H

#include <QObject>
#include <QString>

class QClipboard;
class QMenu;
class QAction;

#include "ApiClient.h"
#include "TextAction.h"
#include "AccessibilityHelper.h" // ADD

// REMOVE X11 Window type
// typedef unsigned long Window;

class BackgroundProcessor : public QObject
{
Q_OBJECT

public:
    explicit BackgroundProcessor(QObject *parent = nullptr);
    ~BackgroundProcessor() override;

    Q_INVOKABLE void onShortcutActivated();

private Q_SLOTS:
    // Renamed internal step
    void showMenuAndPrepare();
    void onActionSelected(QAction *action);
    void handleApiResult(const QString &resultText);
    void handleApiError(const QString &errorMsg);
    // Slot to perform initialization after event loop starts
    void initialize();


private:
    void setupApiClient();
    void createActionMenu();
    void showNotification(const QString &title, const QString &text, bool isError = false);
    // Fallback method
    void fallbackToClipboard(const QString& text, const QString& reason);

    ApiClient *m_apiClient = nullptr;
    QClipboard *m_clipboard;
    QMenu *m_actionMenu;
    QString m_apiKey;

    // Replace X11Helper with AccessibilityHelper
    AccessibilityHelper m_accessibilityHelper;
    // Store info about the element targeted by the shortcut activation
    ElementInfo m_currentTargetInfo;

    // Store the specific action requested for the current operation
    TextAction m_currentAction;

};

#endif // BACKGROUNDPROCESSOR_H