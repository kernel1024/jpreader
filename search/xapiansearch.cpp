#ifdef WITH_XAPIAN
#include <xapian.h>
#endif

#include <QStandardPaths>
#include <QScopeGuard>
#include <QScopedPointer>
#include "xapiansearch.h"
#include "global/control.h"

CXapianSearch::CXapianSearch(QObject *parent)
    : CAbstractThreadedSearch(parent)
{
    m_stemLang = gSet->settings()->xapianStemmerLang.toStdString();
    m_cacheDir.setPath(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
}

void CXapianSearch::doSearch(const QString &qr, int maxLimit)
{
#ifdef WITH_XAPIAN
    if (isWorking()) return;
    setWorking(true);

    if (!m_cacheDir.isReadable()) return;
    const QString dbFile = m_cacheDir.filePath(QSL("xapian_index"));

    QString errMsg;
    QScopedPointer<Xapian::Database> db_ro;

    auto dbCleanup = qScopeGuard([this,&errMsg,&db_ro]{
        if (!errMsg.isEmpty()) {
            errMsg = QSL("XapianSearch: Xapian query exception: %1").arg(errMsg);
            Q_EMIT errorOccured(errMsg);
            qCritical() << errMsg;
        }

        QString errMsgSG;
        try {
            if (db_ro)
                db_ro->close();

        } catch (const Xapian::Error &err) {
            errMsgSG = QString::fromStdString(err.get_msg());
        } catch (const std::string &s) {
            errMsgSG = QString::fromStdString(s);
        } catch (const char *s) {
            errMsgSG = QString::fromUtf8(s);
        }
        if (!errMsgSG.isEmpty()) {
            errMsgSG = QSL("XapianSearch: Xapian query database sync exception: %1").arg(errMsgSG);
            Q_EMIT errorOccured(errMsgSG);
            qCritical() << errMsgSG;
        }

        Q_EMIT finished();
    });

    try {
        Xapian::QueryParser qp;
        if (!m_stemLang.empty()) {
            qp.set_stemmer(Xapian::Stem(m_stemLang));
            qp.set_stemming_strategy(Xapian::QueryParser::STEM_SOME);
        }
        Xapian::Query query = qp.parse_query(qr.toStdString(),Xapian::QueryParser::FLAG_CJK_WORDS);

        db_ro.reset(new Xapian::Database(dbFile.toStdString(), Xapian::DB_OPEN));

        Xapian::Enquire enquire(*db_ro);
        enquire.set_query(query);
        const Xapian::MSet result = enquire.get_mset(0,maxLimit);
        for (auto it = result.begin(), end = result.end(); it != end; ++it) {
            Q_EMIT addHit({ { QSL("jp:fullfilename"), QString::fromStdString(it.get_document().get_data()) },
                            { QSL("relevancyrating"), QSL("%1%").arg(it.get_percent()) } });
        }

    } catch (const Xapian::Error &err) {
        errMsg = QString::fromStdString(err.get_msg());
    } catch (const std::string &s) {
        errMsg = QString::fromStdString(s);
    } catch (const char *s) {
        errMsg = QString::fromUtf8(s);
    }

#else
    Q_UNUSED(qr);
    Q_UNUSED(maxLimit);
    Q_EMIT finished();
#endif
}
