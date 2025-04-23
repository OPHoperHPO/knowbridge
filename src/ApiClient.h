#pragma once
#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

/**
 *  Простая тонкая обёртка над Chat-completion API.
 *  Передаём текст и готовый user-prompt в `processText`.
 */
class ApiClient : public QObject
{
Q_OBJECT
public:
    explicit ApiClient(const QString& apiKey,
                       const QString& endpoint,
                       const QString& model,
                       const QString &systemPrompt,
                       QObject* parent = nullptr);
    ~ApiClient() override = default;

    void processText(const QString& text, const QString& userPrompt);

Q_SIGNALS:
    void processingFinished(const QString& resultText);
    void processingError   (const QString& errorMsg);

private Q_SLOTS:
    void handleNetworkReply(QNetworkReply* reply);

private:
    QString m_apiKey;
    QUrl    m_apiUrl;
    QString m_model;
    QNetworkAccessManager* m_net{nullptr};

    QString m_systemPrompt;
    static QString defaultSystemPrompt();   // keeps the old literal
};
