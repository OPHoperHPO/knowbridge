// --------- src/ActionEditorDialog.cpp ----------
#include "ActionEditorDialog.h"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include "QAbstractButton"
#include <KPasswordLineEdit>
#include <QTextEdit>
#include <QPushButton>

#include <KLocalizedString>

ActionEditorDialog::ActionEditorDialog(QWidget *parent)
        : QDialog(parent)
{
    setWindowTitle(i18nc("@title:window","Custom Action"));
    resize(400, 200);

    auto *lay = new QFormLayout(this);
    lay->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_name   = new QLineEdit(this);
    m_prompt = new QTextEdit(this);
    m_prompt->setAcceptRichText(false);
    m_prompt->setMinimumHeight(80);

    lay->addRow(i18n("Name:"),   m_name);
    lay->addRow(i18n("Prompt:"),    m_prompt);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel, this);
    lay->addRow(m_buttons);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &ActionEditorDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &ActionEditorDialog::reject);

    connect(m_name,   &QLineEdit::textChanged, this, &ActionEditorDialog::validate);
    connect(m_prompt, &QTextEdit::textChanged, this, &ActionEditorDialog::validate);
    validate();
}

void ActionEditorDialog::setAction(const CustomAction &a)
{
    m_name->setText(a.name);
    m_prompt->setPlainText(a.prompt);
    validate();
}

CustomAction ActionEditorDialog::action() const
{
    return {m_name->text().trimmed(), m_prompt->toPlainText().trimmed()};
}

void ActionEditorDialog::validate()
{
    bool ok = !m_name->text().trimmed().isEmpty()
              && !m_prompt->toPlainText().trimmed().isEmpty();
     // Enable/disable only the *Ok* button
     if (auto *okBtn = m_buttons->button(QDialogButtonBox::Ok))
            okBtn->setEnabled(ok);


}
