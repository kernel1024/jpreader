#ifndef SNMSGHANDLER_H
#define SNMSGHANDLER_H

#include <QObject>
#include <QString>
#include <QAction>
#include <QMutex>
#include <QTimer>
#include <QWebEnginePage>

class CSnippetViewer;

class CSnMsgHandler : public QObject
{
    Q_OBJECT
private:
    QMutex lockTranEngine;
    CSnippetViewer *snv;
    qreal zoomFactor;
    QTimer *loadingBarHideTimer;
    QTimer *focusTimer;

public:
    explicit CSnMsgHandler(CSnippetViewer * parent);
    void updateZoomFactor();
    void activateFocusDelay();

public Q_SLOTS:
    void linkHovered(const QString &link);
    void searchFwd();
    void searchBack();
    void searchFocus();
    void pastePassword();
    void setZoom(const QString& z);
    void navByClick();
    void tranEngine(int index);
    void updateTranEngine();
    void hideBarLoading();
    void urlEditSetFocus();
    void renderProcessTerminated(QWebEnginePage::RenderProcessTerminationStatus terminationStatus,
                                 int exitCode);
Q_SIGNALS:
    void loadingBarHide();

};

#endif // SNMSGHANDLER_H
