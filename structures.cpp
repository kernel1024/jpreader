#include "structures.h"

UrlHolder::UrlHolder()
{
    UrlHolder::title="";
    UrlHolder::url=QUrl();
    UrlHolder::uuid=QUuid::createUuid();
}

UrlHolder::UrlHolder(const UrlHolder &other)
{
    title=other.title;
    url=other.url;
    uuid=other.uuid;
}

UrlHolder::UrlHolder(QString title, QUrl url)
{
    UrlHolder::title=title;
    UrlHolder::url=url;
    UrlHolder::uuid=QUuid::createUuid();
}

UrlHolder& UrlHolder::operator=(const UrlHolder& other)
{
    title=other.title;
    url=other.url;
    uuid=other.uuid;
    return *this;
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
    DirStruct::dirName="";
    DirStruct::count=-1;
}

DirStruct::DirStruct(const DirStruct &other)
{
    dirName=other.dirName;
    count=other.count;
}

DirStruct::DirStruct(QString DirName, int Count)
{
    DirStruct::dirName=DirName;
    DirStruct::count=Count;
}

DirStruct& DirStruct::operator=(const DirStruct& other)
{
    dirName=other.dirName;
    count=other.count;
    return *this;
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
