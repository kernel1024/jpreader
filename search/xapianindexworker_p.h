#ifndef XAPIANINDEXWORKER_P_H
#define XAPIANINDEXWORKER_P_H

#ifdef WITH_XAPIAN
#include <xapian.h>
#endif

#include <QObject>
#include <QScopedPointer>
#include <QDir>

class CXapianIndexWorkerPrivate : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(CXapianIndexWorkerPrivate)
public:
#ifdef WITH_XAPIAN
    QScopedPointer<Xapian::WritableDatabase> m_db;
#endif
    QStringList m_indexDirs;
    QDir m_cacheDir;
    std::string m_stemLang;
    bool m_cleanupDatabase { false };
    bool m_fastIncrementalIndex { false };

    explicit CXapianIndexWorkerPrivate(QObject *parent = nullptr);
    ~CXapianIndexWorkerPrivate() override = default;
};

#endif // XAPIANINDEXWORKER_P_H
