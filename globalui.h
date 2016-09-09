#ifndef CGLOBALUI_H
#define CGLOBALUI_H

#include <QObject>
#include <QAction>
#include <QClipboard>
#include <QTimer>
#include "qxtglobalshortcut.h"
#include "mainwindow.h"

class CMainWindow;

class CGlobalUI : public QObject
{
    Q_OBJECT
public:
    QxtGlobalShortcut gctxTranHotkey;
    QAction *actionUseProxy;

    QAction* actionSelectionDictionary;
    QAction* actionGlobalTranslator;

    QAction *actionJSUsage;
    QAction *actionSnippetAutotranslate;
    QAction *actionLogNetRequests;

    QAction *actionAutoTranslate;
    QAction *actionOverrideFont;
    QAction *actionAutoloadImages;
    QAction *actionOverrideFontColor;
    QAction *actionTranslateSubSentences;

    QAction *actionTMAdditive, *actionTMOverwriting, *actionTMTooltip;
    QAction *actionLSJapanese, *actionLSChineseTraditional;
    QAction *actionLSChineseSimplified, *actionLSKorean;
    QActionGroup *translationMode, *sourceLanguage;

    // Actions for Settings menu
    bool useOverrideFont();
    bool autoTranslate();
    bool forceFontColor();
    void startGlobalContextTranslate();
    bool translateSubSentences();

    explicit CGlobalUI(QObject *parent = 0);

private:
    QTimer gctxTimer;
    QString gctxSelection;

signals:
    void gctxStart(const QString& text);

public slots:
    void clipboardChanged(QClipboard::Mode mode);
    void gctxTranslateReady(const QString& text);
    CMainWindow* addMainWindow();
    CMainWindow* addMainWindowEx(bool withSearch, bool withViewer,
                                 const QUrl &withViewerUrl = QUrl());

};

#endif // CGLOBALUI_H
