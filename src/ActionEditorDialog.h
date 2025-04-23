// --------- src/ActionEditorDialog.h ----------
#pragma once
#include <QDialog>
#include <QDialogButtonBox>
#include "ConfigManager.h"

class QLineEdit;
class QTextEdit;

class ActionEditorDialog : public QDialog
{
Q_OBJECT
public:
    explicit ActionEditorDialog(QWidget *parent = nullptr);

    void setAction(const CustomAction &a);
    CustomAction action() const;

private Q_SLOTS:
    void validate();

private:
    QDialogButtonBox *m_buttons{nullptr};   // <- keep a pointer
    QLineEdit *m_name;
    QTextEdit *m_prompt;
};
