#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QMessageLogContext>

class Logger : public QObject
{
    Q_OBJECT
public:
    static Logger& instance();
    static void init(QObject *receiver = nullptr);
    static void setReceiver(QObject *receiver);

signals:
    void newLogMessage(QString message);

private:
    explicit Logger(QObject *parent = nullptr);
    static QObject *logReceiver;
    static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);
};

#endif // LOGGER_H
