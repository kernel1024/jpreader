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

class CUrlHolder {
    friend QDataStream &operator<<(QDataStream &out, const CUrlHolder &obj);
    friend QDataStream &operator>>(QDataStream &in, CUrlHolder &obj);

private:
    QString title;
    QUrl url;
    QUuid uuid;

public:
    CUrlHolder();
    CUrlHolder(const CUrlHolder& other);
    CUrlHolder(CUrlHolder&& other) = default;
    CUrlHolder(const QString &title, const QUrl &url);
    CUrlHolder &operator=(const CUrlHolder& other) = default;
    CUrlHolder &operator=(CUrlHolder&& other) = default;
    ~CUrlHolder() = default;
    bool operator==(const CUrlHolder &s) const;
    bool operator!=(const CUrlHolder &s) const;

    QString getTitle() const;
    void setTitle(const QString &value);
    QUrl getUrl() const;
    void setUrl(const QUrl &value);
    QUuid getUuid() const;
    void setUuid(const QUuid &value);
};

Q_DECLARE_METATYPE(CUrlHolder)

/*class CDirStruct {
private:
    QString dirName;
    int count {-1};

public:
    CDirStruct() = default;
    CDirStruct(const CDirStruct& other);
    CDirStruct(CDirStruct&& other) = default;
    CDirStruct(const QString &DirName, int Count);
    ~CDirStruct() = default;
    CDirStruct &operator=(const CDirStruct& other) = default;
    CDirStruct &operator=(CDirStruct&& other) = default;
};*/

class CLangPair {
    friend QDataStream &operator<<(QDataStream &out, const CLangPair &obj);
    friend QDataStream &operator>>(QDataStream &in, CLangPair &obj);

private:
    QLocale langFrom, langTo;
    void nullify();

public:
    CLangPair();
    CLangPair(const CLangPair& other);
    CLangPair(CLangPair&& other) = default;
    CLangPair(const QString& hash);
    CLangPair(const QString& fromName, const QString &toName);
    ~CLangPair() = default;
    CLangPair &operator=(const CLangPair& other) = default;
    CLangPair &operator=(CLangPair&& other) = default;
    bool isValid() const;
    bool isAtlasAcceptable() const;
    QString getHash() const;
    bool operator==(const CLangPair &s) const;
    bool operator!=(const CLangPair &s) const;
    QLocale getLangFrom() const;
    void setLangFrom(const QLocale &value);
    QLocale getLangTo() const;
    void setLangTo(const QLocale &value);
};

Q_DECLARE_METATYPE(CLangPair)

using CStringHash = QHash<QString, QString>;
using CIntList = QList<int>;
using CUrlHolderVector = QVector<CUrlHolder>;
using CSslCertificateHash = QHash<QSslCertificate,CIntList>;
using CLangPairVector = QVector<CLangPair>;
using CStringSet = QSet<QString>;

QDataStream &operator<<(QDataStream &out, const QSslCertificate &obj);
QDataStream &operator>>(QDataStream &in, QSslCertificate &obj);

QDataStream &operator<<(QDataStream &out, const CLangPair &obj);
QDataStream &operator>>(QDataStream &in, CLangPair &obj);

#endif // STRUCTURES_H
