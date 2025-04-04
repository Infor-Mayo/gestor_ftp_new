#ifndef THEME_MANAGER_H
#define THEME_MANAGER_H

#include <QString>
#include <QFile>
#include <QApplication>
#include <QSettings>

class ThemeManager {
public:
    static void loadTheme(const QString &themeName) {
        QString qssPath = QString(":/styles/%1.qss").arg(themeName);
        QFile file(qssPath);
        
        if (file.open(QFile::ReadOnly | QFile::Text)) {
            QString styleSheet = QLatin1String(file.readAll());
            qApp->setStyleSheet(styleSheet);
            
            QSettings settings("MiEmpresa", "GestorFTP");
            settings.setValue("theme", themeName);
        }
    }
    
    static QString getCurrentTheme() {
        QSettings settings("MiEmpresa", "GestorFTP");
        return settings.value("theme", "light").toString();
    }
};

#endif // THEME_MANAGER_H
