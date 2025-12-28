#include "BackgroundProcessor.h"

#include <QApplication>
#include <QGuiApplication>
#include <QAction>
#include <QCursor>
#include <QTimer>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QScreen>
#include <QWindow>
#include <QDebug>
#include <QVBoxLayout>
#include <QListWidget>
#include <QLabel>
#include <QKeyEvent>
#include <QEventLoop>

// Helper class for Wayland dialog event filtering
class DialogEventFilter : public QObject {
public:
    explicit DialogEventFilter(QWidget* dialog) : QObject(dialog), m_dialog(dialog) {}

protected:
    bool eventFilter(QObject* obj, QEvent* event) override {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Escape) {
                m_dialog->close();
                return true;
            }
        }
        return QObject::eventFilter(obj, event);
    }

private:
    QWidget* m_dialog;
};

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
    delete m_menuAnchor;
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
    const auto actions = m_cfg->actions();  // Store to avoid dangling reference
    for (int i = 0; i < actions.size(); ++i) {
        auto* act = m_menu->addAction(actions[i].name);
        act->setData(i);
    }
    // Note: Action selection is handled directly in showActionMenu() via exec()
    // No signal connection needed here
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
    showActionMenu();
}

void BackgroundProcessor::showActionMenu()
{
    // Get cursor position - may be unreliable on Wayland
    QPoint pos = QCursor::pos();

    // Wayland often returns (0,0) or invalid position
    // Check if position seems invalid and use fallback
    bool positionValid = true;
    if (pos.isNull() || (pos.x() == 0 && pos.y() == 0)) {
        positionValid = false;
    }

    // Also validate position is within any screen bounds
    if (positionValid) {
        bool withinScreen = false;
        const auto screens = QGuiApplication::screens();
        for (const QScreen* screen : screens) {
            if (screen->geometry().contains(pos)) {
                withinScreen = true;
                break;
            }
        }
        if (!withinScreen && !screens.isEmpty()) {
            positionValid = false;
        }
    }

    // Fallback: use center of primary screen
    if (!positionValid) {
        QScreen* screen = QGuiApplication::primaryScreen();
        if (screen) {
            pos = screen->geometry().center();
            qDebug() << "KnowBridge: Using fallback menu position (Wayland cursor position unavailable)";
        }
    }

    const bool isWayland = QGuiApplication::platformName() == QStringLiteral("wayland");

    if (isWayland) {
        // On Wayland, popups require a parent window that has received input.
        // QMenu popup doesn't work with global shortcuts, so use a dialog-style window instead.
        showWaylandActionDialog(pos);
    } else {
        // X11 path - direct exec works fine
        QAction* selectedAction = m_menu->exec(pos);
        if (selectedAction) {
            onActionSelected(selectedAction);
        }
    }
}

void BackgroundProcessor::showWaylandActionDialog(const QPoint& pos)
{
    // Create a tool window styled as a menu - bypasses Wayland popup restrictions
    QWidget* dialog = new QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(QStringLiteral("KnowBridge"));

    QVBoxLayout* layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(0);

    QListWidget* list = new QListWidget(dialog);
    list->setFrameShape(QFrame::NoFrame);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    list->setFocusPolicy(Qt::StrongFocus);

    // Style to look like a menu
    list->setStyleSheet(QStringLiteral(
        "QListWidget { background: palette(window); border: 1px solid palette(mid); }"
        "QListWidget::item { padding: 6px 12px; }"
        "QListWidget::item:hover { background: palette(highlight); color: palette(highlighted-text); }"
        "QListWidget::item:selected { background: palette(highlight); color: palette(highlighted-text); }"
    ));

    // Populate with actions
    const auto& actions = m_cfg->actions();
    for (int i = 0; i < actions.size(); ++i) {
        QListWidgetItem* item = new QListWidgetItem(actions[i].name);
        item->setData(Qt::UserRole, i);
        list->addItem(item);
    }

    layout->addWidget(list);

    // Size to content
    int itemHeight = list->sizeHintForRow(0);
    if (itemHeight <= 0) itemHeight = 30;
    int totalHeight = itemHeight * qMin(actions.size(), 10) + 4;  // Max 10 visible items
    int width = 200;
    for (int i = 0; i < list->count(); ++i) {
        width = qMax(width, list->sizeHintForColumn(0) + 40);
    }
    dialog->setFixedSize(qMin(width, 400), totalHeight);

    // Position the dialog
    QPoint dialogPos = pos;
    dialogPos.rx() -= dialog->width() / 2;  // Center horizontally
    dialog->move(dialogPos);

    // Track selected action
    int selectedIndex = -1;

    // Handle selection
    QObject::connect(list, &QListWidget::itemClicked, dialog, [&selectedIndex, dialog](QListWidgetItem* item) {
        selectedIndex = item->data(Qt::UserRole).toInt();
        dialog->close();
    });

    // Handle Enter key
    QObject::connect(list, &QListWidget::itemActivated, dialog, [&selectedIndex, dialog](QListWidgetItem* item) {
        selectedIndex = item->data(Qt::UserRole).toInt();
        dialog->close();
    });

    // Close on Escape key
    auto* eventFilter = new DialogEventFilter(dialog);
    list->installEventFilter(eventFilter);
    dialog->installEventFilter(eventFilter);

    // Close on focus loss - poll for window deactivation
    QTimer* deactivateTimer = new QTimer(dialog);
    deactivateTimer->setInterval(100);
    QObject::connect(deactivateTimer, &QTimer::timeout, dialog, [dialog]() {
        if (!dialog->isActiveWindow() && dialog->isVisible()) {
            dialog->close();
        }
    });
    deactivateTimer->start();

    // Show and focus
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
    list->setFocus();
    if (list->count() > 0) {
        list->setCurrentRow(0);
    }

    // Block until closed (modal behavior)
    QEventLoop loop;
    QObject::connect(dialog, &QWidget::destroyed, &loop, &QEventLoop::quit);
    loop.exec();

    // Process selection
    if (selectedIndex >= 0 && selectedIndex < actions.size()) {
        m_currentPrompt = actions[selectedIndex].prompt;
        QApplication::setOverrideCursor(Qt::BusyCursor);
        m_api->processText(m_target.text, m_currentPrompt);
    }
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
