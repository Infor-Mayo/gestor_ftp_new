#include "ShortcutManager.h"

ShortcutManager::ShortcutManager(QObject *parent)
    : QObject(parent), m_settings("MiEmpresa", "GestorFTP")
{
}

ShortcutManager::~ShortcutManager()
{
    saveShortcuts();
}

QKeySequence ShortcutManager::getShortcut(const QString &actionName) const
{
    return m_settings.value("shortcuts/" + actionName, m_defaultShortcuts.value(actionName)).value<QKeySequence>();
}

void ShortcutManager::setShortcut(const QString &actionName, const QKeySequence &shortcut)
{
    if (m_registeredActions.contains(actionName)) {
        QAction *action = m_registeredActions[actionName];
        action->setShortcut(shortcut);
        
        // Guardar en la configuración
        m_settings.setValue("shortcuts/" + actionName, shortcut);
    }
}

void ShortcutManager::registerAction(QAction *action, const QString &actionName, const QKeySequence &defaultShortcut)
{
    if (!action || actionName.isEmpty()) {
        return;
    }

    m_registeredActions[actionName] = action;
    m_defaultShortcuts[actionName] = defaultShortcut;

    // Verificar si existe un atajo personalizado guardado
    QKeySequence customShortcut = m_settings.value("shortcuts/" + actionName, defaultShortcut).value<QKeySequence>();
    action->setShortcut(customShortcut);
}

void ShortcutManager::saveShortcuts()
{
    for (auto it = m_registeredActions.begin(); it != m_registeredActions.end(); ++it) {
        QString actionName = it.key();
        QAction *action = it.value();
        m_settings.setValue("shortcuts/" + actionName, action->shortcut());
    }
}

void ShortcutManager::loadShortcuts()
{
    for (auto it = m_registeredActions.begin(); it != m_registeredActions.end(); ++it) {
        QString actionName = it.key();
        QAction *action = it.value();
        QKeySequence shortcut = m_settings.value("shortcuts/" + actionName, m_defaultShortcuts.value(actionName)).value<QKeySequence>();
        action->setShortcut(shortcut);
    }
}

void ShortcutManager::resetShortcut(const QString &actionName)
{
    if (m_registeredActions.contains(actionName) && m_defaultShortcuts.contains(actionName)) {
        QAction *action = m_registeredActions[actionName];
        QKeySequence defaultShortcut = m_defaultShortcuts[actionName];
        action->setShortcut(defaultShortcut);
        m_settings.setValue("shortcuts/" + actionName, defaultShortcut);
    }
}

void ShortcutManager::resetAllShortcuts()
{
    for (auto it = m_registeredActions.begin(); it != m_registeredActions.end(); ++it) {
        QString actionName = it.key();
        resetShortcut(actionName);
    }
}

QMap<QString, QAction*> ShortcutManager::getRegisteredActions() const
{
    return m_registeredActions;
}
