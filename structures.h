#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <QString>
#include <QDataStream>
#include <QUrl>
#include <QUuid>
#include <QMap>
#include <QHash>
#include <QList>
#include <QSslCertificate>

#define TE_GOOGLE 0
#define TE_ATLAS 1
#define TE_BINGAPI 2
#define TE_YANDEX 3
#define TECOUNT 4

#define SE_NONE 0
#define SE_RECOLL 2
#define SE_BALOO5 3

#define TM_ADDITIVE 0
#define TM_OVERWRITING 1
#define TM_TOOLTIP 2

#define LS_JAPANESE 0
#define LS_CHINESETRAD 1
#define LS_CHINESESIMP 2
#define LS_KOREAN 3
#define LSCOUNT 4

#define DBUS_NAME "org.jpreader.auxtranslator"
#define IPC_NAME "org.jpreader.ipc.main"

class UrlHolder {
    friend QDataStream &operator<<(QDataStream &out, const UrlHolder &obj);
    friend QDataStream &operator>>(QDataStream &in, UrlHolder &obj);
public:
    QString title;
    QUrl url;
    QUuid uuid;
    UrlHolder();
    UrlHolder(QString title, QUrl url);
    UrlHolder &operator=(const UrlHolder& other);
    bool operator==(const UrlHolder &s) const;
    bool operator!=(const UrlHolder &s) const;
};

Q_DECLARE_METATYPE(UrlHolder)

class DirStruct {
public:
    DirStruct();
    DirStruct(QString DirName, int Count);
    DirStruct &operator=(const DirStruct& other);
    QString dirName;
    int count;
};

typedef QHash<QString, QString> QStrHash;
typedef QList<int> QIntList;
typedef QList<UrlHolder> QUHList;
typedef QMap<QString, QUrl> QBookmarksMap;
typedef QHash<QSslCertificate,QIntList> QSslCertificateHash;

QDataStream &operator<<(QDataStream &out, const QSslCertificate &obj);
QDataStream &operator>>(QDataStream &in, QSslCertificate &obj);

#endif // STRUCTURES_H
