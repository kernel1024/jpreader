#ifndef CGLOBALUI_H
#define CGLOBALUI_H

#include <QObject>
#include <QAction>
#include <QClipboard>
#include <QTimer>
#include "qxtglobalshortcut.h"
#include "mainwindow.h"
#include "structures.h"

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
    QAction *actionOverrideFontColor;
    QAction *actionTranslateSubSentences;

    QAction *actionTMAdditive, *actionTMOverwriting, *actionTMTooltip;
    QActionGroup *translationMode, *languageSelector;

    // Actions for Settings menu
    bool useOverrideFont();
    bool autoTranslate();
    bool forceFontColor();
    void startGlobalContextTranslate();
    bool translateSubSentences();
    void addActionNotification(QAction* action);

    explicit CGlobalUI(QObject *parent = nullptr);

    TranslationMode getTranslationMode();
    QString getActiveLangPair() const;
private:
    QTimer gctxTimer;
    QString gctxSelection;

Q_SIGNALS:
    void gctxStart(const QString& text);

public Q_SLOTS:
    void clipboardChanged(QClipboard::Mode mode);
    void gctxTranslateReady(const QString& text);
    CMainWindow* addMainWindow();
    CMainWindow* addMainWindowEx(bool withSearch, bool withViewer,
                                 const QVector<QUrl> &viewerUrls = { });
    void showGlobalTooltip(const QString& text);
    void rebuildLanguageActions(QObject *control = nullptr);

private Q_SLOTS:
    void actionToggled();

};

#endif // CGLOBALUI_H
