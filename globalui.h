#ifndef CGLOBALUI_H
#define CGLOBALUI_H

#include <QObject>
#include <QAction>
#include <QClipboard>
#include <QTimer>
#include "qxtglobalshortcut.h"
#include "mainwindow.h"
#include "structures.h"
#include "settings.h"

class CMainWindow;

class CGlobalUI : public QObject
{
    friend class CSettings;

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
    QActionGroup *subsentencesMode;

    QTimer gctxTimer;
    QString gctxSelection;

    QTimer threadedWorkerTestTimer;

    explicit CGlobalUI(QObject *parent = nullptr);

    // Actions for Settings menu
    bool useOverrideTransFont() const;
    bool autoTranslate() const;
    bool forceFontColor() const;
    void startGlobalContextTranslate();
    void addActionNotification(QAction* action) const;

    bool getSubsentencesMode(CStructures::TranslationEngine engine) const;
    CStructures::TranslationMode getTranslationMode() const;
    QString getActiveLangPair() const;
    void setActiveLangPair(const QString& hash) const;

public Q_SLOTS:
    void clipboardChanged(QClipboard::Mode mode);
    void gctxTranslateReady(const QString& text);
    void showGlobalTooltip(const QString& text);
    void rebuildLanguageActions(QObject *control = nullptr);
    void actionToggled();
    void updateBusyCursor();

private:
    CSubsentencesMode getSubsentencesModeHash() const;
    void setSubsentencesModeHash(const CSubsentencesMode& hash) const;


};

#endif // CGLOBALUI_H
