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

class CSearchTab : public QSpecTabContainer
{
    Q_OBJECT

public:
    QString selectedUri;

    explicit CSearchTab(CMainWindow *parent);
    virtual ~CSearchTab();
    void selectFile(const QString& uri = "", const QString& dispFilename = "");
    void updateQueryHistory();
    QString getLastQuery() { return lastQuery; }
    void keyPressEvent(QKeyEvent *event);
    void searchTerm(const QString &term);
    QString getDocTitle();
    void setDocTitle(const QString &title);
private:
    Ui::SearchTab *ui;
    QBResult result;
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
    void selectDir();
    void searchFinished(const QBResult &aResult, const QString &aQuery);
    void headerContextMenu(const QPoint& pos);
    void translateTitles();
    void gotTitleTranslation(const QStringList &res);
    void updateProgress(const int pos);
};

#endif // SEARCHTAB_H
