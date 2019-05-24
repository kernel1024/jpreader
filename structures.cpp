#include "structures.h"

QString CUrlHolder::getTitle() const
{
    return title;
}

void CUrlHolder::setTitle(const QString &value)
{
    title = value;
}

QUrl CUrlHolder::getUrl() const
{
    return url;
}

void CUrlHolder::setUrl(const QUrl &value)
{
    url = value;
}

QUuid CUrlHolder::getUuid() const
{
    return uuid;
}

void CUrlHolder::setUuid(const QUuid &value)
{
    uuid = value;
}

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

/*CDirStruct::CDirStruct(const CDirStruct &other)
{
    dirName=other.dirName;
    count=other.count;
}

CDirStruct::CDirStruct(const QString& DirName, int Count)
{
    CDirStruct::dirName=DirName;
    CDirStruct::count=Count;
}*/

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
    QStringList sl = hash.split('#');
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
    return (langFrom!=langTo);
}

bool CLangPair::isAtlasAcceptable() const
{
    static const QStringList validLangs({"en", "ja"});
    return (isValid() &&
            validLangs.contains(langFrom.bcp47Name()) &&
            validLangs.contains(langTo.bcp47Name()));
}

QString CLangPair::getHash() const
{
    return QStringLiteral("%1#%2").arg(langFrom.bcp47Name(),
                                langTo.bcp47Name());
}

bool CLangPair::operator==(const CLangPair &s) const
{
    return ((langFrom==s.langFrom) && (langTo==s.langTo));
}

bool CLangPair::operator!=(const CLangPair &s) const
{
    return (!operator==(s));

}

QLocale CLangPair::getLangFrom() const
{
    return langFrom;
}

void CLangPair::setLangFrom(const QLocale &value)
{
    langFrom = value;
}

QLocale CLangPair::getLangTo() const
{
    return langTo;
}

void CLangPair::setLangTo(const QLocale &value)
{
    langTo = value;
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
