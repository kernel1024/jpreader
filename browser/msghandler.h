#ifndef SNMSGHANDLER_H
#define SNMSGHANDLER_H

#include <QObject>
#include <QString>
#include <QAction>
#include <QMutex>
#include <QTimer>
#include <QWebEnginePage>

class CBrowserTab;

class CBrowserMsgHandler : public QObject
{
    Q_OBJECT
private:
    CBrowserTab *snv;
    QMutex m_lockTranEngine;
    qreal m_zoomFactor;
    QTimer m_loadingBarHideTimer;
    QTimer m_focusTimer;

public:
    explicit CBrowserMsgHandler(CBrowserTab * parent);
    void updateZoomFactor();
    void activateFocusDelay();

public Q_SLOTS:
    void linkHovered(const QString &link);
    void showInspector();
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
    void languageContextMenu(const QPoint& pos);
    void renderProcessTerminated(QWebEnginePage::RenderProcessTerminationStatus terminationStatus,
                                 int exitCode);
Q_SIGNALS:
    void loadingBarHide();

};

#endif // SNMSGHANDLER_H
