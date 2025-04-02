#include <QObject>
#include <QFile>
#include <QTcpSocket>
#include <QPointer>
#include <QThread>

class TransferWorker : public QObject {
    Q_OBJECT
public:
    explicit TransferWorker(QFile *file, QTcpSocket *dataSocket, QObject *parent = nullptr);

public slots:
    void startDownload();
    void startUpload();

signals:
    void progress(qint64 bytes);
    void finished();
    void error(QString message);
    void logMessage(QString text);

private:
    QPointer<QFile> file;
    QPointer<QTcpSocket> dataSocket;
}; 