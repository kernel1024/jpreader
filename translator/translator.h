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
#include "html/ParserDom.h"
#include "mainwindow.h"
#include "translator-workers/abstracttranslator.h"
#include "global/globalcontrol.h"
#include "browser/waitctl.h"
#include "global/structures.h"
#include "abstractthreadworker.h"

using CHTMLAttributesHash = QHash<QString,QString>;

class CHTMLNode
{
public:
    QString text, tagName, closingText;
    QVector<CHTMLNode> children;
    CHTMLAttributesHash attributes;
    QStringList attributesOrder;
    bool isTag { false };
    bool isComment { false };
    CHTMLNode() = default;
    ~CHTMLNode() = default;
    CHTMLNode(const CHTMLNode& other);
    explicit CHTMLNode(tree<htmlcxx::HTML::Node> const & node);
    explicit CHTMLNode(const QString& innerText);
    CHTMLNode &operator=(const CHTMLNode& other) = default;
    bool operator==(const CHTMLNode &s) const;
    bool operator!=(const CHTMLNode &s) const;
    void normalize();
    bool isTextNode() const;
};

Q_DECLARE_METATYPE(CHTMLNode)

class CBrowserWaitCtl;

class CTranslator : public CAbstractThreadWorker
{
    Q_OBJECT
private:
    enum XMLPassMode {
        PXPreprocess,
        PXCalculate,
        PXTranslate,
        PXPostprocess
    };

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

    Q_DISABLE_COPY(CTranslator)

    bool translateDocument(const QString& srcHtml, QString& dstHtml);

    void examineNode(CHTMLNode & node, XMLPassMode xmlPass);
    bool translateParagraph(CHTMLNode & src, XMLPassMode xmlPass);

    void dumpPage(QUuid token, const QString& suffix, const QString& page);
    void dumpPage(QUuid token, const QString& suffix, const CHTMLNode& page);
    void dumpPage(QUuid token, const QString& suffix, const QByteArray& page);

public:
    explicit CTranslator(QObject* parent, const QString &sourceHtml,
                         const QString &title = QString(),
                         const QUrl &origin = QUrl());
    ~CTranslator() override = default;
    bool documentReparse(const QString& sourceHtml, QString& destHtml);
    QStringList getImgUrls() const;
    QStringList getAnchorUrls() const;
    QString workerDescription() const override;
    static void generateHTML(const CHTMLNode &src, QString &html, bool reformat = false,
                             int depth = 0);
    static void replaceLocalHrefs(CHTMLNode &node, const QUrl &baseUrl);

protected:
    void startMain() override;

Q_SIGNALS:
    void translationFinished(bool success, bool aborted, const QString &resultHtml, const QString &error);
    void setProgress(int value);

private Q_SLOTS:
    void addTranslatorRequestBytes(qint64 size);

};

#endif // TRANSLATOR_H
