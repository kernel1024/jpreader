#ifndef TRANSLATORCACHE_H
#define TRANSLATORCACHE_H

#include <QObject>
#include <QString>
#include <QDir>
#include "structures.h"

class CTranslatorCache : public QObject
{
    Q_OBJECT
public:
    explicit CTranslatorCache(QObject *parent = nullptr);
    void setCachePath(const QString& path);
    QDir getCachePath() const;
    QString cachedTranslatorResult(const QString& source, const CLangPair& languagePair,
                                   CStructures::TranslationEngine engine,
                                   bool translateSubSentences) const;
    QString cachedTranslatorResult(const QString& md5) const;
    void saveTranslatorResult(const QString& source, const QString& result,
                              const CLangPair& languagePair,
                              CStructures::TranslationEngine engine,
                              bool translateSubSentences,
                              const QString &title,
                              const QUrl &origin);


private:
    QDir m_cachePath;

    void cleanOldEntries();
    QString getMD5(const QString& content) const;
    QString getHashSource(const QString& source, const CLangPair& languagePair,
                                CStructures::TranslationEngine engine,
                                bool translateSubSentences) const;
public Q_SLOTS:
    void clearCache();
    void showDialog();

};

#endif // TRANSLATORCACHE_H
