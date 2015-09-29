#ifndef SEARCHTAB_H
#define SEARCHTAB_H

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QKeyEvent>
#include <QSortFilterProxyModel>
#include "mainwindow.h"
#include "indexersearch.h"
#include "titlestranslator.h"
#include "searchmodel.h"

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
    void searchTerm(const QString &term, bool startSearch = true);
    QString getDocTitle();
    void setDocTitle(const QString &title);
private:
    Ui::SearchTab *ui;
    CSearchModel *model;
    QSortFilterProxyModel *sort;
    QStrHash stats;
    QString lastQuery;

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
    void applySnippet(const QModelIndex& index);
    void showSnippet();
    void execSnippet(const QModelIndex& index);
    void selectDir();
    void searchFinished(const QBResult &aResult, const QString &aQuery);
    void translateTitles();
    void gotTitleTranslation(const QStringList &res);
    void updateProgress(const int pos);
    void headerMenu(const QPoint& pos);
    void applyFilter();
    void applySnippet(const QItemSelection & selected, const QItemSelection & deselected);
};

#endif // SEARCHTAB_H
