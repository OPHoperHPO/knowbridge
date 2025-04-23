// File: src/ConfigManager.cpp
#include "ConfigManager.h"
#include <KConfigGroup>
#include <KLocalizedString>

static auto G_GENERAL = QStringLiteral("General");
static auto G_ACT     = QStringLiteral("Actions");

ConfigManager::ConfigManager(QObject *parent)
        : QObject(parent)
{
    load();
}

/*----------- чтение ------------*/
void ConfigManager::load()
{
    KConfigGroup g(&m_cfg, G_GENERAL);
    m_apiKey   = g.readEntry("ApiKey");
    m_endpoint = g.readEntry("Endpoint",
                             QStringLiteral("http://localhost:30000/v1/chat/completions"));
    m_model    = g.readEntry("Model", QStringLiteral("localqwen"));
    m_systemPrompt = g.readEntry("SystemPrompt",
                                 i18n("You are an AI text editor. Strictly follow the instructions. "
                                      "Return ONLY the modified text—no explanations, pre-/post-amble."));
    m_notificationsEnabled = g.readEntry("NotificationsEnabled", true);

    m_actions.clear();
    const KConfigGroup a(&m_cfg, G_ACT);
    const int count = a.readEntry("Count", 0);
    for (int i = 0; i < count; ++i) {
        CustomAction ca;
        ca.name   = a.readEntry(QStringLiteral("Name%1").arg(i));
        ca.prompt = a.readEntry(QStringLiteral("Prompt%1").arg(i));
        if (!ca.name.isEmpty() && !ca.prompt.isEmpty())
            m_actions << ca;
    }
    if (m_actions.isEmpty()) { // дефолты первой загрузки
        m_actions = {
                {i18n("Fix Grammar"),  i18n("Correct typos, punctuation, grammar and capitalization.")},
                {i18n("Improve Style"), i18n("Improve clarity, word choice and readability.")},
                {i18n("Simplify Text"), i18n("Rewrite in plain language suitable for a 6-grade student.")}
        };
    }
}

/*----------- запись ------------*/
void ConfigManager::sync()
{
    KConfigGroup g(&m_cfg, G_GENERAL);
    g.writeEntry("ApiKey",    m_apiKey);
    g.writeEntry("Endpoint",  m_endpoint);
    g.writeEntry("Model",     m_model);
    g.writeEntry("SystemPrompt", m_systemPrompt);
    g.writeEntry("NotificationsEnabled", m_notificationsEnabled);

    KConfigGroup a(&m_cfg, G_ACT);
    a.deleteGroup();                       // перезаписываем
    a.writeEntry("Count", m_actions.size());
    for (int i = 0; i < m_actions.size(); ++i) {
        a.writeEntry(QStringLiteral("Name%1").arg(i),   m_actions[i].name);
        a.writeEntry(QStringLiteral("Prompt%1").arg(i), m_actions[i].prompt);
    }
    m_cfg.sync();
    Q_EMIT configChanged();
}

void ConfigManager::reset()
{
    m_cfg.deleteGroup(G_GENERAL);
    m_cfg.deleteGroup(G_ACT);
    m_cfg.sync();
    load();
}
