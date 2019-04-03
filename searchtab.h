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

class CSearchTab : public CSpecTabContainer
{
    Q_OBJECT

public:
    QString selectedUri;

    explicit CSearchTab(CMainWindow *parent);
    virtual ~CSearchTab();
    void selectFile(const QString& uri = QString(), const QString& dispFilename = QString());
    void updateQueryHistory();
    QString getLastQuery() { return lastQuery; }
    void keyPressEvent(QKeyEvent *event);
    void searchTerm(const QString &term, bool startSearch = true);
    QString getDocTitle();
    void setDocTitle(const QString &title);
private:
    Ui::SearchTab *ui;
    CSearchModel *model;
    CSearchProxyFilterModel *sort;
    QString lastQuery;

    CIndexerSearch *engine;
    CTitlesTranslator *titleTran;

protected:
    void doSearch();
    QString createSpecSnippet(const QString &aFilename, bool forceUntranslated, const QString &auxText);
    QStringList splitQuery(const QString &aQuery);

signals:
    void startSearch(const QString &searchTerm, const QDir &searchDir);
    void translateTitlesSrc(const QStringList &titles);

public slots:
    void doNewSearch();
    void applySnippetIdx(const QModelIndex& index);
    void showSnippet();
    void execSnippet(const QModelIndex& index);
    void selectDir();
    void searchFinished(const CStringHash &stats, const QString &query);
    void translateTitles();
    void gotTitleTranslation(const QStringList &res);
    void updateProgress(const int pos);
    void headerMenu(const QPoint& pos);
    void snippetMenu(const QPoint& pos);
    void applyFilter();
    void applySnippet(const QItemSelection & selected, const QItemSelection & deselected);
    void gotSearchResult(const CStringHash &item);
};

#endif // SEARCHTAB_H
