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

class CHTMLNode {
public:
    QString text, tagName, closingText;
    QList<CHTMLNode> children;
    QHash<QString,QString> attributes;
    QStringList attributesOrder;
    bool isTag, isComment;
    bool dummy[2];
    CHTMLNode();
    ~CHTMLNode();
    CHTMLNode(const CHTMLNode& other);
    CHTMLNode(tree<htmlcxx::HTML::Node> const & node);
    CHTMLNode(const QString& innerText);
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
    CAbstractTranslator* tran;
    QColor forcedFontColor;
    QFont overrideFont;
    QString srcLanguage;
    QUrl metaSrcUrl;
    QStringList* createdFiles;
    int atlTcpRetryCount;
    int atlTcpTimeout;
    int translationEngine;
    int textNodesCnt;
    int textNodesProgress;
    int translationMode;
    int engine;
    bool localHosting;
    bool useSCP;
    bool abortFlag;
    bool translatorFailed;
    bool tranInited;
    bool useOverrideFont;
    bool forceFontColor;
    bool translateSubSentences;

    bool calcLocalUrl(const QString& aUri, QString& calculatedUrl);
    bool translateDocument(const QString& srcUri, QString& dst);

    void examineNode(CHTMLNode & node, XMLPassMode xmlPass);
    bool translateParagraph(CHTMLNode & src, XMLPassMode xmlPass);

    void dumpPage(const QUuid& token, const QString& suffix, const QString& page);
    void dumpPage(const QUuid& token, const QString& suffix, const CHTMLNode& page);
    void dumpPage(const QUuid& token, const QString& suffix, const QByteArray& page);

public:
    explicit CTranslator(QObject* parent, QString aUri, bool forceTranSubSentences = false);
    ~CTranslator();
    bool documentReparse(const QString& srcUri, QString& dst);

signals:
    void calcFinished(const bool success, const QString &aUrl);
    void setProgress(int value);

public slots:
    void abortTranslator();
    void translate();

};

#endif // TRANSLATOR_H
