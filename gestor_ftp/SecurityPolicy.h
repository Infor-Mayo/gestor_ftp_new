#ifndef SECURITYPOLICY_H
#define SECURITYPOLICY_H

#include <QObject>
#include <QString>
#include <QDebug>
#include <QRegularExpression>
#include <QHostAddress>
#include <QSet>

class SecurityPolicy : public QObject {
    Q_OBJECT
public:
    static bool isIPAllowed(const QHostAddress &ip) {
        // Implementar lista blanca/negra
        QSet<QString> blacklist = {"192.168.1.100", "10.0.0.5"};
        return !blacklist.contains(ip.toString());
    }
    
    static bool validateUsername(const QString &username) {
        return username.length() >= 4 && 
               username.length() <= 20 &&
               !username.contains(QRegularExpression("[^a-zA-Z0-9_-]"));
    }
    
    static bool validatePassword(const QString &password) {
        return password.length() >= 8 &&
               password.contains(QRegularExpression("[A-Z]")) &&
               password.contains(QRegularExpression("[0-9]")) &&
               password.contains(QRegularExpression("[!@#$%^&*]"));
    }
};

#endif // SECURITYPOLICY_H
