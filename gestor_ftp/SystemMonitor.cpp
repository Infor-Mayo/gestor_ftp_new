#include "SystemMonitor.h"
#include <QDebug>
#include <QRandomGenerator>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#elif defined(Q_OS_LINUX)
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <sys/sysinfo.h>
#elif defined(Q_OS_MACOS)
#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#endif

SystemMonitor::SystemMonitor(QObject *parent) : QObject(parent) {
    connect(&timer, &QTimer::timeout, this, &SystemMonitor::updateMetrics);
    timer.start(1000); // Actualizar cada segundo
    
#ifdef Q_OS_WIN
    // Inicializar contadores de rendimiento de Windows
    PdhOpenQuery(NULL, 0, &cpuQuery);
    PdhAddEnglishCounter(cpuQuery, L"\\Processor(_Total)\\% Processor Time", 0, &cpuTotal);
    PdhCollectQueryData(cpuQuery);
#endif
    
    qDebug() << "SystemMonitor inicializado";
}

SystemMonitor::~SystemMonitor() {
#ifdef Q_OS_WIN
    if (cpuQuery) {
        PdhCloseQuery(cpuQuery);
    }
#endif
}

void SystemMonitor::updateMetrics() {
    qreal cpu = getCpuUsage();
    qreal memory = getMemoryUsage();
    qreal network = getNetworkUsage();
    
    emit metricsUpdated(cpu, memory, network);
}

qreal SystemMonitor::getCpuUsage() {
#ifdef Q_OS_WIN
    PDH_FMT_COUNTERVALUE counterVal;
    
    PdhCollectQueryData(cpuQuery);
    PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, NULL, &counterVal);
    
    return static_cast<qreal>(counterVal.doubleValue);
    
#elif defined(Q_OS_LINUX)
    static unsigned long long lastTotalUser = 0, lastTotalUserLow = 0, lastTotalSys = 0, lastTotalIdle = 0;
    
    std::ifstream file("/proc/stat");
    std::string line;
    std::getline(file, line);
    
    std::istringstream iss(line);
    std::string cpu_label;
    unsigned long long user, nice, system, idle;
    iss >> cpu_label >> user >> nice >> system >> idle;
    
    unsigned long long totalUser = user + nice;
    unsigned long long totalSys = system;
    unsigned long long totalIdle = idle;
    unsigned long long total = totalUser + totalSys + totalIdle;
    
    if (lastTotalUser != 0) {
        unsigned long long totalDiff = total - (lastTotalUser + lastTotalSys + lastTotalIdle);
        unsigned long long idleDiff = totalIdle - lastTotalIdle;
        
        if (totalDiff > 0) {
            return static_cast<qreal>((totalDiff - idleDiff) * 100.0 / totalDiff);
        }
    }
    
    lastTotalUser = totalUser;
    lastTotalSys = totalSys;
    lastTotalIdle = totalIdle;
    
    return 0.0;
    
#elif defined(Q_OS_MACOS)
    host_cpu_load_info_data_t cpuinfo;
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    
    if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, 
                       (host_info_t)&cpuinfo, &count) == KERN_SUCCESS) {
        
        static unsigned int lastUser = 0, lastSystem = 0, lastIdle = 0, lastNice = 0;
        
        unsigned int user = cpuinfo.cpu_ticks[CPU_STATE_USER];
        unsigned int system = cpuinfo.cpu_ticks[CPU_STATE_SYSTEM];
        unsigned int idle = cpuinfo.cpu_ticks[CPU_STATE_IDLE];
        unsigned int nice = cpuinfo.cpu_ticks[CPU_STATE_NICE];
        
        unsigned int totalTicks = (user - lastUser) + (system - lastSystem) + 
                                 (idle - lastIdle) + (nice - lastNice);
        unsigned int usedTicks = (user - lastUser) + (system - lastSystem) + (nice - lastNice);
        
        lastUser = user;
        lastSystem = system;
        lastIdle = idle;
        lastNice = nice;
        
        if (totalTicks > 0) {
            return static_cast<qreal>(usedTicks * 100.0 / totalTicks);
        }
    }
    
    return 0.0;
    
#else
    // Fallback para sistemas no soportados
    return 0.0;
#endif
}

qreal SystemMonitor::getMemoryUsage() {
#ifdef Q_OS_WIN
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    
    DWORDLONG totalPhysMem = memInfo.ullTotalPhys;
    DWORDLONG physMemUsed = memInfo.ullTotalPhys - memInfo.ullAvailPhys;
    
    return static_cast<qreal>(physMemUsed * 100.0 / totalPhysMem);
    
#elif defined(Q_OS_LINUX)
    struct sysinfo memInfo;
    sysinfo(&memInfo);
    
    long long totalPhysMem = memInfo.totalram * memInfo.mem_unit;
    long long physMemUsed = (memInfo.totalram - memInfo.freeram) * memInfo.mem_unit;
    
    return static_cast<qreal>(physMemUsed * 100.0 / totalPhysMem);
    
#elif defined(Q_OS_MACOS)
    vm_size_t page_size;
    vm_statistics64_data_t vm_stat;
    mach_msg_type_number_t count = sizeof(vm_stat) / sizeof(natural_t);
    
    if (host_page_size(mach_host_self(), &page_size) == KERN_SUCCESS &&
        host_statistics64(mach_host_self(), HOST_VM_INFO, 
                         (host_info64_t)&vm_stat, &count) == KERN_SUCCESS) {
        
        long long total_mem = (vm_stat.free_count + vm_stat.active_count + 
                              vm_stat.inactive_count + vm_stat.wire_count) * page_size;
        long long used_mem = (vm_stat.active_count + vm_stat.inactive_count + 
                             vm_stat.wire_count) * page_size;
        
        return static_cast<qreal>(used_mem * 100.0 / total_mem);
    }
    
    return 0.0;
    
#else
    return 0.0;
#endif
}

qreal SystemMonitor::getNetworkUsage() {
    // Implementación básica - en un sistema real necesitarías 
    // monitorear interfaces de red específicas
    
#ifdef Q_OS_WIN
    // En Windows podrías usar GetIfTable2 para obtener estadísticas de red
    // Por simplicidad, devolvemos un valor simulado
    static qreal networkUsage = 0.0;
    networkUsage += (QRandomGenerator::global()->bounded(10) - 5) * 0.1; // Simulación
    networkUsage = qMax(0.0, qMin(100.0, networkUsage));
    return networkUsage;
    
#elif defined(Q_OS_LINUX)
    // En Linux podrías leer /proc/net/dev
    std::ifstream file("/proc/net/dev");
    if (file.is_open()) {
        std::string line;
        // Saltar las dos primeras líneas (headers)
        std::getline(file, line);
        std::getline(file, line);
        
        static unsigned long long lastRxBytes = 0, lastTxBytes = 0;
        unsigned long long totalRx = 0, totalTx = 0;
        
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string interface;
            unsigned long long rxBytes, rxPackets, rxErrs, rxDrop, rxFifo, rxFrame, rxCompressed, rxMulticast;
            unsigned long long txBytes, txPackets, txErrs, txDrop, txFifo, txColls, txCarrier, txCompressed;
            
            iss >> interface >> rxBytes >> rxPackets >> rxErrs >> rxDrop >> rxFifo >> rxFrame >> rxCompressed >> rxMulticast
                >> txBytes >> txPackets >> txErrs >> txDrop >> txFifo >> txColls >> txCarrier >> txCompressed;
            
            // Ignorar la interfaz loopback
            if (interface.find("lo:") == std::string::npos) {
                totalRx += rxBytes;
                totalTx += txBytes;
            }
        }
        
        if (lastRxBytes != 0 && lastTxBytes != 0) {
            unsigned long long diffRx = totalRx - lastRxBytes;
            unsigned long long diffTx = totalTx - lastTxBytes;
            unsigned long long totalDiff = diffRx + diffTx;
            
            // Convertir a porcentaje basado en un ancho de banda estimado (100 Mbps)
            const unsigned long long maxBandwidth = 100 * 1024 * 1024 / 8; // 100 Mbps en bytes/s
            qreal usage = static_cast<qreal>(totalDiff * 100.0 / maxBandwidth);
            
            lastRxBytes = totalRx;
            lastTxBytes = totalTx;
            
            return qMin(100.0, usage);
        }
        
        lastRxBytes = totalRx;
        lastTxBytes = totalTx;
    }
    
    return 0.0;
    
#else
    // Fallback: valor simulado
    static qreal networkUsage = 0.0;
    networkUsage += (QRandomGenerator::global()->bounded(10) - 5) * 0.1;
    networkUsage = qMax(0.0, qMin(100.0, networkUsage));
    return networkUsage;
#endif
}
