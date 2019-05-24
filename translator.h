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

class CTranslator;

class CHTMLNode {
    friend class CTranslator;

private:
    QString m_text, m_tagName, m_closingText;
    QVector<CHTMLNode> m_children;
    QHash<QString,QString> m_attributes;
    QStringList m_attributesOrder;
    bool m_isTag {false};
    bool m_isComment {false};

public:
    CHTMLNode() = default;
    ~CHTMLNode();
    CHTMLNode(const CHTMLNode& other);
    CHTMLNode(CHTMLNode&& other) = default;
    CHTMLNode(tree<htmlcxx::HTML::Node> const & node);
    CHTMLNode(const QString& innerText);
    CHTMLNode &operator=(const CHTMLNode& other);
    CHTMLNode &operator=(CHTMLNode&& other) = default;
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
    QUrl metaSrcUrl;
    QStringList* createdFiles;
    QStringList imgUrls;
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

    void dumpPage(QUuid token, const QString& suffix, const QString& page);
    void dumpPage(QUuid token, const QString& suffix, const CHTMLNode& page);
    void dumpPage(QUuid token, const QString& suffix, const QByteArray& page);

public:
    explicit CTranslator(QObject* parent, const QString &aUri, bool forceTranSubSentences = false);
    ~CTranslator();
    bool documentReparse(const QString& srcUri, QString& dst);
    QStringList getImgUrls() const;

    static void generateHTML(const CHTMLNode &src, QString &html, bool reformat = false,
                             int depth = 0);
    static void replaceLocalHrefs(CHTMLNode &node, const QUrl &baseUrl);
signals:
    void calcFinished(bool success, const QString &aUrl, const QString &error);
    void setProgress(int value);

public slots:
    void abortTranslator();
    void translate();

};

#endif // TRANSLATOR_H
