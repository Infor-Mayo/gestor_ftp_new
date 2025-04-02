#include "TransferWorker.h"
#include <QElapsedTimer>
#include <QThread>

TransferWorker::TransferWorker(QFile *file, QTcpSocket *dataSocket, QObject *parent)
    : QObject(parent), file(file), dataSocket(dataSocket) 
{
    if(dataSocket) {
        dataSocket->setParent(nullptr);
        if(QThread::currentThread()) {
            dataSocket->moveToThread(QThread::currentThread());
        }
    }
}

void TransferWorker::startDownload() {
    try {
        emit logMessage("Iniciando descarga...");
        emit progress(0);
        
        if(!file->open(QIODevice::ReadOnly)) {
            throw std::runtime_error("550 Error opening file");
        }
        
        qint64 fileSize = file->size();
        qint64 bytesSent = 0;
        char buffer[4 * 1024 * 1024]; // 4MB buffer
        
        while(!file->atEnd() && !dataSocket.isNull()) {
            qint64 bytesRead = file->read(buffer, sizeof(buffer));
            qint64 bytesWritten = 0;
            
            while(bytesWritten < bytesRead && dataSocket->isValid()) {
                qint64 result = dataSocket->write(buffer + bytesWritten, bytesRead - bytesWritten);
                if(result == -1) throw std::runtime_error("426 Write error");
                bytesWritten += result;
                
                if(!dataSocket->waitForBytesWritten(5000)) {
                    throw std::runtime_error("421 Timeout");
                }
            }
            
            bytesSent += bytesWritten;
            emit progress(bytesSent);
        }
        
        emit finished();
    } catch(const std::exception &e) {
        emit error(QString::fromStdString(e.what()));
        emit logMessage("Error crítico en descarga");
    }
    
    file->close();
}

void TransferWorker::startUpload() {
    try {
        emit logMessage("Iniciando subida...");
        emit progress(0);
        
        if(!file->open(QIODevice::WriteOnly)) {
            throw std::runtime_error("550 Error creating file");
        }
        
        qint64 totalReceived = 0;
        QByteArray buffer;
        buffer.reserve(4 * 1024 * 1024); // 4MB buffer
        
        while(!dataSocket.isNull() && dataSocket->isValid()) {
            if(dataSocket->waitForReadyRead(5000)) {
                buffer = dataSocket->readAll();
                totalReceived += buffer.size();
                
                if(file->write(buffer) != buffer.size()) {
                    throw std::runtime_error("451 Write error");
                }
                
                emit progress(totalReceived);
            } else {
                throw std::runtime_error("421 Timeout esperando datos");
            }
        }
        
        emit finished();
    } catch(const std::exception &e) {
        emit error(QString::fromStdString(e.what()));
        emit logMessage("Error crítico en subida");
    }
    
    file->close();
} 