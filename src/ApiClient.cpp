#include "ApiClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QDebug>
#include <KLocalizedString>

ApiClient::ApiClient(const QString& key,
                     const QString& endpoint,
                     const QString& model,
                     const QString &systemPrompt,
                     QObject* parent)
        : QObject(parent)
        , m_apiKey(key)
        , m_apiUrl(QUrl(endpoint))
        , m_model(model)
        , m_systemPrompt(systemPrompt)
{
    m_net = new QNetworkAccessManager(this);
}

QString ApiClient::defaultSystemPrompt()
{
    return u"You are an AI text editor. "
           "Strictly follow the instructions. "
           "Return ONLY the modified text—no explanations, pre-/post-amble."_qs;
}

void ApiClient::processText(const QString& text, const QString& userPrompt)
{
    QString finalPrompt =  userPrompt + QStringLiteral("\n") + QStringLiteral("\n") + text;

    QNetworkRequest req(m_apiUrl);
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/json"));
    req.setRawHeader("Authorization",
                     "Bearer " + m_apiKey.toUtf8());

    QJsonObject root;
    root.insert(QStringLiteral("model"), m_model);

    QJsonArray messages;
    messages.append(QJsonObject{
            {u"role"_qs,    u"system"_qs},
            {u"content"_qs, m_systemPrompt.isEmpty() ? defaultSystemPrompt()
                                                     : m_systemPrompt}
    });
    messages.append(QJsonObject{
            {QStringLiteral("role"),    QStringLiteral("user")},
            {QStringLiteral("content"),  finalPrompt}
    });
    root.insert(QStringLiteral("messages"), messages);

    auto* r = m_net->post(req, QJsonDocument(root).toJson());
    connect(r, &QNetworkReply::finished,
            this, [this, r]{ handleNetworkReply(r); });
}

void ApiClient::handleNetworkReply(QNetworkReply* reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        Q_EMIT processingError(
                i18n("Network error: %1", reply->errorString()));
        return;
    }

    const auto doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isObject()) {
        Q_EMIT processingError(i18n("Malformed JSON in reply."));
        return;
    }

    const auto obj = doc.object();
    const auto choices = obj.value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty()) {
        Q_EMIT processingError(i18n("No choices in reply."));
        return;
    }

    const auto msg = choices.first().toObject()
            .value(QStringLiteral("message")).toObject()
            .value(QStringLiteral("content")).toString();
    if (msg.isEmpty()) {
        Q_EMIT processingError(i18n("Empty content in reply."));
        return;
    }

    Q_EMIT processingFinished(msg.trimmed());
}
