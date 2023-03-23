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

class CGlobalActions : public QObject
{
    friend class CSettings;

    Q_OBJECT
public:
    QPointer<QxtGlobalShortcut> gctxTranHotkey;
    QAction *actionUseProxy;

    QAction* actionSelectionDictionary;
    QAction* actionGlobalTranslator;

    QAction *actionXapianForceFullScan;
    QAction *actionXapianClearAndRescan;

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

    QTimer threadedWorkerTestTimer;

    QHash<CStructures::TranslationEngine, QPointer<QActionGroup> > subsentencesMode;

    explicit CGlobalActions(QObject *parent = nullptr);

    // Actions for Settings menu
    bool useOverrideTransFont() const;
    bool autoTranslate() const;
    bool forceFontColor() const;
    void startGlobalContextTranslate();
    void addActionNotification(QAction* action) const;

    CStructures::SubsentencesMode getSubsentencesMode(CStructures::TranslationEngine engine) const;
    CStructures::TranslationMode getTranslationMode() const;
    QString getActiveLangPair() const;
    void setActiveLangPair(const QString& hash) const;

    QList<QAction*> getTranslationLanguagesActions() const;
    QList<QAction *> getSubsentencesModeActions(CStructures::TranslationEngine engine) const;

public Q_SLOTS:
    void clipboardChanged(QClipboard::Mode mode);
    void gctxTranslateReady(const QString& text);
    void showGlobalTooltip(const QString& text);
    void rebuildLanguageActions(QObject *control = nullptr);
    void rebindGctxHotkey(QObject *control = nullptr);
    void actionToggled();
    void updateBusyCursor();

private:
    CSubsentencesMode getSubsentencesModeHash() const;
    void setSubsentencesModeHash(const CSubsentencesMode& hash);
    void updateSubsentencesModeActions(const CSubsentencesMode &hash);

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

};

#endif // CGLOBALUI_H
