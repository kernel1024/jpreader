#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <algorithm>
#include <execution>
#include "translatorcache.h"
#include "utils/genericfuncs.h"
#include "global/globalcontrol.h"
#include "translatorcachedialog.h"

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

QString CTranslatorCache::cachedTranslatorResult(const QString &source,
                                                 const CLangPair &languagePair,
                                                 CStructures::TranslationEngine engine,
                                                 bool translateSubSentences) const
{
    if (!m_cachePath.exists()) return QString();

    QString filename = getMD5(getHashSource(source,languagePair,engine,translateSubSentences));

    QFile f(m_cachePath.filePath(filename));
    if (!f.open(QIODevice::ReadOnly)) return QString();

    QString result = QString::fromUtf8(f.readAll());
    f.close();

    return result;
}

QString CTranslatorCache::cachedTranslatorResult(const QString &md5) const
{
    if (!m_cachePath.exists()) return QString();

    QFile f(m_cachePath.filePath(md5));
    if (!f.open(QIODevice::ReadOnly)) return QString();

    QString result = QString::fromUtf8(f.readAll());
    f.close();

    return result;
}

void CTranslatorCache::saveTranslatorResult(const QString &source, const QString &result,
                                            const CLangPair &languagePair,
                                            CStructures::TranslationEngine engine,
                                            bool translateSubSentences,
                                            const QString &title,
                                            const QUrl &origin)
{
    if (!m_cachePath.exists()) return;

    QString filename = getMD5(getHashSource(source,languagePair,engine,translateSubSentences));

    QFile f(m_cachePath.filePath(filename));
    if (!f.open(QIODevice::WriteOnly)) return;
    f.write(result.toUtf8());
    f.close();

    QJsonObject root;
    root.insert(QSL("title"),title);
    root.insert(QSL("engine"),CStructures::translationEngines().value(engine));
    root.insert(QSL("langpair"),languagePair.toString());
    root.insert(QSL("length"),source.length());
    if (!(origin.toString().startsWith(QSL("data:"),Qt::CaseInsensitive)))
        root.insert(QSL("origin"),origin.toString());
    QJsonDocument doc(root);

    QFile info(m_cachePath.filePath(QSL("%1.info").arg(filename)));
    if (!info.open(QIODevice::WriteOnly)) return;
    info.write(doc.toJson());
    info.close();

    cleanOldEntries();
}

QDir CTranslatorCache::getCachePath() const
{
    return m_cachePath;
}

QString CTranslatorCache::getMD5(const QString &content) const
{
    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(content.toUtf8());
    return QString::fromLatin1(md5.result().toHex());
}

QString CTranslatorCache::getHashSource(const QString &source,
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

    std::for_each(std::execution::par,list.constBegin(),list.constEnd(),
                   [](const QFileInfo& item){
        QFile f(item.absoluteFilePath());
        f.remove();
    });
}

void CTranslatorCache::clearCache()
{
    if (!m_cachePath.exists()) return;
    const QFileInfoList list = m_cachePath.entryInfoList(QDir::Files | QDir::Writable | QDir::Readable);
    std::for_each(std::execution::par,list.constBegin(),list.constEnd(),
                   [](const QFileInfo& item){
        QFile f(item.absoluteFilePath());
        f.remove();
    });
}

void CTranslatorCache::showDialog()
{
    CTranslatorCacheDialog dlg(gSet->activeWindow());
    dlg.exec();
}
