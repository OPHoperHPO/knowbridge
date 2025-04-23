// --- File: src/BackgroundProcessor.cpp ---
#include "BackgroundProcessor.h"

#include <QApplication>
#include <QClipboard>
#include <QMenu>
#include <QAction>
#include <QMetaEnum>
#include <QRegularExpression>
#include <QCursor>
#include <QTimer>
#include <QDebug>
#include <QStandardPaths>
#include <QCoreApplication> // For qApp

#ifdef HAVE_KNOTIFICATIONS
#include <KNotification>
#include <KLocalizedString> // Probably not strictly needed here but good practice for UI strings
#endif

// Constructor: Initialize members, schedule further init
BackgroundProcessor::BackgroundProcessor(QObject *parent)
        : QObject(parent),
          m_apiClient(nullptr),
          m_clipboard(QApplication::clipboard()),
          m_actionMenu(new QMenu()),
          m_accessibilityHelper(this) // Parent helper if needed
{
    // Basic setup that doesn't depend on external systems yet
    createActionMenu();

    // Schedule the main initialization (AT-SPI, API Key check)
    QTimer::singleShot(0, this, &BackgroundProcessor::initialize);
}

// Destructor
BackgroundProcessor::~BackgroundProcessor()
{
    delete m_actionMenu;
    // m_apiClient is cleaned up by QObject parenting if set
    // m_accessibilityHelper is cleaned up by QObject parenting
}


// Initialization function called by timer
void BackgroundProcessor::initialize() {
    bool atsip_ok = false;
#ifdef HAVE_ATSPI
    atsip_ok = m_accessibilityHelper.initialize();
    if (!atsip_ok) {
        showNotification(QStringLiteral("Initialization Warning"), QStringLiteral("Could not initialize Accessibility services (AT-SPI). Direct text modification disabled."), true);
        // Don't quit, clipboard fallback is still possible
    } else {
        qInfo() << "Accessibility Helper initialized.";
    }
#else
    showNotification(QStringLiteral("Initialization Warning"), QStringLiteral("Compiled without Accessibility support (AT-SPI). Direct text modification disabled."), true);
#endif


    // Try setting up the API client (reads environment variable)
    setupApiClient();
    if (!m_apiClient) {
        qCritical() << "API Client could not be initialized (OPENAI_API_KEY missing?).";
        showNotification(QStringLiteral("Configuration Error"), QStringLiteral("OpenAI API Key not found. Set OPENAI_API_KEY environment variable. Application cannot function."), true);
        // Quit if API key is essential and missing
        QTimer::singleShot(100, qApp, &QCoreApplication::quit);
        return;
    }

    // Connect signals from ApiClient
    connect(m_apiClient, &ApiClient::processingFinished, this, &BackgroundProcessor::handleApiResult);
    connect(m_apiClient, &ApiClient::processingError, this, &BackgroundProcessor::handleApiError);

    qInfo() << "BackgroundProcessor initialized successfully.";
    if (!atsip_ok) {
        qWarning() << "Running in clipboard-only fallback mode due to AT-SPI issues.";
    }
}


void BackgroundProcessor::setupApiClient() {
    // Check if apiClient already exists to prevent multiple instances
    if (m_apiClient) {
        return;
    }
    m_apiKey = qEnvironmentVariable("OPENAI_API_KEY");
    if (m_apiKey.isEmpty()) {
        qWarning() << "Environment variable OPENAI_API_KEY is not set.";
        return; // Do not create the client if key is missing
    }
    m_apiClient = new ApiClient(m_apiKey, this); // Parent to BackgroundProcessor
}

void BackgroundProcessor::createActionMenu() {
    QMetaEnum metaEnum = QMetaEnum::fromType<TextAction>();
    for (int i = 0; i < metaEnum.keyCount(); ++i) {
        QString key = QString::fromLatin1(metaEnum.key(i));
        QString displayText = key;
        // Improve display text: Add spaces before capital letters (camelCase to Title Case)
        displayText.replace(QRegularExpression(QStringLiteral("(?<=[a-z])([A-Z])")), QStringLiteral(" \\1"));
        QAction *action = m_actionMenu->addAction(displayText);
        action->setData(QVariant::fromValue(metaEnum.value(i))); // Store enum value as int
    }
    connect(m_actionMenu, &QMenu::triggered, this, &BackgroundProcessor::onActionSelected);
}

// --- Workflow ---

void BackgroundProcessor::onShortcutActivated() {
    qDebug() << "Shortcut activated.";

    // Clear previous state
    m_currentTargetInfo = ElementInfo(); // Reset element info

    if (!m_apiClient) {
        qWarning() << "Shortcut ignored: API Client not initialized.";
        showNotification(QStringLiteral("Error"), QStringLiteral("API Client not ready (check API key)."), true);
        return;
    }

#ifdef HAVE_ATSPI
    if (!m_accessibilityHelper.isInitialized()) {
        qWarning() << "AT-SPI not available. Attempting clipboard fallback ONLY.";
        QString clipboardText = m_clipboard->text(QClipboard::Clipboard);
        if (clipboardText.trimmed().isEmpty()) {
            clipboardText = m_clipboard->text(QClipboard::Selection); // Try selection clipboard too
        }

        if (clipboardText.trimmed().isEmpty()) {
            showNotification(QStringLiteral("Warning"), QStringLiteral("No text found in clipboard."), false);
            return;
        }
        qDebug() << "AT-SPI unavailable, using clipboard text.";
        m_currentTargetInfo.isValid = true;
        m_currentTargetInfo.isEditable = false;
        m_currentTargetInfo.text = clipboardText;
        m_currentTargetInfo.wasSelection = false;
        m_currentTargetInfo.selectionStart = 0;
        m_currentTargetInfo.selectionEnd = clipboardText.length();
        m_currentTargetInfo.textLength = clipboardText.length();
        showMenuAndPrepare();

    } else {
        // --- Step 1: Get Focused Element Info (AT-SPI) ---
        m_currentTargetInfo = m_accessibilityHelper.getFocusedElementInfo();

        if (!m_currentTargetInfo.isValid) {
            qWarning() << "Could not get valid focused text element via AT-SPI. Trying active selection.";
            QString clipboardText = m_clipboard->text(QClipboard::Selection);
            if (clipboardText.trimmed().isEmpty()) {
                qWarning() << "Could not get valid focused text element via active selection. Trying clipboard.";
                 clipboardText = m_clipboard->text(QClipboard::Clipboard);
            }

            if (clipboardText.trimmed().isEmpty()) {
                showNotification(QStringLiteral("Warning"), QStringLiteral("No focused text element or clipboard text found."), false);
                return; // Give up
            }

            m_currentTargetInfo.isValid = true;
            m_currentTargetInfo.isEditable = false; // Assume not editable if AT-SPI failed
            m_currentTargetInfo.text = clipboardText;
            m_currentTargetInfo.wasSelection = false;
            m_currentTargetInfo.selectionStart = 0;
            m_currentTargetInfo.selectionEnd = clipboardText.length();
            m_currentTargetInfo.textLength = clipboardText.length();

        } else if (m_currentTargetInfo.text.trimmed().isEmpty() && !m_currentTargetInfo.isEditable) {
            qDebug() << "Focused element via AT-SPI is empty and not editable.";
            showNotification(QStringLiteral("Info"), QStringLiteral("The focused non-editable element is empty."), false);
            return; // Nothing to process
        } else if (m_currentTargetInfo.text.trimmed().isEmpty() && m_currentTargetInfo.isEditable) {
            qDebug() << "Focused element via AT-SPI is editable but empty.";
            // Proceed to show menu - user might want to insert generated text
        }
        showMenuAndPrepare();
    }
#else // No AT-SPI compiled in
    qWarning() << "AT-SPI support not compiled in. Attempting clipboard fallback ONLY.";
    QString clipboardText = m_clipboard->text(QClipboard::Clipboard);
    if (clipboardText.trimmed().isEmpty()) {
        clipboardText = m_clipboard->text(QClipboard::Selection);
    }
    if (clipboardText.trimmed().isEmpty()) {
         showNotification(QStringLiteral("Warning"), QStringLiteral("No text found in clipboard."), false);
         return;
    }
     qDebug() << "Using clipboard text (no AT-SPI).";
    m_currentTargetInfo.isValid = true;
    m_currentTargetInfo.isEditable = false; // Cannot edit directly
    m_currentTargetInfo.text = clipboardText;
    m_currentTargetInfo.wasSelection = false;
    m_currentTargetInfo.selectionStart = 0;
    m_currentTargetInfo.selectionEnd = clipboardText.length();
    m_currentTargetInfo.textLength = clipboardText.length();
    showMenuAndPrepare();
#endif
}

void BackgroundProcessor::showMenuAndPrepare() {
    if (!m_currentTargetInfo.isValid) {
        qWarning() << "showMenuAndPrepare called with invalid state.";
        m_currentTargetInfo = ElementInfo(); // Clear state
        return;
    }
    if (m_currentTargetInfo.text.trimmed().isEmpty()) {
        qDebug() << "Target text is empty (editable: " << m_currentTargetInfo.isEditable << "). Showing menu anyway.";
    } else {
        qDebug() << "Got text (length " << m_currentTargetInfo.text.length() << ", selected: " << m_currentTargetInfo.wasSelection << ", editable: " << m_currentTargetInfo.isEditable << "). Showing menu.";
        qDebug() << "Text preview (first 50 chars):" << m_currentTargetInfo.text.left(50) << "...";
    }

    // --- Step 2: Show Action Menu ---
    m_actionMenu->setWindowFlags(m_actionMenu->windowFlags() | Qt::WindowStaysOnTopHint);
    m_actionMenu->popup(QCursor::pos());
}


void BackgroundProcessor::onActionSelected(QAction *action) {
    if (!action || !m_apiClient || !m_currentTargetInfo.isValid) {
        qWarning() << "onActionSelected called with invalid action, API client, or target info.";
        m_currentTargetInfo = ElementInfo(); // Clear state
        return;
    }

    bool ok;
    int enumValue = action->data().toInt(&ok);
    if (!ok) {
        qWarning() << "Invalid data in selected action.";
        m_currentTargetInfo = ElementInfo(); // Clear state
        return;
    }
    m_currentAction = static_cast<TextAction>(enumValue); // Store action

    qDebug() << "Action selected:" << action->text() << "(Enum value:" << enumValue << ")";

    // Treat empty editable text as a valid case for insertion
    if (m_currentTargetInfo.text.isEmpty() && !m_currentTargetInfo.isEditable) {
        qDebug() << "Target text is empty and not editable, nothing to send to API.";
        showNotification(QStringLiteral("Warning"), QStringLiteral("Cannot process empty non-editable text."), false);
        m_currentTargetInfo = ElementInfo(); // Clear state
        return;
    }

    qDebug() << "Sending text to API for processing...";
    showNotification(QStringLiteral("Processing"), QStringLiteral("Contacting AI assistant..."), false);

    // --- Step 3: Process Text via API ---
    m_apiClient->processText(m_currentTargetInfo.text, m_currentAction);
}


void BackgroundProcessor::handleApiResult(const QString &resultText) {
    qDebug() << "API result received (length " << resultText.length() << ")";
    qDebug() << "Result preview:" << resultText.left(50) << "...";

    if (!m_currentTargetInfo.isValid) {
        qWarning() << "Target element info lost before API result handling. Falling back to clipboard.";
        fallbackToClipboard(resultText, QStringLiteral("Target application context lost."));
        m_currentTargetInfo = ElementInfo(); // Clear state
        return;
    }

    // --- Step 4: Attempt to Replace Text via AT-SPI ---
    bool replacedViaAtspi = false;
#ifdef HAVE_ATSPI
    // Check accessible pointer is still valid conceptually and element is editable
    if (m_accessibilityHelper.isInitialized() && m_currentTargetInfo.isEditable && m_currentTargetInfo.accessible) {
        qDebug() << "Attempting direct text replacement via AT-SPI...";
        replacedViaAtspi = m_accessibilityHelper.replaceTextInElement(m_currentTargetInfo, resultText);
    } else {
        qDebug() << "Direct replacement conditions not met (AT-SPI off:" << !m_accessibilityHelper.isInitialized()
                 << ", not editable:" << !m_currentTargetInfo.isEditable
                 << ", no accessible object:" << !m_currentTargetInfo.accessible << ").";
    }
#endif // HAVE_ATSPI

    // --- Step 5: Fallback or Standard Clipboard Update ---
    if (replacedViaAtspi) {
        qInfo() << "Text modified and replaced directly.";
        showNotification(QStringLiteral("Success"), QStringLiteral("Text modified and replaced."), false);
        // Optionally update clipboard as well
        // m_clipboard->setText(resultText, QClipboard::Clipboard);
    } else {
        // AT-SPI replacement failed or wasn't possible/attempted
        QString reason;
        if (m_currentTargetInfo.isEditable
            #ifdef HAVE_ATSPI
            && m_accessibilityHelper.isInitialized() && m_currentTargetInfo.accessible // Only add specific warning if it *should* have worked
#endif
                )
        {
            qWarning() << "Direct text replacement failed. Falling back to clipboard.";
            reason = QStringLiteral("Could not directly modify text.");
        } else if (!m_currentTargetInfo.isEditable) {
            qInfo() << "Target was not editable. Result copied to clipboard.";
            reason = QStringLiteral("Target not editable.");
        } else {
            qInfo() << "AT-SPI unavailable/disabled or object lost. Result copied to clipboard.";
            reason = QStringLiteral("Direct modification unavailable.");
        }
        fallbackToClipboard(resultText, reason);
    }

    // --- Step 6: Cleanup ---
    m_currentTargetInfo = ElementInfo(); // Clear state for next operation
}

void BackgroundProcessor::handleApiError(const QString &errorMsg) {
    qWarning() << "API Error reported:" << errorMsg;
    showNotification(QStringLiteral("API Error"), errorMsg, true);
    // Clear state on error
    m_currentTargetInfo = ElementInfo();
}

// Helper for clipboard fallback
void BackgroundProcessor::fallbackToClipboard(const QString& text, const QString& reason) {
    m_clipboard->setText(text, QClipboard::Clipboard);
    qInfo() << reason << "Result placed on main clipboard.";
    // Combine reason and standard message for notification
    QString notificationText = reason + QStringLiteral(" Result copied to clipboard.");
    showNotification(QStringLiteral("Result Copied"), notificationText, true); // Show as warning state
}
void BackgroundProcessor::showNotification(const QString &title, const QString &text, bool isError) {
    qDebug() << "Notification Request:" << title << "-" << text << "(Error:" << isError << ")";

#ifdef HAVE_KNOTIFICATIONS // Check if KNotifications support is compiled in

    // --- KNotifications Implementation (KF6 Object-Oriented Approach) ---

    // 1. Define a unique Event ID for this *type* of notification.
    //    This allows users to configure behavior (sound, popup) for all
    //    notifications of this type in System Settings -> Notifications -> Applications.
    //    Use a reverse-domain style name specific to your application/event.
    //    Example: "yourapp-process-finished", "yourapp-network-error", etc.
    //    Let's use a generic one for this example, but replace it with something meaningful.
    const QString eventId = QStringLiteral("knowbridge-event"); // REPLACE with a proper ID

    // 2. Create a KNotification object, associating it with the event ID.
    //    We create it on the heap because we'll send it asynchronously.
    KNotification *notification = new KNotification(eventId); // Use 'new'

    notification->setComponentName(qApp->applicationDisplayName());

    // 4. Determine Urgency based on whether it's an error or other criteria.
    KNotification::Urgency urgency = KNotification::NormalUrgency; // Default
    if (isError) {
        urgency = KNotification::CriticalUrgency; // Use Critical for errors
    }
    // Example: You could use LowUrgency for less important background task updates
    // else if (title.contains(QStringLiteral("Progress"), Qt::CaseInsensitive)) {
    //     urgency = KNotification::LowUrgency;
    // }
    notification->setUrgency(urgency);

    // 5. Choose an appropriate Icon Name from the theme.
    QString iconName;
    if (isError) {
        iconName = QStringLiteral("dialog-error");          // Standard error icon
    } else if (title.contains(QStringLiteral("Success"), Qt::CaseInsensitive)) {
        iconName = QStringLiteral("dialog-ok-apply");       // Standard success icon
    } else {
        iconName = QStringLiteral("dialog-information");    // Default info icon
    }
    // Use setIconName to leverage the system icon theme
    notification->setIconName(iconName);
    // Alternatively, for a custom icon not in the theme, use setPixmap:
    // notification->setPixmap(QPixmap(":/path/to/your/custom_icon.png"));

    // 6. Set the main Title and Text content.
    notification->setTitle(title);
    notification->setText(text);

    // 7. Set other optional properties as needed:
    // - Actions: Add buttons the user can click.
    //   notification->setActions({QStringLiteral("Action 1"), QStringLiteral("Action 2")});
    //   // Connect to the actionActivated(QString) signal to handle clicks.
    //   connect(notification, &KNotification::actionActivated, this, &MyClass::handleNotificationAction);
    // - Sound: Play a specific sound file.
    //   notification->setSoundFile(QUrl::fromLocalFile("/path/to/sound.wav"));
    // - Loop Sound (usually for critical urgency):
    //   if (isError) notification->setLoopSound(true);
    // - Persistence (How long it stays on screen, system may override):
    //   notification->setPersistent(false); // Default is usually non-persistent
    // - Close on Timeout: Make it disappear automatically.
    //   notification->setCloseOnTimeout(true); // Often the default behavior

    // 8. Send the notification event asynchronously.
    //    The return value is the notification's runtime ID, useful if you
    //    need to update or close this specific notification later.
    notification->sendEvent();


    // 9. IMPORTANT: Manage the lifetime of the KNotification object.
    //    Since we created it with 'new', we must delete it to avoid memory leaks.
    //    The standard way for fire-and-forget notifications is to connect the
    //    `closed()` signal (emitted when the notification is dismissed by the user
    //    or times out) to the object's `deleteLater()` slot.
    QObject::connect(notification, &KNotification::closed, notification, &QObject::deleteLater);

    // If you *might* need to interact with the notification later (e.g., using
    // KNotification::closeNotification(notificationId) or updating it), you would store
    // the 'notification' pointer (or just the 'notificationId') in a member variable
    // or map and manage its deletion when it's no longer needed (e.g., when closed
    // or when the associated task completes).

#else // Fallback if HAVE_KNOTIFICATIONS is not defined (e.g., non-Linux platforms or minimal builds)

    // --- Fallback Logging ---
    // Use Qt's logging categories for better output control.
    if (isError) {
        qCritical().nospace() << "NOTIFICATION [Error]: " << title << " - " << text;
    } else {
        qInfo().nospace() << "NOTIFICATION [Info]: " << title << " - " << text;
    }

#endif // HAVE_KNOTIFICATIONS
}