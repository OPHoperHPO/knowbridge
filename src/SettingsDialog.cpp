// File: src/SettingsDialog.cpp
#include "SettingsDialog.h"
#include "ActionEditorDialog.h"
#include "ApiClient.h"
#include <QFormLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QTabWidget>
#include <QDialogButtonBox>
#include <QLabel>
#include <KPasswordLineEdit>
#include <KLocalizedString>
#include <QCheckBox>

static const QRegularExpression urlRx(QStringLiteral(R"(https?://.+)"));

SettingsDialog::SettingsDialog(QWidget *parent, ConfigManager *cfg)
        : QDialog(parent), m_cfg(cfg)
{
    setWindowTitle(i18nc("@title:window","Settings"));
    resize(520, 400);

    m_tabs = new QTabWidget(this);

    /* ---------------- General tab ---------------- */
    auto *gen = new QWidget(this);
    auto *gLay = new QFormLayout(gen);
    gLay->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_apiKey  = new KPasswordLineEdit(gen);
    m_apiKey->setClearButtonEnabled(true);

    m_endpoint = new QLineEdit(gen);
    m_endpoint->setClearButtonEnabled(true);
    m_endpoint->setPlaceholderText(QStringLiteral("https://api.example.com/v1/chat/completions"));
    m_endpoint->setValidator(new QRegularExpressionValidator(urlRx, this));
    connect(m_endpoint, &QLineEdit::textChanged, this, &SettingsDialog::validateEndpoint);

    m_endpointWarn = new QLabel(this);
    m_endpointWarn->setVisible(false);
    m_endpointWarn->setStyleSheet(QStringLiteral("color: red;"));

    m_model   = new QLineEdit(gen);
    m_model->setClearButtonEnabled(true);

    auto *apiBtn = new QPushButton(QIcon::fromTheme(QStringLiteral("system-run")),
                                   i18n("Test…"), gen);
    connect(apiBtn, &QPushButton::clicked, this, &SettingsDialog::testApiKey);

    auto *apiBox = new QHBoxLayout;
    apiBox->addWidget(m_apiKey);
    apiBox->addWidget(apiBtn);

    m_systemPrompt = new QTextEdit(gen);
    m_systemPrompt->setAcceptRichText(false);
    m_systemPrompt->setMinimumHeight(80);

    // New notifications checkbox
    m_notificationsCb = new QCheckBox(i18n("Enable notifications"), gen);

    gLay->addRow(i18n("API key:"),    apiBox);
    gLay->addRow(i18n("API endpoint:"), m_endpoint);
    gLay->addRow(QString(), m_endpointWarn);
    gLay->addRow(i18n("Model:"),      m_model);
    gLay->addRow(i18n("System prompt:"), m_systemPrompt);
    gLay->addRow(QString(), m_notificationsCb);

    m_tabs->addTab(gen, i18n("General"));

    /* ---------------- Actions tab ---------------- */
    auto *act = new QWidget(this);
    auto *v = new QVBoxLayout(act);
    m_list = new QListWidget(act);
    v->addWidget(m_list);

    m_addBtn    = new QPushButton(QIcon::fromTheme(QStringLiteral("list-add")),     i18n("Add…"),    act);
    m_editBtn   = new QPushButton(QIcon::fromTheme(QStringLiteral("document-edit")),i18n("Edit…"),   act);
    m_removeBtn = new QPushButton(QIcon::fromTheme(QStringLiteral("list-remove")),  i18n("Remove"),  act);
    m_upBtn     = new QPushButton(QIcon::fromTheme(QStringLiteral("go-up")),        i18n("Up"),      act);
    m_downBtn   = new QPushButton(QIcon::fromTheme(QStringLiteral("go-down")),      i18n("Down"),    act);

    connect(m_addBtn,    &QPushButton::clicked, this, &SettingsDialog::addAction);
    connect(m_editBtn,   &QPushButton::clicked, this, &SettingsDialog::editAction);
    connect(m_removeBtn, &QPushButton::clicked, this, &SettingsDialog::removeAction);
    connect(m_upBtn,     &QPushButton::clicked, this, &SettingsDialog::moveUp);
    connect(m_downBtn,   &QPushButton::clicked, this, &SettingsDialog::moveDown);
    connect(m_list,      &QListWidget::currentRowChanged, this, &SettingsDialog::updateButtons);

    auto *btnRow = new QHBoxLayout;
    btnRow->addWidget(m_addBtn);
    btnRow->addWidget(m_editBtn);
    btnRow->addWidget(m_removeBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_upBtn);
    btnRow->addWidget(m_downBtn);
    v->addLayout(btnRow);

    m_tabs->addTab(act, i18n("Actions"));

    /* ---------------- Dialog buttons ---------------- */
    auto *bb = new QDialogButtonBox(QDialogButtonBox::RestoreDefaults
                                    |QDialogButtonBox::Ok
                                    |QDialogButtonBox::Cancel,
                                    this);
    connect(bb->button(QDialogButtonBox::RestoreDefaults), &QAbstractButton::clicked,
            this, [this]{
                m_cfg->reset();
                m_apiKey->setPassword(m_cfg->apiKey());
                m_endpoint->setText(m_cfg->apiEndpoint());
                m_model->setText(m_cfg->model());
                m_systemPrompt->setPlainText(m_cfg->systemPrompt());
                m_notificationsCb->setChecked(m_cfg->notificationsEnabled());
                loadActions();
            });
    connect(bb, &QDialogButtonBox::accepted, this, &SettingsDialog::store);
    connect(bb, &QDialogButtonBox::rejected, this, &SettingsDialog::cancel);

    auto *lay = new QVBoxLayout(this);
    lay->addWidget(m_tabs);
    lay->addWidget(bb);

    /* Fill from cfg */
    m_apiKey ->setPassword(m_cfg->apiKey());
    m_endpoint->setText(m_cfg->apiEndpoint());
    m_model   ->setText(m_cfg->model());
    m_systemPrompt->setPlainText(m_cfg->systemPrompt());
    m_notificationsCb->setChecked(m_cfg->notificationsEnabled());

    loadActions();
    validateEndpoint();
}

void SettingsDialog::validateEndpoint()
{
    bool ok = urlRx.match(m_endpoint->text()).hasMatch();
    m_endpointWarn->setVisible(!ok);
    m_endpointWarn->setText(ok ? QString() : i18n("Endpoint looks invalid."));
}

void SettingsDialog::loadActions()
{
    m_list->clear();
    for (const auto &a : m_cfg->actions())
        m_list->addItem(a.name);
    updateButtons();
}

void SettingsDialog::updateButtons()
{
    const int row = m_list->currentRow();
    const int count = m_list->count();
    const bool hasSelection = row >= 0;

    m_editBtn  ->setEnabled(hasSelection);
    m_removeBtn->setEnabled(hasSelection);
    m_upBtn    ->setEnabled(hasSelection && row > 0);
    m_downBtn  ->setEnabled(hasSelection && row < count - 1);

    if (auto* okBtn = findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok))
        okBtn->setEnabled(count > 0);
}

void SettingsDialog::addAction()
{
    ActionEditorDialog dlg(this);
    if (dlg.exec()==QDialog::Accepted)
    {
        m_cfg->setActions(m_cfg->actions()+QVector<CustomAction>{dlg.action()});
        loadActions();
    }
}

void SettingsDialog::editAction()
{
    int row = m_list->currentRow();
    if (row<0) return;
    ActionEditorDialog dlg(this);
    dlg.setAction(m_cfg->actions()[row]);
    if (dlg.exec()==QDialog::Accepted)
    {
        auto acts=m_cfg->actions();
        acts[row]=dlg.action();
        m_cfg->setActions(acts);
        loadActions();
    }
}

void SettingsDialog::removeAction()
{
    int row = m_list->currentRow();
    if (row<0) return;
    auto acts=m_cfg->actions();
    acts.remove(row);
    m_cfg->setActions(acts);
    loadActions();
}

void SettingsDialog::moveUp()
{
    int row=m_list->currentRow();
    if (row>0)
    {
        auto acts=m_cfg->actions();
        acts.swapItemsAt(row,row-1);
        m_cfg->setActions(acts);
        loadActions();
        m_list->setCurrentRow(row-1);
    }
}

void SettingsDialog::moveDown()
{
    int row=m_list->currentRow();
    if (row>=0 && row< m_cfg->actions().size()-1)
    {
        auto acts=m_cfg->actions();
        acts.swapItemsAt(row,row+1);
        m_cfg->setActions(acts);
        loadActions();
        m_list->setCurrentRow(row+1);
    }
}

void SettingsDialog::testApiKey()
{
    if (m_apiKey->password().trimmed().isEmpty()) {
        QMessageBox::warning(this, i18n("Missing key"),
                             i18n("Enter an API key first."));
        return;
    }
    auto apiClient = new ApiClient(m_apiKey->password().trimmed(),
                                   m_endpoint->text().trimmed(),
                                   m_model->text().trimmed(),
                                   m_systemPrompt->toPlainText().trimmed(),
                                   this);
    connect(apiClient, &ApiClient::processingFinished,
            this, [this](const QString &resultText) {
                QMessageBox::information(this, i18n("Test result"),
                                         i18n("API key is valid. Result:\n%1", resultText));
            });
    connect(apiClient, &ApiClient::processingError,
            this, [this](const QString &errorMsg) {
                QMessageBox::warning(this, i18n("Test result"),
                                     i18n("API key is invalid.\n%1", errorMsg));
            });
    apiClient->processText(QStringLiteral("Hello world!"),
                           QStringLiteral("Translate to Russian:"));
}

void SettingsDialog::store()
{
    if (!urlRx.match(m_endpoint->text()).hasMatch())
        return;

    m_cfg->setApiKey(m_apiKey->password().trimmed());
    m_cfg->setApiEndpoint(m_endpoint->text().trimmed());
    m_cfg->setModel(m_model->text().trimmed());
    m_cfg->setSystemPrompt(m_systemPrompt->toPlainText().trimmed());
    m_cfg->setNotificationsEnabled(m_notificationsCb->isChecked());
    m_cfg->sync();
    accept();
}

void SettingsDialog::cancel()
{
    m_cfg->load();
    m_apiKey->setPassword(m_cfg->apiKey());
    m_endpoint->setText(m_cfg->apiEndpoint());
    m_model->setText(m_cfg->model());
    m_systemPrompt->setPlainText(m_cfg->systemPrompt());
    m_notificationsCb->setChecked(m_cfg->notificationsEnabled());
    reject();
}
