// File: src/SettingsDialog.h
#pragma once
#include <QDialog>
#include <KPasswordLineEdit>
#include "ConfigManager.h"
#include <QMessageBox>
#include <QTextEdit>
#include <QCheckBox>

class QListWidget;
class QLineEdit;
class QTabWidget;
class QLabel;

class SettingsDialog : public QDialog
{
Q_OBJECT
public:
    SettingsDialog(QWidget *parent, ConfigManager *cfg);

private Q_SLOTS:
    void addAction();
    void editAction();
    void removeAction();
    void moveUp();
    void moveDown();

    void testApiKey();
    void validateEndpoint();
    void store();
    void cancel();

private:
    void loadActions();
    void updateButtons();

    ConfigManager *m_cfg;
    QTabWidget *m_tabs;

    /* General tab */
    KPasswordLineEdit *m_apiKey;
    QLineEdit         *m_endpoint;
    QLineEdit         *m_model;
    QLabel            *m_endpointWarn;
    QTextEdit         *m_systemPrompt;
    QCheckBox         *m_notificationsCb;

    /* Actions tab */
    QListWidget *m_list;

    // Кнопки управления списком действий
    QPushButton *m_addBtn;
    QPushButton *m_editBtn;
    QPushButton *m_removeBtn;
    QPushButton *m_upBtn;
    QPushButton *m_downBtn;
};
