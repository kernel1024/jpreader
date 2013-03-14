#ifndef CALCTHREAD_H
#define CALCTHREAD_H

#include <QThread>
#include <QString>
#include <QMutex>
#include <QDomNode>
#include <netdb.h>
#include "waitdlg.h"
#include "mainwindow.h"
#include "atlastranslator.h"
#include "globalcontrol.h"

class CCalcThread : public QThread
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
    CWaitDlg* waitDlg;
    QString atlHost;
    int atlPort;
    bool atlasToAbort;
    bool atlasSlipped;
    int translationEngine;
    QMutex abortMutex;
    int textNodesCnt;
    int textNodesProgress;
    CAtlasTranslator atlas;
    XMLPassMode xmlPass;

    bool calcLocalUrl(const QString& aUri, QString& calculatedUrl);
    bool translateWithAtlas(const QString& srcUri, QString& dst);
    void examineXMLNode(QDomNode node);
    bool translateParagraph(QDomNode src);

public:
    explicit CCalcThread(QObject* parent, QString aUri, CWaitDlg* aWaitDlg);
    void run();

signals:
    void calcFinished(bool success, QString aUrl);

public slots:
    void abortAtlas();

};

#endif // CALCTHREAD_H
