#ifndef TRANSLATOR_H
#define TRANSLATOR_H

#include <QObject>
#include <QString>
#include <QMutex>
#include <QColor>
#include <QFont>
#include <QUuid>
#include <netdb.h>
#include "html/ParserDom.h"
#include "mainwindow.h"
#include "abstracttranslator.h"
#include "globalcontrol.h"
#include "snwaitctl.h"
#include "structures.h"

class CHTMLNode {
public:
    QString text, tagName, closingText;
    QVector<CHTMLNode> children;
    QHash<QString,QString> attributes;
    QStringList attributesOrder;
    bool isTag, isComment;
    CHTMLNode();
    ~CHTMLNode();
    CHTMLNode(const CHTMLNode& other);
    explicit CHTMLNode(tree<htmlcxx::HTML::Node> const & node);
    explicit CHTMLNode(const QString& innerText);
    CHTMLNode &operator=(const CHTMLNode& other);
    bool operator==(const CHTMLNode &s) const;
    bool operator!=(const CHTMLNode &s) const;
    void normalize();
    bool isTextNode() const;
};

Q_DECLARE_METATYPE(CHTMLNode)

class CSnWaitCtl;

class CTranslator : public QObject
{
    Q_OBJECT
private:
    enum XMLPassMode {
        PXPreprocess,
        PXCalculate,
        PXTranslate,
        PXPostprocess
    };

    QString hostingDir;
    QString hostingUrl;
    QString Uri;
    QString scpParams;
    QString scpHost;
    QString translatorError;
    QMutex abortMutex;
    CAbstractTranslator* tran { nullptr };
    QColor forcedFontColor;
    QFont overrideFont;
    QUrl metaSrcUrl;
    QStringList imgUrls;
    int atlTcpRetryCount { 0 };
    int atlTcpTimeout { 0 };
    CStructures::TranslationEngine translationEngine { CStructures::teAtlas };
    int textNodesCnt { 0 };
    int textNodesProgress { 0 };
    CStructures::TranslationMode translationMode { CStructures::tmAdditive };
    bool localHosting { false };
    bool useSCP { false };
    bool abortFlag { false };
    bool translatorFailed { false };
    bool tranInited { false };
    bool useOverrideFont { false };
    bool forceFontColor { false };
    bool translateSubSentences { false };

    Q_DISABLE_COPY(CTranslator)

    bool calcLocalUrl(const QString& aUri, QString& calculatedUrl);
    bool translateDocument(const QString& srcUri, QString& dst);

    void examineNode(CHTMLNode & node, XMLPassMode xmlPass);
    bool translateParagraph(CHTMLNode & src, XMLPassMode xmlPass);

    void dumpPage(QUuid token, const QString& suffix, const QString& page);
    void dumpPage(QUuid token, const QString& suffix, const CHTMLNode& page);
    void dumpPage(QUuid token, const QString& suffix, const QByteArray& page);

public:
    explicit CTranslator(QObject* parent, const QString &aUri, bool forceTranSubSentences = false);
    ~CTranslator() override;
    bool documentReparse(const QString& srcUri, QString& dst);
    QStringList getImgUrls() const;
    static void generateHTML(const CHTMLNode &src, QString &html, bool reformat = false,
                             int depth = 0);
    static void replaceLocalHrefs(CHTMLNode &node, const QUrl &baseUrl);

Q_SIGNALS:
    void calcFinished(bool success, const QString &aUrl, const QString &error);
    void setProgress(int value);
    void finished();

public Q_SLOTS:
    void abortTranslator();
    void translate();

};

#endif // TRANSLATOR_H
