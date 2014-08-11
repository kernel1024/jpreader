#ifndef SEARCHTAB_H
#define SEARCHTAB_H

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QKeyEvent>
#include "mainwindow.h"
#include "indexersearch.h"
#include "titlestranslator.h"

namespace Ui {
    class SearchTab;
}

class CSearchTab : public QWidget
{
    Q_OBJECT

public:
    QString selectedUri;

    explicit CSearchTab(QWidget *parent, CMainWindow *parentWnd);
    virtual ~CSearchTab();
    void selectFile(const QString& uri = "", const QString& dispFilename = "");
    void updateQueryHistory();
    void bindToTab(QSpecTabWidget *tabs);
    QString getLastQuery() { return lastQuery; }
    void keyPressEvent(QKeyEvent *event);
    void searchTerm(const QString &term);
private:
    Ui::SearchTab *ui;
    CMainWindow *mainWnd;
    QBResult result;
    QSpecTabWidget *tabWidget;
    QString lastQuery;
    int sortMode;

    CIndexerSearch *engine;
    CTitlesTranslator *titleTran;

protected:
    void doSearch();
    QString createSpecSnippet(QString aFilename, bool forceUntranslated = false);
    QStringList splitQuery(QString aQuery);

signals:
    void startSearch(const QString &searchTerm, const QDir &searchDir);
    void translateTitlesSrc(const QStringList &titles);

public slots:
    void doNewSearch();
    void applyFilter(int idx);
    void applySort(int idx);
    void applySnippet(int row, int column, int oldRow, int oldColumn);
    void showSnippet();
    void execSnippet(int row, int column);
    void columnClicked(int col);
    void rowIdxClicked(int row);
    void closeTab(bool nowait = false);
    void selectDir();
    void searchFinished(const QBResult &aResult, const QString &aQuery);
    void headerContextMenu(const QPoint& pos);
    void translateTitles();
    void gotTitleTranslation(const QStringList &res);
    void updateProgress(const int pos);

};

#endif // SEARCHTAB_H
