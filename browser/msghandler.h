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
    Q_DISABLE_COPY(CBrowserMsgHandler)
private:
    CBrowserTab *snv;
    QMutex m_lockTranEngine;
    qreal m_zoomFactor { 1.0 };
    QTimer m_loadingBarHideTimer;
    QTimer m_focusTimer;

public:
    enum PasteLoginMode {
        plmBoth,
        plmUsername,
        plmPassword
    };
    Q_ENUM(PasteLoginMode)
    explicit CBrowserMsgHandler(CBrowserTab * parent);
    ~CBrowserMsgHandler() override = default;
    void updateZoomFactor();
    void activateFocusDelay();
    void pastePassword(const QString& realm, CBrowserMsgHandler::PasteLoginMode mode);

public Q_SLOTS:
    void linkHovered(const QString &link);
    void showInspector();
    void searchFwd();
    void searchBack();
    void searchFocus();
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
