#ifndef APICLIENT_H
#define APICLIENT_H

#include <QObject>
#include <QString>
#include <QUrl>
#include "TextAction.h" // Include the TextAction enum definition

// Forward declare Qt Network classes used in private slots/members
class QNetworkAccessManager;
class QNetworkReply;

class ApiClient : public QObject
{
Q_OBJECT // Macro needed for signals and slots

public:
    explicit ApiClient(const QString &apiKey, QObject *parent = nullptr);
    ~ApiClient() override = default; // Use default destructor

    // Function to initiate the text processing request
    void processText(const QString &text, TextAction action);

    // Functions to get standard messages (as provided in the request)
    QString getSystemMessage() const;
    QString getUserPrompt(TextAction action) const;


Q_SIGNALS: // Use Q_SIGNALS macro instead of 'signals' keyword
    // Signal emitted when processing is successful
    void processingFinished(const QString &resultText);
    // Signal emitted when an error occurs
    void processingError(const QString &errorMsg);

private Q_SLOTS: // Use Q_SLOTS macro instead of 'slots' keyword
    // Slot to handle the finished network reply
    void handleNetworkReply(QNetworkReply *reply);

private:
    QString m_apiKey;
    QNetworkAccessManager *m_networkManager; // Pointer is owned by this object
    // Use QStringLiteral for compile-time string construction
    const QUrl m_apiUrl = QUrl(QStringLiteral("http://localhost:30000/v1/chat/completions"));
    const QString m_model = QStringLiteral("localqwen"); // Or choose another model
};

#endif // APICLIENT_H