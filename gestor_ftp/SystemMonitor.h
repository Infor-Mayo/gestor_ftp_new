#ifndef SYSTEMMONITOR_H
#define SYSTEMMONITOR_H

#include <QObject>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#include <pdh.h>
#endif

class SystemMonitor : public QObject {
    Q_OBJECT
public:
    explicit SystemMonitor(QObject *parent = nullptr);
    ~SystemMonitor();

signals:
    void metricsUpdated(qreal cpuUsage, qreal memoryUsage, qreal networkUsage);

private slots:
    void updateMetrics();

private:
    QTimer timer;
    
#ifdef Q_OS_WIN
    PDH_HQUERY cpuQuery = nullptr;
    PDH_HCOUNTER cpuTotal = nullptr;
#endif
    
    // Funciones específicas del sistema operativo
    qreal getCpuUsage();
    qreal getMemoryUsage();
    qreal getNetworkUsage();
};

#endif // SYSTEMMONITOR_H
