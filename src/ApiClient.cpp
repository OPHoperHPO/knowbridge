#include "ApiClient.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QVariant> // For QNetworkRequest::ContentTypeHeader

ApiClient::ApiClient(const QString &apiKey, QObject *parent)
        : QObject(parent), m_apiKey(apiKey)
{
    // The QNetworkAccessManager should be parented to ensure proper cleanup
    m_networkManager = new QNetworkAccessManager(this);
}

QString ApiClient::getSystemMessage() const {
    // Use QString::fromUtf8 for strings with non-ASCII characters
    return QString::fromUtf8("You are an AI text editor. Strictly follow the instructions. Return *only* the modified text: no introductions, explanations, comments, apologies, or phrases like 'Here is the corrected text:'.");
}

QString ApiClient::getUserPrompt(TextAction action) const {
    // Use QString::fromUtf8 for strings with non-ASCII characters
    switch (action) {
        case TextAction::FixGrammar:
            return QString::fromUtf8("Correct typos, punctuation, grammar, and capitalization in the text. Preserve the original line breaks.");
        case TextAction::FixStructure:
            return QString::fromUtf8("Improve the structure and coherence of the text for clarity and logic. Rearrange sentences/paragraphs if needed. Preserve the original meaning and structural line breaks (paragraphs, lists).");
        case TextAction::ImproveStyle:
            return QString::fromUtf8("Improve the style of the text: word choice, sentence variety, readability. Preserve the original meaning and structural line breaks.");
        case TextAction::MakeFormal:
            return QString::fromUtf8("Rewrite the text in a formal business tone. Avoid slang, contractions, and colloquial expressions. Preserve the main idea and structure (paragraphs).");
        case TextAction::SimplifyText:
            return QString::fromUtf8("Simplify the text: use simple words and short sentences for easy understanding. Preserve the main idea and structure (paragraphs).");
        default:
            qWarning() << "Unknown text action requested:" << static_cast<int>(action);
            // Provide a default fallback prompt
            return QString::fromUtf8("Fix obvious errors in the text. Preserve the original line breaks.");
    }
}


void ApiClient::processText(const QString &text, TextAction action) {
    if (m_apiKey.isEmpty()) {
        // Use Q_EMIT macro instead of 'emit' keyword
        // Use QStringLiteral for compile-time string construction
        Q_EMIT processingError(QStringLiteral("API Key is missing. Set the OPENAI_API_KEY environment variable."));
        return;
    }

    QNetworkRequest request(m_apiUrl);
    // Use QLatin1String for standard ASCII header values
    request.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("application/json"));
    // Construct Authorization header safely (ensure API key is UTF-8)
    QByteArray authHeaderValue = "Bearer " + m_apiKey.toUtf8();
    request.setRawHeader("Authorization", authHeaderValue);

    // Construct the JSON payload
    QJsonObject rootObject;
    // Use QStringLiteral for JSON keys
    rootObject.insert(QStringLiteral("model"), m_model);

    QJsonArray messagesArray;
    QJsonObject systemMessage;
    systemMessage.insert(QStringLiteral("role"), QStringLiteral("system"));
    systemMessage.insert(QStringLiteral("content"), getSystemMessage());
    messagesArray.append(systemMessage);

    QJsonObject userMessage;
    userMessage.insert(QStringLiteral("role"), QStringLiteral("user"));
    // Combine prompt and text, use QLatin1String for the separator
    QString userContent = getUserPrompt(action) + QLatin1String("\n\n") + text;
    userMessage.insert(QStringLiteral("content"), userContent); // <-- FIX
    messagesArray.append(userMessage);

    rootObject.insert(QStringLiteral("messages"), messagesArray);
    // Optional: Add other parameters like temperature, max_tokens if needed
    // rootObject.insert(QStringLiteral("temperature"), 0.7);
    // rootObject.insert(QStringLiteral("max_tokens"), 1000); // Example limit

    QJsonDocument jsonDoc(rootObject);
    QByteArray jsonData = jsonDoc.toJson();

    // Use QString::fromUtf8 for debug output of potentially non-ASCII JSON
    qDebug() << "Sending request to OpenAI:" << QString::fromUtf8(jsonData);

    QNetworkReply *reply = m_networkManager->post(request, jsonData);
// Connect the finished signal to our handler slot
// FIX: Use a lambda to connect finished() signal to handleNetworkReply(QNetworkReply*) slot
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { // <-- CORRECTED LAMBDA CAPTURE
     handleNetworkReply(reply);
    });
}

// This function must be declared as a slot in the header using Q_SLOTS
void ApiClient::handleNetworkReply(QNetworkReply *reply) {
    if (!reply) {
        qWarning() << "Received null reply pointer in handleNetworkReply.";
        // Use Q_EMIT and QStringLiteral
        Q_EMIT processingError(QStringLiteral("Network error: Invalid reply object received."));
        return;
    }

    // Ensure the reply object is deleted later to prevent memory leaks
    reply->deleteLater();

    // Check for network errors (e.g., connection refused, timeout)
    if (reply->error() != QNetworkReply::NoError) {
        QString errorString = reply->errorString(); // Cache before reading body potentially clears it
        QByteArray replyBody = reply->readAll(); // Read body for more context
        qWarning() << "Network Error:" << errorString;
        qWarning() << "Response Body (if any):" << QString::fromUtf8(replyBody);
        // Use Q_EMIT and construct error message
        Q_EMIT processingError(QStringLiteral("Network error: ") + errorString);
        return;
    }

    // Read the response data
    QByteArray responseData = reply->readAll();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);

    // Check if JSON parsing failed
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        qWarning() << "Failed to parse JSON response. Raw data:" << QString::fromUtf8(responseData);
        // Use Q_EMIT and QStringLiteral
        Q_EMIT processingError(QStringLiteral("Error parsing API response."));
        return;
    }

    // Log the received JSON for debugging
    QJsonObject jsonObject = jsonDoc.object();
    qDebug() << "Received response from OpenAI:" << QString::fromUtf8(jsonDoc.toJson(QJsonDocument::Compact));

    // Check for API errors reported within the JSON structure
    // Use QStringLiteral for key checking
    if (jsonObject.contains(QStringLiteral("error"))) {
        QJsonObject errorObj = jsonObject[QStringLiteral("error")].toObject();
        // Provide a default message if "message" key is missing
        QString errorMsg = errorObj.value(QStringLiteral("message")).toString(QStringLiteral("Unknown API error structure"));
        qWarning() << "API Error:" << errorMsg;
        // Use Q_EMIT and construct error message
        Q_EMIT processingError(QStringLiteral("API Error: ") + errorMsg);
        return;
    }


    // Extract the content from the expected response structure
    // Use QStringLiteral for key checking
    if (jsonObject.contains(QStringLiteral("choices")) && jsonObject[QStringLiteral("choices")].isArray()) {
        QJsonArray choicesArray = jsonObject[QStringLiteral("choices")].toArray();
        if (!choicesArray.isEmpty()) {
            // Assume the first choice is the relevant one
            QJsonObject firstChoice = choicesArray.first().toObject();
            if (firstChoice.contains(QStringLiteral("message")) && firstChoice[QStringLiteral("message")].isObject()) {
                QJsonObject messageObject = firstChoice[QStringLiteral("message")].toObject();
                if (messageObject.contains(QStringLiteral("content")) && messageObject[QStringLiteral("content")].isString()) {
                    QString resultText = messageObject[QStringLiteral("content")].toString();
                    // Use Q_EMIT to signal success
                    Q_EMIT processingFinished(resultText.trimmed()); // Trim leading/trailing whitespace
                    return; // Success, exit the function
                }
            }
        }
    }

    // If we reach here, the response format was not as expected
    qWarning() << "Could not extract content from response structure:" << QString::fromUtf8(jsonDoc.toJson(QJsonDocument::Compact));
    // Use Q_EMIT and QStringLiteral
    Q_EMIT processingError(QStringLiteral("Unexpected API response format."));
}