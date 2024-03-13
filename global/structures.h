#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <QObject>
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

namespace CStructures {
Q_NAMESPACE

enum TranslationEngine {
    teAtlas = 1,
    teBingAPI = 2,
    teYandexAPI = 3,
    teGoogleGTX = 4,
    teAmazonAWS = 5,
    teYandexCloud = 6,
    teGoogleCloud = 7,
    teAliCloud = 8,
    teDeeplFree = 9,
    tePromtOneFree = 10,
    tePromtNmtAPI = 11,
    teDeeplAPI = 12,
    teOpenAI = 13
};
Q_ENUM_NS(TranslationEngine)

enum SearchEngine {
    seNone = 0,
    seRecoll = 2,
    seBaloo = 3,
    seXapian = 4
};
Q_ENUM_NS(SearchEngine)

enum TranslationMode {
    tmAdditive = 0,
    tmOverwriting = 1,
    tmTooltip = 2
};
Q_ENUM_NS(TranslationMode)

enum SubsentencesMode {
    smSplitByPunctuation = 0,
    smKeepParagraph = 1,
    smCombineToMaxTokens = 2
};
Q_ENUM_NS(SubsentencesMode)

enum SearchModelRole {
    cpSortRole = 1,
    cpFilterRole = 2
};
Q_ENUM_NS(SearchModelRole)

enum DateRange {
    drWeek = 1,
    drMonth = 2,
    drYear = 3,
    drAll = 4,
    drThisMonth = 5
};
Q_ENUM_NS(DateRange)

enum PixivIndexSortOrder {
    psTitle = 0,
    psSize = 1,
    psDate = 2,
    psAuthor = 3,
    psSeries = 4,
    psDescription = 5,
    psBookmarkCount = 6
};
Q_ENUM_NS(PixivIndexSortOrder)

enum AliCloudTranslatorMode {
    aliTranslatorGeneral = 0,
    aliTranslatorECTitle = 1,
    aliTranslatorECDescription = 2,
    aliTranslatorECCommunication = 3,
    aliTranslatorECMedical = 4,
    aliTranslatorECSocial = 5
};
Q_ENUM_NS(AliCloudTranslatorMode)

enum DeeplAPIMode {
    deeplAPIModeFree = 0,
    deeplAPIModePro = 1
};
Q_ENUM_NS(DeeplAPIMode)

enum DeeplAPISplitSentences {
    deeplAPISplitDefault = 0,
    deeplAPISplitNone = 1,
    deeplAPISplitFull = 2,
    deeplAPISplitNoNewlines = 3
};
Q_ENUM_NS(DeeplAPISplitSentences)

enum DeeplAPIFormality {
    deeplAPIFormalityDefault = 0,
    deeplAPIFormalityMore = 1,
    deeplAPIFormalityLess = 2
};
Q_ENUM_NS(DeeplAPIFormality)

enum PixivFetchCoversMode {
    pxfmNone = 0,
    pxfmLazyFetch = 1,
    pxfmFetchAll = 2
};
Q_ENUM_NS(PixivFetchCoversMode)

enum PixivMangaPageSize {
    pxmpSmall = 0,
    pxmpRegular = 1,
    pxmpOriginal = 2
};
Q_ENUM_NS(PixivMangaPageSize)

const QMap<CStructures::TranslationEngine, QString> &translationEngines();
const QMap<CStructures::TranslationEngine, QString> &translationEngineCodes();
const QStringList &incompatibleHttp2Urls();

}

namespace CDefaults {
const auto DBusName = "org.kernel1024.jpreader";
const auto IPCName = "org.kernel1024.jpreader.ipc.main";
const int maxTitleElideLength = 100;
}

class CUrlHolder {
    friend QDataStream &operator<<(QDataStream &out, const CUrlHolder &obj);
    friend QDataStream &operator>>(QDataStream &in, CUrlHolder &obj);
public:
    QString title;
    QUrl url;
    QUuid uuid;

    CUrlHolder();
    CUrlHolder(const CUrlHolder& other) = default;
    CUrlHolder(const QString &aTitle, const QUrl &aUrl);
    ~CUrlHolder() = default;
    CUrlHolder &operator=(const CUrlHolder& other) = default;
    bool operator==(const CUrlHolder &s) const;
    bool operator!=(const CUrlHolder &s) const;
};

Q_DECLARE_METATYPE(CUrlHolder)

class CLangPair {
    friend QDataStream &operator<<(QDataStream &out, const CLangPair &obj);
    friend QDataStream &operator>>(QDataStream &in, CLangPair &obj);
public:
    QLocale langFrom;
    QLocale langTo;

    CLangPair();
    explicit CLangPair(const QString& hash);
    CLangPair(const CLangPair& other) = default;
    CLangPair(const QString& fromName, const QString &toName);
    ~CLangPair() = default;
    CLangPair &operator=(const CLangPair& other) = default;
    bool isValid() const;
    bool isAtlasAcceptable() const;
    QString getHash() const;
    QString toString() const;
    QString toShortString() const;
    QString toLongString() const;
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
using CSubsentencesMode = QHash<CStructures::TranslationEngine,CStructures::SubsentencesMode>;
using CTranslatorStatistics = QHash<CStructures::TranslationEngine,QMap<QDate,quint64> >;
using CSelectedLangPairs = QHash<CStructures::TranslationEngine,QString>;
using CUrlWithName = QPair<QString,QString>;

#define QSL QStringLiteral //NOLINT

QDataStream &operator<<(QDataStream &out, const QSslCertificate &obj);
QDataStream &operator>>(QDataStream &in, QSslCertificate &obj);

QDataStream &operator<<(QDataStream &out, const CLangPair &obj);
QDataStream &operator>>(QDataStream &in, CLangPair &obj);

QDataStream &operator<<(QDataStream &out, const CStructures::TranslationEngine &obj);
QDataStream &operator>>(QDataStream &in, CStructures::TranslationEngine &obj);

#endif // STRUCTURES_H
