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
#include "atlastranslator.h"
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
    QString atlHost;
    int atlPort;
    int atlTcpRetryCount;
    int atlTcpTimeout;
    bool atlasToAbort;
    bool atlasSlipped;
    int translationEngine;
    QMutex abortMutex;
    int textNodesCnt;
    int textNodesProgress;
    CAtlasTranslator atlas;
    XMLPassMode xmlPass;
    bool useOverrideFont;
    bool forceFontColor;
    int translationMode;
    QColor forcedFontColor;
    QFont overrideFont;

    bool calcLocalUrl(const QString& aUri, QString& calculatedUrl);
    bool translateWithAtlas(const QString& srcUri, QString& dst);
    void examineXMLNode(QDomNode node);
    bool translateParagraph(QDomNode src);

public:
    explicit CTranslator(QObject* parent, QString aUri, CSnWaitCtl* aWaitDlg);

signals:
    void calcFinished(const bool success, const QString &aUrl);

public slots:
    void abortAtlas();
    void translate();

};

#endif // TRANSLATOR_H
