#pragma once
#include <QObject>
#include <QString>
#include <QClipboard>
#include <QMenu>

#include "AccessibilityHelper.h"
#include "ConfigManager.h"
#include "ApiClient.h"

/**
 *  Управляет жизненным циклом операции:
 *  1. Читает выделенный/клипборд-текст.
 *  2. Показывает меню с пользовательскими действиями.
 *  3. Отправляет запрос в ApiClient, показывает прогресс.
 *  4. Вставляет результат через AT-SPI или падает в буфер обмена.
 */
class BackgroundProcessor : public QObject
{
Q_OBJECT
public:
    explicit BackgroundProcessor(ConfigManager* cfg,
                                 QObject* parent = nullptr);
    ~BackgroundProcessor() override;

    Q_INVOKABLE void onShortcutActivated();

private Q_SLOTS:
    void initialize();                  // отложенный старт
    void onActionSelected(QAction* act);

    void handleResult(const QString& text);
    void handleError (const QString& err);

private:
    void setupApiClient();
    void createActionMenu();
    void notify(const QString& title,
                const QString& text,
                bool error = false);
    void clipboardFallback(const QString& text,
                           const QString& why);

    bool               m_processing{false}; // <- добавлено
    ConfigManager*      m_cfg;
    ApiClient*          m_api{nullptr};
    QClipboard*         m_clip;
    QMenu*              m_menu;
    AccessibilityHelper m_a11y;
    ElementInfo         m_target;
    QString             m_currentPrompt;
};
