// --- File: src/AccessibilityHelper.h ---
#ifndef ACCESSIBILITYHELPER_H
#define ACCESSIBILITYHELPER_H

#include <QObject>
#include <QString>
#include <QSharedPointer> // For managing AtspiAccessible lifecycle
#include <utility> // For std::move

// Forward declare Qt classes used in private members
class QTimer;

// Forward declare AT-SPI types if HAVE_ATSPI is defined
#ifdef HAVE_ATSPI
// Opaque struct forward declarations
struct _AtspiAccessible;
typedef struct _AtspiAccessible AtspiAccessible;
struct _AtspiText;
typedef struct _AtspiText AtspiText;
struct _AtspiEditableText;
typedef struct _AtspiEditableText AtspiEditableText;
struct _AtspiRange; // Used by AtspiText for selection
typedef struct _AtspiRange AtspiRange;
struct _AtspiEventListener; // For listener handle
typedef struct _AtspiEventListener AtspiEventListener;
struct _AtspiEvent; // For event callback parameter
typedef struct _AtspiEvent AtspiEvent;

#include <glib-object.h> // Needed for g_object_unref definition used in deleter

#else
// Define dummy types if AT-SPI is not available
typedef void AtspiAccessible;
typedef void AtspiText;
typedef void AtspiEditableText;
typedef void AtspiRange;
typedef void AtspiEventListener; // Dummy type
typedef void AtspiEvent; // Dummy type
#endif

// Helper function prototype (definition in .cpp)
QString getAccessibleDebugString(AtspiAccessible *obj);

// Structure to hold information about the focused text element
struct ElementInfo {
    bool isValid = false;       // Was a valid, focused, text element found?
    bool isEditable = false;    // Does the element support text editing?
    QString text;               // The retrieved text (selected or all)
    int selectionStart = -1;    // Start offset of selection (-1 if none/invalid)
    int selectionEnd = -1;      // End offset of selection (-1 if none/invalid)
    int textLength = 0;         // Total length of the text in the element
    bool wasSelection = false;  // True if specific text was selected, false if all text was retrieved

#ifdef HAVE_ATSPI
    // Use QSharedPointer with a custom deleter for automatic g_object_unref
    // Takes ownership of the passed AtspiAccessible* reference.
    QSharedPointer<AtspiAccessible> accessible;
#endif

    // Default constructor
    ElementInfo() = default;

#ifdef HAVE_ATSPI
    // Constructor for valid info
    // Takes ownership of the 'acc' pointer (expects a ref has been taken by the caller)
    ElementInfo(AtspiAccessible* acc, bool editable, QString t, int selStart, int selEnd, int len, bool wasSel)
            : isValid(true), isEditable(editable), text(std::move(t)), selectionStart(selStart), selectionEnd(selEnd), textLength(len), wasSelection(wasSel)
    {
        // Custom deleter for AtspiAccessible*
        auto deleter = [](AtspiAccessible* ptr) {
            if (ptr) {
                // qDebug() << "Unreffing AtspiAccessible via QSharedPointer:" << getAccessibleDebugString(ptr);
                g_object_unref(ptr); // Correct place to unref the object reference owned by the pointer
            }
        };
        // Reset takes ownership of the existing reference in 'acc'.
        accessible.reset(acc, deleter);
    }
#endif
};


class AccessibilityHelper : public QObject
{
Q_OBJECT
public:
    explicit AccessibilityHelper(QObject *parent = nullptr);
    ~AccessibilityHelper() override;

    bool initialize(); // Initialize AT-SPI connection, register listener, start timer
    bool isInitialized() const;

    // Get info about the currently focused text element using the tracked focus.
    ElementInfo getFocusedElementInfo();

    // Replace text in the given element.
    // Uses selectionStart/End from ElementInfo to determine the range.
    bool replaceTextInElement(const ElementInfo& elementInfo, const QString& newText);

#ifdef HAVE_ATSPI
    // Public method to update the internal focus pointer (called by static callback)
    // Must ensure this is called thread-safely if callbacks can happen off main thread
    // (but our timer approach keeps it on the main thread)
    void updateCurrentFocus(AtspiAccessible* newFocus);
#endif

private Q_SLOTS:
#ifdef HAVE_ATSPI
    // Slot connected to QTimer to process GLib events
    void processGlibEvents();
#endif

private:
    bool m_initialized = false;

#ifdef HAVE_ATSPI
    // Helper to get text safely from AtspiText interface
    QString getTextFromAtspiText(AtspiText* textInterface, int startOffset, int endOffset);

    // Helper to get text safely from AtspiAccessible object
    QString getTextFromAccessible(AtspiAccessible* acc, int startOffset, int endOffset);

    QTimer* m_glibEventTimer; // Timer to drive GLib event loop processing
    AtspiEventListener* m_focusListener; // Handle for the registered focus listener
    AtspiAccessible* m_currentFocus;     // Pointer to the currently focused accessible object (owned ref)

#endif

};

#endif // ACCESSIBILITYHELPER_H