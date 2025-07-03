#ifndef SHORTCUTMANAGER_H
#define SHORTCUTMANAGER_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QKeySequence>
#include <QSettings>
#include <QAction>

class ShortcutManager : public QObject
{
    Q_OBJECT

public:
    explicit ShortcutManager(QObject *parent = nullptr);
    ~ShortcutManager();

    // Obtener la secuencia de teclas para una acción
    QKeySequence getShortcut(const QString &actionName) const;

    // Establecer la secuencia de teclas para una acción
    void setShortcut(const QString &actionName, const QKeySequence &shortcut);

    // Registrar una acción con su atajo predeterminado
    void registerAction(QAction *action, const QString &actionName, const QKeySequence &defaultShortcut);

    // Guardar todos los atajos
    void saveShortcuts();

    // Cargar todos los atajos
    void loadShortcuts();

    // Restablecer un atajo a su valor predeterminado
    void resetShortcut(const QString &actionName);

    // Restablecer todos los atajos a sus valores predeterminados
    void resetAllShortcuts();

    // Obtener todas las acciones registradas
    QMap<QString, QAction*> getRegisteredActions() const;

private:
    QMap<QString, QAction*> m_registeredActions;
    QMap<QString, QKeySequence> m_defaultShortcuts;
    QSettings m_settings;
};

#endif // SHORTCUTMANAGER_H
