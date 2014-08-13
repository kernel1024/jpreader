#ifndef SNCTXHANDLER_H
#define SNCTXHANDLER_H

#include <QObject>
#include <QPoint>
#include <QString>
#include "snviewer.h"

class CSnippetViewer;

class CSnCtxHandler : public QObject
{
    Q_OBJECT
private:
    CSnippetViewer *snv;
public:
    bool menuActive;
    CSnCtxHandler(CSnippetViewer *parent);
public slots:
    void contextMenu(const QPoint &pos);
    void translateFragment();
    void copyImgUrl();
    void duplicateTab();
    void openNewWindow();
    void openImgNewTab();
    void saveImgToFile();
    void addContextBlock();
    void saveToFile();
    void openNewTab();
    void openNewTabTranslate();
    void bookmarkFrame();
    void openFrame();
    void loadKwrite();
    void execKonq();
    void searchInGoogle();
    void searchLocal();
    void searchInJisho();
    void toolTipHide();
    void gotTranslation(const QString& text);
    void createPlainTextTab();
    void createPlainTextTabTranslate();
signals:
    void startTranslation();
    void hideTooltips();
};

#endif // SNCTXHANDLER_H
