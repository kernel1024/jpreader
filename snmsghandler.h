#ifndef SNMSGHANDLER_H
#define SNMSGHANDLER_H

#include <QObject>
#include <QUrl>
#include <QString>
#include <QAction>
#include <QMutex>
#include <QTimer>
#include "snviewer.h"

class CSnippetViewer;

class CSnMsgHandler : public QObject
{
    Q_OBJECT
private:
    QMutex lockSrcLang, lockTranEngine;
    CSnippetViewer *snv;
public:
    QTimer *loadingBarHideTimer;
    CSnMsgHandler(CSnippetViewer * parent);
public slots:
    void linkHovered(const QString &link);
    void searchFwd();
    void searchBack();
    void setZoom(QString z);
    void navByClick();
    void srcLang(int lang);
    void tranEngine(int engine);
    void updateSrcLang(QAction *action);
    void updateTranEngine();
    void hideBarLoading();
};

#endif // SNMSGHANDLER_H
