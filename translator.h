#ifndef TRANSLATOR_H
#define TRANSLATOR_H

#include <QObject>
#include <QString>
#include <QMutex>
#include <QColor>
#include <QFont>
#include <netdb.h>
#include "html/ParserDom.h"
#include "mainwindow.h"
#include "abstracttranslator.h"
#include "globalcontrol.h"
#include "snwaitctl.h"

using namespace htmlcxx;

class CHTMLNode {
public:
    QString text, tagName, closingText;
    bool isTag, isComment;
    QList<CHTMLNode> children;
    QMap<QString,QString> attributes;
    CHTMLNode();
    ~CHTMLNode();
    CHTMLNode(tree<HTML::Node> const & node);
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


    bool localHosting;
    QString hostingDir;
    QString hostingUrl;
    QStringList* createdFiles;
    bool useSCP;
    QString Uri;
    QString scpParams;
    QString scpHost;
    CSnWaitCtl* waitDlg;
    int atlTcpRetryCount;
    int atlTcpTimeout;
    bool abortFlag;
    bool translatorFailed;
    QString translatorError;
    int translationEngine;
    QMutex abortMutex;
    int textNodesCnt;
    int textNodesProgress;
    CAbstractTranslator* tran;
    bool tranInited;
    bool useOverrideFont;
    bool forceFontColor;
    int translationMode;
    QColor forcedFontColor;
    QFont overrideFont;
    int engine;
    QString srcLanguage;

    bool calcLocalUrl(const QString& aUri, QString& calculatedUrl);
    bool translateDocument(const QString& srcUri, QString& dst);

    void examineNode(CHTMLNode & node, XMLPassMode xmlPass);
    bool translateParagraph(CHTMLNode & src, XMLPassMode xmlPass);
    void generateHTML(CHTMLNode & src, QString &html);

public:
    explicit CTranslator(QObject* parent, QString aUri, CSnWaitCtl* aWaitDlg);
    ~CTranslator();
    bool documentReparse(const QString& srcUri, QString& dst);

signals:
    void calcFinished(const bool success, const QString &aUrl);

public slots:
    void abortTranslator();
    void translate();

};

#endif // TRANSLATOR_H
