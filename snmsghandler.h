#ifndef SNMSGHANDLER_H
#define SNMSGHANDLER_H

#include <QObject>
#include <QString>
#include <QAction>
#include <QMutex>
#include <QTimer>
#include <QWebEnginePage>
#include "snviewer.h"

class CSnippetViewer;

class CSnMsgHandler : public QObject
{
    Q_OBJECT
private:
    QMutex lockTranEngine;
    CSnippetViewer *snv;
    qreal zoomFactor;
public:
    QTimer *loadingBarHideTimer;
    QTimer *focusTimer;
    CSnMsgHandler(CSnippetViewer * parent);
    void updateZoomFactor();
public slots:
    void linkHovered(const QString &link);
    void searchFwd();
    void searchBack();
    void searchFocus();
    void pastePassword();
    void setZoom(QString z);
    void navByClick();
    void tranEngine(int engine);
    void updateTranEngine();
    void hideBarLoading();
    void urlEditSetFocus();
    void renderProcessTerminated(QWebEnginePage::RenderProcessTerminationStatus terminationStatus,
                                 int exitCode);
};

#endif // SNMSGHANDLER_H
