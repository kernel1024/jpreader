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
    QAction *actionOverrideTransFont;
    QAction *actionOverrideTransFontColor;

    QAction *actionTMAdditive;
    QAction *actionTMOverwriting;
    QAction *actionTMTooltip;
    QActionGroup *translationMode;
    QActionGroup *languageSelector;

    QTimer gctxTimer;
    QString gctxSelection;

    explicit CGlobalUI(QObject *parent = nullptr);

    // Actions for Settings menu
    bool useOverrideTransFont() const;
    bool autoTranslate() const;
    bool forceFontColor() const;
    void startGlobalContextTranslate();
    void addActionNotification(QAction* action);

    CStructures::TranslationMode getTranslationMode() const;
    QString getActiveLangPair() const;

public Q_SLOTS:
    void clipboardChanged(QClipboard::Mode mode);
    void gctxTranslateReady(const QString& text);
    void showGlobalTooltip(const QString& text);
    void rebuildLanguageActions(QObject *control = nullptr, const QString &activeLangPair = QString());
    void actionToggled();

};

#endif // CGLOBALUI_H
