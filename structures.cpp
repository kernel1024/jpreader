#include "structures.h"

UrlHolder::UrlHolder()
{
    UrlHolder::title=QString();
    UrlHolder::url=QUrl();
    UrlHolder::uuid=QUuid::createUuid();
}

UrlHolder::UrlHolder(const UrlHolder &other)
{
    title=other.title;
    url=other.url;
    uuid=other.uuid;
}

UrlHolder::UrlHolder(const QString& title, const QUrl& url)
{
    UrlHolder::title=title;
    UrlHolder::url=url;
    UrlHolder::uuid=QUuid::createUuid();
}

bool UrlHolder::operator==(const UrlHolder &s) const
{
    return (s.url==url);
}

bool UrlHolder::operator!=(const UrlHolder &s) const
{
    return !operator==(s);
}

DirStruct::DirStruct()
{
    DirStruct::dirName=QString();
    DirStruct::count=-1;
}

DirStruct::DirStruct(const DirStruct &other)
{
    dirName=other.dirName;
    count=other.count;
}

DirStruct::DirStruct(const QString& DirName, int Count)
{
    DirStruct::dirName=DirName;
    DirStruct::count=Count;
}

QDataStream &operator<<(QDataStream &out, const UrlHolder &obj) {
    out << obj.title << obj.url << obj.uuid;
    return out;
}

QDataStream &operator>>(QDataStream &in, UrlHolder &obj) {
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
    QStringList validLangs({"en", "ja"});
    return (isValid() &&
            validLangs.contains(langFrom.bcp47Name()) &&
            validLangs.contains(langTo.bcp47Name()));
}

QString CLangPair::getHash() const
{
    return QString("%1#%2").arg(langFrom.bcp47Name(),
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
