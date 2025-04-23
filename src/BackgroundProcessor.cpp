#include "BackgroundProcessor.h"

#include <QApplication>
#include <QAction>
#include <QCursor>
#include <QTimer>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QDebug>

#ifdef HAVE_KNOTIFICATIONS
#include <KNotification>
#include <QSystemTrayIcon>
#include <KLocalizedString>

#endif

BackgroundProcessor::BackgroundProcessor(ConfigManager* cfg, QObject* parent)
        : QObject(parent)
        , m_cfg(cfg)
        , m_clip(QApplication::clipboard())
        , m_menu(new QMenu)
        , m_a11y(this)
{
    createActionMenu();
    connect(m_cfg, &ConfigManager::configChanged,
            this,   &BackgroundProcessor::createActionMenu);
    // update api client
    connect(m_cfg, &ConfigManager::configChanged,
            this,   &BackgroundProcessor::setupApiClient);
    QTimer::singleShot(0, this, &BackgroundProcessor::initialize);

}

BackgroundProcessor::~BackgroundProcessor()
{
    delete m_menu;
}

void BackgroundProcessor::initialize()
{
#ifdef HAVE_ATSPI
    m_a11y.initialize();
#endif
    setupApiClient();
}

void BackgroundProcessor::setupApiClient()
{
    // free old client
    if (m_api) {
        m_api->deleteLater();
        m_api = nullptr;
    }
    m_api = new ApiClient(m_cfg->apiKey(),
                          m_cfg->apiEndpoint(),
                          m_cfg->model(),
                          m_cfg->systemPrompt(),
                          this);
    connect(m_api, &ApiClient::processingFinished,
            this, &BackgroundProcessor::handleResult);
    connect(m_api, &ApiClient::processingError,
            this, &BackgroundProcessor::handleError);
}

void BackgroundProcessor::createActionMenu()
{
    m_menu->clear();
    for (int i = 0; i < m_cfg->actions().size(); ++i) {
        const auto& a = m_cfg->actions()[i];
        auto* act = m_menu->addAction(a.name);
        act->setData(i);
    }
    connect(m_menu, &QMenu::triggered,
            this,  &BackgroundProcessor::onActionSelected);
}

void BackgroundProcessor::onShortcutActivated()
{
    if (m_processing) return;
    m_target = ElementInfo();

#ifdef HAVE_ATSPI
    if (m_a11y.isInitialized())
        m_target = m_a11y.getFocusedElementInfo();
#endif
    if (!m_target.isValid || m_target.text.trimmed().isEmpty())
        m_target.text = m_clip->text(QClipboard::Selection).trimmed();
    if (m_target.text.isEmpty())
        m_target.text = m_clip->text().trimmed();

    if (m_target.text.isEmpty()) {
        notify(i18n("Nothing to process"),
               i18n("No text was found in focus or clipboard."),
               false);
        return;
    }
    m_menu->popup(QCursor::pos());
}

void BackgroundProcessor::onActionSelected(QAction* act)
{
    const int idx = act->data().toInt();
    if (idx < 0 || idx >= m_cfg->actions().size())
        return;

    m_currentPrompt = m_cfg->actions()[idx].prompt;

    QApplication::setOverrideCursor(Qt::BusyCursor);
    QSystemTrayIcon* tray = qobject_cast<QSystemTrayIcon*>(sender());
    if (tray)
        tray->setToolTip(i18n("Processing…"));
    m_api->processText(m_target.text, m_currentPrompt);
}

void BackgroundProcessor::handleResult(const QString& text)
{
    QApplication::restoreOverrideCursor();
    m_processing = false;

#ifdef HAVE_ATSPI
    bool ok = false;
    if (m_target.isEditable && m_target.accessible &&
        m_a11y.isInitialized())
        ok = m_a11y.replaceTextInElement(m_target, text);
    if (ok) {
        notify(i18n("Done"), i18n("Text was replaced."), false);
        return;
    }
#endif
    clipboardFallback(text, i18n("Inserted into clipboard."));
}

void BackgroundProcessor::handleError(const QString& err)
{
    QApplication::restoreOverrideCursor();
    m_processing = false;
    notify(i18n("Error"), err, true);
}

void BackgroundProcessor::clipboardFallback(const QString& text,
                                            const QString& why)
{
    m_clip->setText(text);
    notify(i18n("Result copied"), why, false);
}

void BackgroundProcessor::notify(const QString& title,
                                 const QString& body,
                                 bool error)
{
    // Respect the user’s preference
    if (!m_cfg->notificationsEnabled())
        return;
#ifdef HAVE_KNOTIFICATIONS
    KNotification* n = new KNotification(QStringLiteral("knowbridge"));
    n->setTitle(title);
    n->setText(body);
    n->setIconName(error ? QStringLiteral("dialog-error")
                         : QStringLiteral("dialog-information"));
    n->setUrgency(error ? KNotification::CriticalUrgency
                        : KNotification::NormalUrgency);
    n->setAutoDelete(true);
    n->setFlags(KNotification::CloseOnTimeout | KNotification::CloseWhenWindowActivated);
    connect(n, &KNotification::closed, n, &QObject::deleteLater);
    n->sendEvent();
#else
    (error ? qWarning() : qInfo()) << qPrintable(title) << "-" << qPrintable(body);
#endif
}
