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

enum TranslationEngine {
    teGoogle = 0,
    teAtlas = 1,
    teBingAPI = 2,
    teYandexAPI = 3,
    teGoogleGTX = 4
};

static const QMap<TranslationEngine,QString> translationEngines = {
    { teGoogle, QStringLiteral("Google") },
    { teAtlas, QStringLiteral("ATLAS") },
    { teBingAPI, QStringLiteral("Bing API") },
    { teYandexAPI, QStringLiteral("Yandex API") },
    { teGoogleGTX, QStringLiteral("Google GTX") }
};

enum SearchEngine {
    seNone = 0,
    seRecoll = 2,
    seBaloo5 = 3
};

enum TranslationMode {
    tmAdditive = 0,
    tmOverwriting = 1,
    tmTooltip = 2
};

const char DBusName[] = "org.kernel1024.jpreader";
const char IPCName[] = "org.kernel1024.jpreader.ipc.main";

class CUrlHolder {
    friend QDataStream &operator<<(QDataStream &out, const CUrlHolder &obj);
    friend QDataStream &operator>>(QDataStream &in, CUrlHolder &obj);
public:
    QString title;
    QUrl url;
    QUuid uuid;
    CUrlHolder();
    CUrlHolder(const CUrlHolder& other);
    CUrlHolder(const QString &title, const QUrl &url);
    CUrlHolder &operator=(const CUrlHolder& other) = default;
    bool operator==(const CUrlHolder &s) const;
    bool operator!=(const CUrlHolder &s) const;
};

Q_DECLARE_METATYPE(CUrlHolder)

class CLangPair {
    friend QDataStream &operator<<(QDataStream &out, const CLangPair &obj);
    friend QDataStream &operator>>(QDataStream &in, CLangPair &obj);
public:
    QLocale langFrom, langTo;
    CLangPair();
    CLangPair(const CLangPair& other);
    CLangPair(const QString& hash);
    CLangPair(const QString& fromName, const QString &toName);
    CLangPair &operator=(const CLangPair& other) = default;
    bool isValid() const;
    bool isAtlasAcceptable() const;
    QString getHash() const;
    bool operator==(const CLangPair &s) const;
    bool operator!=(const CLangPair &s) const;
private:
    void nullify();
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
