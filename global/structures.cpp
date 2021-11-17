#include "structures.h"
#include "control.h"
#include "network.h"

CUrlHolder::CUrlHolder()
{
    CUrlHolder::title=QString();
    CUrlHolder::url=QUrl();
    CUrlHolder::uuid=QUuid::createUuid();
}

CUrlHolder::CUrlHolder(const CUrlHolder &other)
{
    title=other.title;
    url=other.url;
    uuid=other.uuid;
}

CUrlHolder::CUrlHolder(const QString& title, const QUrl& url)
{
    CUrlHolder::title=title;
    CUrlHolder::url=url;
    CUrlHolder::uuid=QUuid::createUuid();
}

bool CUrlHolder::operator==(const CUrlHolder &s) const
{
    return (s.url==url);
}

bool CUrlHolder::operator!=(const CUrlHolder &s) const
{
    return !operator==(s);
}

QDataStream &operator<<(QDataStream &out, const CUrlHolder &obj) {
    out << obj.title << obj.url << obj.uuid;
    return out;
}

QDataStream &operator>>(QDataStream &in, CUrlHolder &obj) {
    in >> obj.title >> obj.url >> obj.uuid;
    return in;
}

QDataStream &operator<<(QDataStream &out, const QSslCertificate &obj)
{
    out << obj.toPem();
    return out;
}

QDataStream &operator>>(QDataStream &in, QSslCertificate &obj)
{
    QByteArray pem;
    in >> pem;
    QList<QSslCertificate> slist = QSslCertificate::fromData(pem,QSsl::Pem);
    if (!slist.isEmpty())
        obj = slist.first();
    return in;
}

CLangPair::CLangPair()
{
    nullify();
}

CLangPair::CLangPair(const CLangPair &other)
{
    langFrom = other.langFrom;
    langTo = other.langTo;
}

CLangPair::CLangPair(const QString &hash)
{
    QStringList sl = hash.split(u'#');
    if (sl.count()!=2) {
        nullify();
        return;
    }
    langFrom = QLocale(sl.first());
    langTo = QLocale(sl.last());
}

CLangPair::CLangPair(const QString &fromName, const QString &toName)
{
    langFrom = QLocale(fromName);
    langTo = QLocale(toName);
}

bool CLangPair::isValid() const
{
    return ((langFrom != langTo)
            && (langFrom != QLocale::c())
            && (langTo != QLocale::c()));
}

bool CLangPair::isAtlasAcceptable() const
{
    static const QStringList validLangs({ QSL("en"), QSL("ja") });
    return (isValid() &&
            validLangs.contains(langFrom.bcp47Name()) &&
            validLangs.contains(langTo.bcp47Name()));
}

QString CLangPair::getHash() const
{
    return QSL("%1#%2").arg(langFrom.bcp47Name(),
                            langTo.bcp47Name());
}

QString CLangPair::toString() const
{
    return QSL("%1 -> %2").arg(QLocale::languageToString(langFrom.language()),
                               QLocale::languageToString(langTo.language()));
}

QString CLangPair::toShortString() const
{
    const QChar arrow(0x2192);
    return QSL("%1 %2 %3")
            .arg(langFrom.bcp47Name(),
                 arrow,
                 langTo.bcp47Name());
}

QString CLangPair::toLongString() const
{
    const QChar arrow(0x2192);
    return QSL("%1 %2 %3")
            .arg(gSet->net()->getLanguageName(langFrom.bcp47Name()),
                 arrow,
                 gSet->net()->getLanguageName(langTo.bcp47Name()));
}

bool CLangPair::operator==(const CLangPair &s) const
{
    return ((langFrom==s.langFrom) && (langTo==s.langTo));
}

bool CLangPair::operator!=(const CLangPair &s) const
{
    return (!operator==(s));

}

void CLangPair::nullify()
{
    langFrom = QLocale::system();
    langTo = QLocale::system();
}

QDataStream &operator<<(QDataStream &out, const CLangPair &obj)
{
    out << obj.getHash();
    return out;
}

QDataStream &operator>>(QDataStream &in, CLangPair &obj)
{
    QString hash;
    in >> hash;
    obj = CLangPair(hash);
    return in;
}

const QMap<CStructures::TranslationEngine, QString> &CStructures::translationEngines()
{
    static const QMap<CStructures::TranslationEngine,QString> engines = {
        { teAtlas, QSL("ATLAS") },
        { teBingAPI, QSL("Bing API") },
        { teYandexAPI, QSL("Yandex API") },
        { teGoogleGTX, QSL("Google GTX") },
        { teAmazonAWS, QSL("Amazon Translate") },
        { teYandexCloud, QSL("Yandex Cloud") },
        { teGoogleCloud, QSL("Google Cloud") },
        { teAliCloud, QSL("Alibaba Cloud") },
        { teDeeplFree, QSL("DeepL Free") }
    };

    return engines;
}

const QMap<CStructures::TranslationEngine, QString> &CStructures::translationEngineCodes()
{
    static const QMap<CStructures::TranslationEngine,QString> engines = {
        { teAtlas, QSL("atlas") },
        { teBingAPI, QSL("bing") },
        { teYandexAPI, QSL("yandex") },
        { teGoogleGTX, QSL("google") },
        { teAmazonAWS, QSL("aws") },
        { teYandexCloud, QSL("yandex-cloud") },
        { teGoogleCloud, QSL("gcp") },
        { teAliCloud, QSL("alicloud") },
        { teDeeplFree, QSL("deepl-free") }
    };

    return engines;
}

QDataStream &operator<<(QDataStream &out, const CStructures::TranslationEngine &obj)
{
    out << static_cast<int>(obj);
    return out;
}

QDataStream &operator>>(QDataStream &in, CStructures::TranslationEngine &obj)
{
    int buf = 0;
    in >> buf;
    obj = static_cast<CStructures::TranslationEngine>(buf);
    return in;
}

const QStringList &CStructures::incompatibleHttp2Urls()
{
    static const QStringList urls({ QSL("pximg.net"), QSL("pixiv.net"), QSL("yandex.com") });
    return urls;
}
