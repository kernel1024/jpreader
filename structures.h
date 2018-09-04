#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <QString>
#include <QDataStream>
#include <QUrl>
#include <QUuid>
#include <QMap>
#include <QHash>
#include <QPair>
#include <QList>
#include <QSslCertificate>
#include <QLocale>

#define TE_GOOGLE 0
#define TE_ATLAS 1
#define TE_BINGAPI 2
#define TE_YANDEX 3
#define TE_GOOGLE_GTX 4
#define TECOUNT 5

#define SE_NONE 0
#define SE_RECOLL 2
#define SE_BALOO5 3

#define TM_ADDITIVE 0
#define TM_OVERWRITING 1
#define TM_TOOLTIP 2

#define DBUS_NAME "org.kernel1024.jpreader"
#define IPC_NAME "org.kernel1024.jpreader.ipc.main"

class UrlHolder {
    friend QDataStream &operator<<(QDataStream &out, const UrlHolder &obj);
    friend QDataStream &operator>>(QDataStream &in, UrlHolder &obj);
public:
    QString title;
    QUrl url;
    QUuid uuid;
    UrlHolder();
    UrlHolder(const UrlHolder& other);
    UrlHolder(QString title, QUrl url);
    UrlHolder &operator=(const UrlHolder& other);
    bool operator==(const UrlHolder &s) const;
    bool operator!=(const UrlHolder &s) const;
};

Q_DECLARE_METATYPE(UrlHolder)

class DirStruct {
public:
    DirStruct();
    DirStruct(const DirStruct& other);
    DirStruct(QString DirName, int Count);
    DirStruct &operator=(const DirStruct& other);
    QString dirName;
    int count;
};

class CLangPair {
    friend QDataStream &operator<<(QDataStream &out, const CLangPair &obj);
    friend QDataStream &operator>>(QDataStream &in, CLangPair &obj);
public:
    QLocale langFrom, langTo;
    CLangPair();
    CLangPair(const CLangPair& other);
    CLangPair(const QString& hash);
    CLangPair(const QString& fromName, const QString &toName);
    CLangPair &operator=(const CLangPair& other);
    bool isValid() const;
    bool isAtlasAcceptable() const;
    QString getHash() const;
    bool operator==(const CLangPair &s) const;
    bool operator!=(const CLangPair &s) const;
private:
    void nullify();
};

Q_DECLARE_METATYPE(CLangPair)

typedef QHash<QString, QString> QStrHash;
typedef QList<int> QIntList;
typedef QList<UrlHolder> QUHList;
typedef QMap<QString, QUrl> QBookmarksMap;
typedef QList<QPair<QString, QUrl>> QBookmarks;
typedef QHash<QSslCertificate,QIntList> QSslCertificateHash;
typedef QList<CLangPair> CLangPairList;

QDataStream &operator<<(QDataStream &out, const QSslCertificate &obj);
QDataStream &operator>>(QDataStream &in, QSslCertificate &obj);

QDataStream &operator<<(QDataStream &out, const CLangPair &obj);
QDataStream &operator>>(QDataStream &in, CLangPair &obj);

#endif // STRUCTURES_H
