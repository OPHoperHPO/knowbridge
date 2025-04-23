// File: src/ConfigManager.h
#pragma once
#include <QObject>
#include <KConfig>
#include <QVector>

/* -------- пользовательские действия ---------- */
struct CustomAction {
    QString name;
    QString prompt;
};

class ConfigManager : public QObject
{
Q_OBJECT
public:
    explicit ConfigManager(QObject *parent = nullptr);

    /*--- простые параметры ---*/
    QString apiKey()      const { return m_apiKey; }
    QString apiEndpoint() const { return m_endpoint; }
    QString model()       const { return m_model;   }
    QString systemPrompt() const { return m_systemPrompt; }
    bool notificationsEnabled() const { return m_notificationsEnabled; }

    void setApiKey     (const QString &v) { m_apiKey = v; }
    void setApiEndpoint(const QString &v) { m_endpoint = v; }
    void setModel      (const QString &v) { m_model = v; }
    void setSystemPrompt(const QString &v) { m_systemPrompt = v; }
    void setNotificationsEnabled(bool v) { m_notificationsEnabled = v; }

    /*--- действия ---*/
    QVector<CustomAction> actions() const { return m_actions; }
    void setActions(const QVector<CustomAction>& v) { m_actions = v; }

    void sync();                 // записать на диск
    void load();                 // прочитать из диска
    void reset();

Q_SIGNALS:
    void configChanged();

private:
    KConfig             m_cfg{ QStringLiteral("knowbridge") };
    QString             m_apiKey;
    QString             m_endpoint;
    QString             m_model;
    QString             m_systemPrompt;
    bool                m_notificationsEnabled = true;
    QVector<CustomAction> m_actions;
};
