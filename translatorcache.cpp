#include <QCryptographicHash>
#include <QDir>
#include "translatorcache.h"
#include "genericfuncs.h"
#include "globalcontrol.h"

CTranslatorCache::CTranslatorCache(QObject *parent) : QObject(parent)
{

}

void CTranslatorCache::setCachePath(const QString &path)
{
    if (path.isEmpty()) return;

    m_cachePath.setPath(path);
    if (!m_cachePath.exists())
        m_cachePath.mkpath(QSL("."));
}

const QString CTranslatorCache::cachedTranslatorResult(const QString &source,
                                                       const CLangPair &languagePair,
                                                       CStructures::TranslationEngine engine,
                                                       bool translateSubSentences)
{
    if (!m_cachePath.exists()) return QString();

    QString filename = getMD5(getHashSource(source,languagePair,engine,translateSubSentences));

    QFile f(m_cachePath.filePath(filename));
    if (!f.open(QIODevice::ReadOnly)) return QString();

    QString result = QString::fromUtf8(f.readAll());
    f.close();

    return result;
}

void CTranslatorCache::saveTranslatorResult(const QString &source, const QString &result,
                                            const CLangPair &languagePair,
                                            CStructures::TranslationEngine engine,
                                            bool translateSubSentences)
{
    if (!m_cachePath.exists()) return;

    QString filename = getMD5(getHashSource(source,languagePair,engine,translateSubSentences));

    QFile f(m_cachePath.filePath(filename));
    if (!f.open(QIODevice::WriteOnly)) return;

    f.write(result.toUtf8());
    f.close();

    cleanOldEntries();
}

const QString CTranslatorCache::getMD5(const QString &content) const
{
    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(content.toUtf8());
    return QString::fromLatin1(md5.result().toHex());
}

const QString CTranslatorCache::getHashSource(const QString &source,
                                              const CLangPair &languagePair,
                                              CStructures::TranslationEngine engine,
                                              bool translateSubSentences) const
{
    QString content = source;
    content.append(QSL("#%1#%2#%3").arg(languagePair.getHash(),CGenericFuncs::bool2str2(translateSubSentences))
                   .arg(static_cast<int>(engine)));
    return content;
}

void CTranslatorCache::cleanOldEntries()
{
    if (!m_cachePath.exists()) return;

    QFileInfoList list = m_cachePath.entryInfoList(QDir::Files | QDir::Writable | QDir::Readable,
                                                   QDir::Time | QDir::Reversed);
    qint64 sumSize = 0;
    qint64 maxSize = gSet->settings()->translatorCacheSize * CDefaults::oneMB;
    while (!list.isEmpty() && sumSize<maxSize) {
        sumSize += list.takeLast().size();
    }

    for (const auto &item : list) {
        QFile f(item.absoluteFilePath());
        f.remove();
    }
}

void CTranslatorCache::clearCache()
{
    if (!m_cachePath.exists()) return;
    QFileInfoList list = m_cachePath.entryInfoList(QDir::Files | QDir::Writable | QDir::Readable);
    for (const auto &item : list) {
        QFile f(item.absoluteFilePath());
        f.remove();
    }
}
