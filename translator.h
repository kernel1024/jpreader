#ifndef TRANSLATOR_H
#define TRANSLATOR_H

#include <QObject>
#include <QString>
#include <QMutex>
#include <QDomNode>
#include <QColor>
#include <QFont>
#include <netdb.h>
#include "mainwindow.h"
#include "abstracttranslator.h"
#include "globalcontrol.h"
#include "snwaitctl.h"

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
    void examineXMLNode(QDomNode node, XMLPassMode xmlPass);
    bool translateParagraph(QDomNode src, XMLPassMode xmlPass);

public:
    explicit CTranslator(QObject* parent, QString aUri, CSnWaitCtl* aWaitDlg);
    ~CTranslator();
    bool documentToXML(const QString& srcUri, QString& dst);

signals:
    void calcFinished(const bool success, const QString &aUrl);

public slots:
    void abortTranslator();
    void translate();

};

#endif // TRANSLATOR_H
