#ifndef TRANSLATOR_H
#define TRANSLATOR_H

#include <QObject>
#include <QString>
#include <QColor>
#include <QFont>
#include <QUuid>
#include <QScopedPointer>
#include <QAtomicInteger>
#include <netdb.h>
#include "translator-workers/abstracttranslator.h"
#include "global/structures.h"
#include "utils/htmlparser.h"
#include "abstractthreadworker.h"

class CBrowserWaitCtl;

class CTranslator : public CAbstractThreadWorker
{
    Q_OBJECT
    Q_DISABLE_COPY(CTranslator)
private:
    enum XMLPassMode {
        PXPreprocess,
        PXCalculate,
        PXTranslate,
        PXPostprocess
    };

    int m_retryCount { 0 };
    int m_textNodesCnt { 0 };
    int m_textNodesProgress { 0 };
    bool m_translatorFailed { false };
    bool m_tranInited { false };
    bool m_useOverrideTransFont { false };
    bool m_forceFontColor { false };
    bool m_translateSubSentences { false };
    CStructures::TranslationEngine m_translationEngine { CStructures::teAtlas };
    CStructures::TranslationMode m_translationMode { CStructures::tmAdditive };

    QScopedPointer<CAbstractTranslator,QScopedPointerDeleteLater> m_tran;
    QString m_sourceHtml;
    QString m_engineName;
    QColor m_forcedFontColor;
    QFont m_overrideTransFont;
    QUrl m_metaSrcUrl;
    QStringList m_imgUrls;
    QStringList m_allAnchorUrls;
    QString m_title;
    QUrl m_origin;
    CLangPair m_langPair;

    bool translateDocument(const QString& srcHtml, QString& dstHtml);

    void examineNode(CHTMLNode & node, XMLPassMode xmlPass);
    bool translateParagraph(CHTMLNode & src, XMLPassMode xmlPass);

    void dumpPage(QUuid token, const QString& suffix, const QString& page);
    void dumpPage(QUuid token, const QString& suffix, const CHTMLNode& page);
    void dumpPage(QUuid token, const QString& suffix, const QByteArray& page);

public:
    CTranslator(QObject* parent, const QString &sourceHtml,
                const QString &title = QString(), const QUrl &origin = QUrl(),
                CStructures::TranslationEngine engine = CStructures::teAtlas,
                const CLangPair &langPair = CLangPair());
    ~CTranslator() override = default;
    bool documentReparse(const QString& sourceHtml, QString& destHtml);
    QStringList getImgUrls() const;
    QStringList getAnchorUrls() const;
    QString workerDescription() const override;

protected:
    void startMain() override;

Q_SIGNALS:
    void translationFinished(bool success, bool aborted, const QString &resultHtml, const QString &error);
    void setProgress(int value);

private Q_SLOTS:
    void addTranslatorRequestBytes(qint64 size);

};

#endif // TRANSLATOR_H
