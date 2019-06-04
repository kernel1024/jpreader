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

    QAction *actionTMAdditive;
    QAction *actionTMOverwriting;
    QAction *actionTMTooltip;
    QActionGroup *translationMode;
    QActionGroup *languageSelector;

    QTimer gctxTimer;
    QString gctxSelection;

    explicit CGlobalUI(QObject *parent = nullptr);

    // Actions for Settings menu
    bool useOverrideFont() const;
    bool autoTranslate() const;
    bool forceFontColor() const;
    void startGlobalContextTranslate();
    bool translateSubSentences() const;
    void addActionNotification(QAction* action);

    TranslationMode getTranslationMode() const;
    QString getActiveLangPair() const;

Q_SIGNALS:
    void gctxStart(const QString& text);

public Q_SLOTS:
    void clipboardChanged(QClipboard::Mode mode);
    void gctxTranslateReady(const QString& text);
    void showGlobalTooltip(const QString& text);
    void rebuildLanguageActions(QObject *control = nullptr);
    void actionToggled();

};

#endif // CGLOBALUI_H
