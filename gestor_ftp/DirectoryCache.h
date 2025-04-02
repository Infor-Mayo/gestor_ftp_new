#pragma once
#include <QCache>
#include <QFileInfoList>
#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>

class DirectoryCache {
public:
    static DirectoryCache& instance() {
        static DirectoryCache instance;
        return instance;
    }

    QFileInfoList getContents(const QString& path) {
        QMutexLocker locker(&mutex);
        if(cache.contains(path)) {
            QPair<QFileInfoList, QDateTime>* cached = cache[path];
            if(cached && QDateTime::currentDateTime().secsTo(cached->second) < 5) {
                return cached->first;
            }
        }
        return QFileInfoList();
    }

    void updateCache(const QString& path, const QFileInfoList& contents) {
        QMutexLocker locker(&mutex);
        cache.insert(path, new QPair<QFileInfoList, QDateTime>(contents, QDateTime::currentDateTime()));
    }

private:
    QCache<QString, QPair<QFileInfoList, QDateTime>> cache;
    QMutex mutex;
    DirectoryCache() : cache(1000) {} // Caché para 1000 directorios
};
