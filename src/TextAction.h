#ifndef TEXTACTION_H
#define TEXTACTION_H

#include <QObject> // Include QObject for Q_ENUM

// Define the enum within a QObject based class or namespace to use Q_ENUM
// This allows easy conversion to/from strings, especially for QComboBox
class TextActionHelper : public QObject {
Q_OBJECT // Required for Q_ENUM
public:
    // No constructor needed, just the enum definition and registration
    enum class Action {
        FixGrammar,
        FixStructure,
        ImproveStyle,
        MakeFormal,
        SimplifyText
    };
    Q_ENUM(Action) // Register the enum with Qt's meta-object system

private:
    // Private constructor to prevent instantiation if only used for enum
    TextActionHelper() = delete;
};

// Alias for easier use
using TextAction = TextActionHelper::Action;

#endif // TEXTACTION_H