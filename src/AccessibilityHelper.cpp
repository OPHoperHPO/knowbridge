// --- File: src/AccessibilityHelper.cpp ---
#include "AccessibilityHelper.h"
#include <QDebug>
#include <QTimer> // For GLib event processing
#include <QCoreApplication> // For thread check
#include <QThread> // <<< FIX 1: Include QThread

#ifdef HAVE_ATSPI
#include <atspi/atspi.h>
#include <glib.h> // For g_free, g_error_free etc.
#include <glib-object.h> // For g_object_unref
#include <atspi/atspi-text.h> // Include necessary for AtspiTextRange, atspi_text_get_selection
#include <atspi/atspi-registry.h> // Include necessary for AT-SPI registry access (if used)
#include <atspi/atspi-accessible.h> // Include necessary for interface functions
#include <atspi/atspi-editabletext.h> // Include necessary for AtspiEditableText and related functions
#include <atspi/atspi-event-listener.h> // For listening to focus events, AtspiEventListenerMode
#include <atspi/atspi-constants.h> // For ATSPI_STATE_FOCUSED etc.

#ifdef HAVE_ATK
#include <atk/atk.h> // For initial focus fallback
#endif // HAVE_ATK

// Custom Deleter implementation needed outside the header for QSharedPointer
// (Handled fine by lambda in ElementInfo constructor in header)
#endif // HAVE_ATSPI

namespace { // Anonymous namespace for static callback

#ifdef HAVE_ATSPI
// Static callback function for focus events
// Needs access to the AccessibilityHelper instance to update m_currentFocus
// We pass 'this' as user_data when registering the listener.
    void focus_event_callback(AtspiEvent *event, gpointer user_data) {
        // <<< FIX 1 applied here (QThread include)
        if (!QCoreApplication::instance() || QCoreApplication::instance()->thread() != QThread::currentThread()) {
            // Should not happen if GLib events are processed in Qt main thread, but good sanity check
            qWarning() << "AT-SPI focus callback received in wrong thread!";
            return;
        }

        AccessibilityHelper *helper = static_cast<AccessibilityHelper*>(user_data);
        if (!helper)            return;

        if (!event || !event->source) {
            qWarning() << "AT-SPI focus event received with null event or source.";
            return;
        }

        // --- START: Check if the focus event is from our own application ---
        GError* pid_error = nullptr;
        // Note: atspi_accessible_get_process_id does not take a GError** argument
        // It returns -1 on error.
        int focused_app_pid = atspi_accessible_get_process_id(event->source, nullptr); // Pass nullptr for error

        if (focused_app_pid == -1) {
            // Could not get PID, proceed cautiously (maybe log a warning)
            qWarning() << "AT-SPI: Failed to get process ID for focused object. Cannot ignore self.";
            // Don't compare if we couldn't get the PID
        } else {
            qint64 self_pid = QCoreApplication::applicationPid();
            if (self_pid > 0 && focused_app_pid == self_pid) {
                // It's our own application window gaining focus (e.g., menu popup).
                // Ignore this event to keep focus on the original target application.
                // qDebug() << "AT-SPI: Ignoring focus event from self (PID:" << self_pid << ")";
                return; // <-- EXIT EARLY, DO NOT UPDATE m_currentFocus
            }
        }
        // --- END: Check if the focus event is from our own application ---

        // Check if the event indicates focus gain
        // The event string "object:state-changed:focused" implies state change.
        // We need to check if the FOCUSED state is *now* set on the source object.
        // We can rely on the event source directly being the newly focused object for
        // standard focus events like `Focus:Object` or `Focus:Window`.
        // For `object:state-changed:focused`, let's verify the state.
        AtspiStateSet* stateSet = atspi_accessible_get_state_set(event->source);
        // <<< FIX 2: Use g_object_unref for AtspiStateSet
        if (stateSet) {
            g_object_unref(stateSet); // Free the state set reference
        }

        if (atspi_state_set_contains(stateSet, ATSPI_STATE_FOCUSED)
            && atspi_state_set_contains(stateSet, ATSPI_STATE_EDITABLE)) {
            // The source object is now focused.
            helper->updateCurrentFocus(event->source);
        } else if (atspi_state_set_contains(stateSet, ATSPI_STATE_FOCUSED)) {
            helper->updateCurrentFocus(nullptr); // we don't need other components
        }

        // This could be a focus-lost event for this object, ignore for our purpose
        // unless we also want to track focus loss explicitly.
        // NOTE: DO NOT free the event here, the listener machinery does it.
        //       atspi_event_free(event); // <-- WRONG

        return;
    }

    void   destroy_callback     (gpointer){};
#endif // HAVE_ATSPI

} // end anonymous namespace


AccessibilityHelper::AccessibilityHelper(QObject *parent)
        : QObject(parent)
#ifdef HAVE_ATSPI
        , m_glibEventTimer(new QTimer(this)), // Initialize timer here
          m_focusListener(nullptr),
          m_currentFocus(nullptr)
#endif // HAVE_ATSPI
{
    // Initialization moved to explicit initialize() method
    // Connect the timer here
#ifdef HAVE_ATSPI
    connect(m_glibEventTimer, &QTimer::timeout, this, &AccessibilityHelper::processGlibEvents);
#endif
}

AccessibilityHelper::~AccessibilityHelper()
{
#ifdef HAVE_ATSPI
    if (m_glibEventTimer) {
        m_glibEventTimer->stop();
        // Timer is child object, will be deleted by QObject parent destructor
    }
    if (m_focusListener) {
        GError* error = nullptr;
        // Use the listener handle to deregister
        // <<< FIX 3: Add the event type string argument
        atspi_event_listener_deregister(m_focusListener, "object:state-changed:focused", &error);
        if (error) {
            qWarning() << "Error deregistering AT-SPI focus listener:" << error->message;
            g_error_free(error);
        } else {
            qInfo() << "AT-SPI focus listener deregistered.";
        }
        m_focusListener = nullptr; // Mark as deregistered
    }
    if (m_currentFocus) {
        g_object_unref(m_currentFocus);
        m_currentFocus = nullptr;
    }
    if (m_initialized) {
        qInfo() << "AT-SPI potentially shutting down (if managed by this helper).";
        // Commented out as it might interfere with Desktop Environment AT-SPI management
        // if (atspi_is_initialized()) {
        //     atspi_exit();
        // }
    }
#endif // HAVE_ATSPI
}

bool AccessibilityHelper::initialize()
{
    if (m_initialized) return true;

#ifdef HAVE_ATSPI
    // g_type_init(); // Deprecated

    // Initialize AT-SPI. Consider potential conflicts if Qt/DE already does.
    if (!atspi_is_initialized()) { // Check if already initialized
        // <<< FIX 4: Remove unused init_error variable
        // GError* init_error = nullptr;
        // atspi_init returns 0 on success, non-zero on failure
        if (atspi_init() != 0) {
            // Note: atspi_init doesn't use GError currently AFAIK
            qWarning() << "Failed to initialize AT-SPI library (atspi_init failed).";
            m_initialized = false;
            return false;
        }
        qInfo() << "AT-SPI library initialized by this helper.";
    } else {
        qInfo() << "AT-SPI library was already initialized.";
    }

    m_initialized = true;

    // Register focus listener
    GError* error = nullptr;
    // Listen for the state change event indicating an object gained focus.
    // Passing 'this' as user_data allows the static callback to call our member function.
    // We store the returned listener handle to deregister later.
    // Using "object:state-changed:focused" is specific. "Focus:" might be broader.
    // <<< FIX 5 & 6: Use atspi_event_listener_register_full and correct mode enum
    m_focusListener = atspi_event_listener_new(focus_event_callback, this, destroy_callback);
    atspi_event_listener_register(m_focusListener,
                                                         "object:state-changed:focused",
                                                         &error);

    if (!m_focusListener || error) {
        qWarning() << "Failed to register AT-SPI focus listener:" << (error ? error->message : "Unknown error");
        g_clear_error(&error);
        // Don't completely fail initialization, maybe initial focus query works
    } else {
        qInfo() << "AT-SPI focus listener registered successfully."; // FIX 7: Warning gone now
    }

    // Start GLib event processing timer (e.g., every 50ms)
    m_glibEventTimer->start(50);
    qInfo() << "GLib event processing timer started.";
    qInfo() << "Initial focus will be set by the first focus event received.";

    return true;
#else
    qWarning() << "AT-SPI support is disabled (not found at compile time).";
    m_initialized = false;
    return false;
#endif // HAVE_ATSPI
}

bool AccessibilityHelper::isInitialized() const
{
    return m_initialized;
}


#ifdef HAVE_ATSPI
// Slot to process pending GLib events
void AccessibilityHelper::processGlibEvents() {
    // Process all pending events in the default GLib context without blocking
    while (g_main_context_pending(nullptr)) {
        g_main_context_iteration(nullptr, FALSE);
    }
}

// Member function to update the currently tracked focused object
// MUST be called from the main Qt thread (where GLib events are processed)
void AccessibilityHelper::updateCurrentFocus(AtspiAccessible* newFocus) {
    if (!newFocus) {
        // Release the old reference if it exists
        if (m_currentFocus) {
            g_object_unref(m_currentFocus);
        }
        m_currentFocus = nullptr;
    }



    // Avoid self-assignment и unnecessary ref/unref.
    if (newFocus == m_currentFocus) {
        return;
    }

    // Get info before updating pointer
    char *name = atspi_accessible_get_name(newFocus, nullptr);
    AtspiRole role = atspi_accessible_get_role(newFocus, nullptr);
    char *role_name = atspi_role_get_name(role);
    qDebug() << "AT-SPI Focus changed to:" << getAccessibleDebugString(newFocus);
    g_free(name);
    g_free(role_name);


    // Release the old reference if it exists
    if (m_currentFocus) {
        g_object_unref(m_currentFocus);
    }

    // Take a new reference to the new object and store it
    m_currentFocus = static_cast<AtspiAccessible*>(g_object_ref(newFocus));
}


// Helper to safely get text from AtspiText*, returns empty QString on error or if no text
// This isolates the direct text fetching logic.
QString AccessibilityHelper::getTextFromAtspiText(AtspiText* textInterface, int startOffset, int endOffset) {
    if (!textInterface) return QString();

    GError *error = nullptr;
    // Note: atspi_text_get_text returns a NEW string that must be g_free'd
    char *c_text = atspi_text_get_text(textInterface, startOffset, endOffset, &error);

    if (error) {
        qWarning() << "AT-SPI Error getting text:" << error->message;
        g_error_free(error);
        return QString();
    }

    if (!c_text) {
        // Not necessarily an error, could just be empty range or content
        return QString();
    }

    QString result = QString::fromUtf8(c_text);
    g_free(c_text); // Free the C string
    return result;
}

// Helper to get text from AtspiAccessible*. Matches header declaration.
// Gets the Text interface and then calls getTextFromAtspiText.
QString AccessibilityHelper::getTextFromAccessible(AtspiAccessible* acc, int startOffset, int endOffset) {
    if (!acc) return QString();

    AtspiText *text_iface = atspi_accessible_get_text_iface(acc); // No GError** argument
    if (!text_iface) {
        // Not an error, just doesn't support the interface
        // qDebug() << "Object does not support AtspiText interface.";
        return QString();
    }

    QString result = getTextFromAtspiText(text_iface, startOffset, endOffset);

    // Unref the interface pointer obtained from get_text_iface
    // Note: Interfaces obtained via atspi_accessible_get_*_iface don't need separate unref typically
    // They are owned by the AtspiAccessible object itself. Let's remove this unref.
    // g_object_unref(text_iface);

    return result;
}


#endif // HAVE_ATSPI

QString getAccessibleDebugString(AtspiAccessible *obj) {
#ifdef HAVE_ATSPI
    if (!obj) return QStringLiteral("<null>");
    gchar *name = atspi_accessible_get_name(obj, nullptr);
    gchar *role_name = atspi_accessible_get_role_name(obj, nullptr);
    // Use accessible pointer directly for identity
    QString debugStr = QStringLiteral("Obj: %1 (Name: '%2', Role: '%3')")
            .arg(QString::number(reinterpret_cast<quintptr>(obj), 16)) // Pointer address
            .arg(name ? QString::fromUtf8(name) : QStringLiteral("<no name>"))
            .arg(role_name ? QString::fromUtf8(role_name) : QStringLiteral("<no role>"));
    g_free(name);
    g_free(role_name);
    return debugStr;
#else
    Q_UNUSED(obj);
    return QStringLiteral("<AT-SPI disabled>");
#endif
}



ElementInfo AccessibilityHelper::getFocusedElementInfo()
{
#ifdef HAVE_ATSPI
    if (!m_initialized) {
        qWarning() << "AT-SPI not initialized, cannot get focused element.";
        return ElementInfo(); // Return invalid struct
    }

    // Use the currently tracked focus object from the event listener
    AtspiAccessible *focused_acc_tracked = m_currentFocus;

    if (!focused_acc_tracked) {
        qWarning() << "AT-SPI: No object currently tracked as focused (no focus event received yet?).";
        return ElementInfo();
    }

    // We need to pass a new reference to ElementInfo, as it will unref it later.
    // The m_currentFocus reference belongs to the helper class instance.
    AtspiAccessible *focused_acc = static_cast<AtspiAccessible*>(g_object_ref(focused_acc_tracked));

    qDebug() << "AT-SPI: Processing tracked focused object:" << getAccessibleDebugString(focused_acc);

    if (!focused_acc) {
        qWarning() << "AT-SPI: No object currently focused.";
        // No need to unref desktop as we didn't get it
        return ElementInfo();
    }

    // --- Get Text interface ---
    // No GError** argument here
    AtspiText *text_iface = atspi_accessible_get_text_iface(focused_acc);

    if (!text_iface) {
        qWarning() << "Focused object does not support AT-SPI Text interface.";
        g_object_unref(focused_acc); // Unref the focused object before returning
        return ElementInfo();
    }

    // --- Get Text Length ---
    GError* error = nullptr;
    gint text_length = atspi_text_get_character_count(text_iface, &error);
    if (error) {
        qWarning() << "AT-SPI Error getting character count:" << error->message;
        g_error_free(error);
        error = nullptr;
        text_length = 0; // Assume zero length on error
    } else {
        qDebug() << "AT-SPI: Text length:" << text_length;
    }

    // --- Get Selection ---
    int start_offset = -1;
    int end_offset = -1;
    error = nullptr; // Reset error pointer
    // Use AtspiTextRange, not AtspiRange. Function takes AtspiText*
    AtspiRange* selection_range = atspi_text_get_selection(text_iface, 0, &error); // Selection index 0

    if (error) {
        qWarning() << "AT-SPI Error getting selection:" << error->message;
        g_error_free(error);
        error = nullptr; // Reset error
        // Continue, maybe we can still get all text
    }

    bool has_selection = (selection_range != nullptr && selection_range->start_offset >= 0 && selection_range->end_offset >= selection_range->start_offset);
    if (has_selection) {
        start_offset = selection_range->start_offset;
        end_offset = selection_range->end_offset;
    }
    if (selection_range) {
        // atspi_text_range_free(selection_range); // Use correct free function <<-- REMOVED
        g_free(selection_range); // <<-- CORRECTED: Use g_free for memory returned by AT-SPI functions like this
    }

    // --- Get Text Content ---
    QString element_text;
    bool was_selection = false;

    if (has_selection && start_offset >= 0 && end_offset >= start_offset && end_offset <= text_length && end_offset != 0) {
        // Valid selection found, get selected text using the dedicated helper
        element_text = getTextFromAtspiText(text_iface, start_offset, end_offset); // Pass text_iface
        was_selection = true;
    } else {
        // No valid selection or error getting selection, get all text
        start_offset = 0; // Reset offsets for "all text" case
        end_offset = text_length;
        // Only get text if length is positive to avoid unnecessary calls/errors
        if (text_length > 0) {
            element_text = getTextFromAtspiText(text_iface, start_offset, end_offset); // Pass text_iface
        } else {
            element_text = QString(); // Ensure empty string if length is 0
        }
        was_selection = false;
        if (text_length > 0) {
            qDebug() << "AT-SPI: No selection found, got all text (Length:" << text_length << ")";
        } else {
            qDebug() << "AT-SPI: No selection found, and no text content.";
        }
    }
    qDebug() << "AT-SPI: Text retrieved (WasSelection:" << was_selection << "Range:" << start_offset << "-" << end_offset << "):" << element_text.left(50) << "...";

    // --- Check Editability ---
    // No GError** argument here
    AtspiEditableText* editable_iface_check = atspi_accessible_get_editable_text_iface(focused_acc);
    bool is_editable = (editable_iface_check != nullptr);

    // Don't unref the interface pointer obtained this way
    // if (editable_iface_check) {
    //     g_object_unref(editable_iface_check); // Unref immediately, only needed for the check
    // }

    if (!is_editable) {
        qDebug() << "Focused element is not editable via AT-SPI.";
        // No error message here, as null return is the expected way to indicate non-support
    }

    // Don't unref the text interface pointer obtained this way
    // g_object_unref(text_iface);

    // The AtspiAccessible* (focused_acc) is passed to the ElementInfo constructor,
    // which takes ownership via QSharedPointer and ensures it's unref'd later.
    // We don't unref focused_acc here anymore.
    return ElementInfo(focused_acc, is_editable, element_text,
                       was_selection ? start_offset : 0,
                       was_selection ? end_offset : text_length,
                       text_length, was_selection);

#else
    qWarning() << "AT-SPI support is disabled.";
    return ElementInfo(); // Return invalid default struct
#endif // HAVE_ATSPI
}


bool AccessibilityHelper::replaceTextInElement(const ElementInfo& elementInfo, const QString& newText)
{
#ifdef HAVE_ATSPI
    if (!m_initialized || !elementInfo.isValid || !elementInfo.isEditable || !elementInfo.accessible) {
        qWarning() << "Cannot replace text: AT-SPI not init, element invalid/uneditable, or accessible ptr missing.";
        return false;
    }

    AtspiAccessible* acc = elementInfo.accessible.data(); // Get raw pointer from QSharedPointer
    if (!acc) {
        qWarning() << "Accessible pointer is null in ElementInfo.";
        return false;
    }

    // Get the EditableText interface pointer
    // No GError** argument here
    AtspiEditableText* editable_iface = atspi_accessible_get_editable_text_iface(acc);

    if (!editable_iface) {
        qWarning() << "Could not get EditableText interface for replacement (already checked isEditable, but verify again).";
        return false;
    }

    GError *error = nullptr;
    bool success = false;
    QByteArray newTextUtf8 = newText.toUtf8();

    qDebug() << "AT-SPI: Attempting to replace range" << elementInfo.selectionStart
             << "to" << elementInfo.selectionEnd << "with new text (length " << newTextUtf8.length() << ")";

    // Strategy: Delete original range, then insert new text at start position.

    // 1. Delete the original text range
    if (elementInfo.selectionEnd > elementInfo.selectionStart) { // Only delete if range has size > 0
        success = atspi_editable_text_delete_text(editable_iface,
                                                  elementInfo.selectionStart,
                                                  elementInfo.selectionEnd,
                                                  &error);
        if (error) {
            qWarning() << "AT-SPI Error deleting text:" << error->message;
            g_error_free(error);
            error = nullptr; // Reset error for next step
            success = false; // Explicitly mark failure on error
        } else if (!success) {
            qWarning() << "AT-SPI: Deleting text failed without specific GError.";
            // Success remains false
        } else {
            qDebug() << "AT-SPI: Successfully deleted range" << elementInfo.selectionStart << "-" << elementInfo.selectionEnd;
        }
    } else {
        qDebug() << "AT-SPI: Skipping delete step as range has zero or negative size:"
                 << elementInfo.selectionStart << "to" << elementInfo.selectionEnd;
        success = true; // Consider the (non-)delete step successful to proceed to insert
    }


    // Optional delay (use with caution)
    // QThread::msleep(20);

    // 2. Insert the new text at the original start position (only if delete step succeeded)
    if (success && !newTextUtf8.isEmpty()) { // Also check if there's actually text to insert
        // Note: The length parameter for insert_text is the number of *characters*, not bytes.
        // However, AT-SPI C API often expects byte length for char*. Let's stick with byte length.
        // CORRECTION: Documentation often specifies length is in characters for Glib/Gtk related C APIs dealing with UTF-8.
        // Let's use the character count from the QString.
        success = atspi_editable_text_insert_text(editable_iface,
                                                  elementInfo.selectionStart, // Insert at the beginning of the original range
                                                  newTextUtf8.constData(),
                                                  newText.length(), // Pass CHARACTER length
                                                  &error);
        if (error) {
            qWarning() << "AT-SPI Error inserting text:" << error->message;
            g_error_free(error);
            success = false; // Explicitly mark as failed
        } else if (!success) {
            qWarning() << "AT-SPI: Inserting text failed without specific GError.";
            // Success remains false
        }
    } else if (success && newTextUtf8.isEmpty()) {
        qDebug() << "AT-SPI: Skipping insert step as new text is empty.";
        // Success remains true (as delete might have been the only intended action)
    }


    // Cleanup the interface pointer - Not needed for interfaces obtained via atspi_accessible_get_*_iface
    // g_object_unref(editable_iface);

    if (success) {
        qInfo() << "AT-SPI: Text replacement/modification successful.";
    } else {
        qWarning() << "AT-SPI: Text replacement/modification failed.";
    }

    return success;

#else
    Q_UNUSED(elementInfo);
    Q_UNUSED(newText);
    qWarning() << "AT-SPI support is disabled, cannot replace text.";
    return false;
#endif // HAVE_ATSPI
}