#ifndef SEARCHTAB_H
#define SEARCHTAB_H

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QKeyEvent>
#include <QSortFilterProxyModel>
#include <QScopedPointer>
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
    explicit CSearchTab(CMainWindow *parent);
    ~CSearchTab() override;
    void selectFile(const QString& uri = QString(), const QString& dispFilename = QString());
    void updateQueryHistory();
    QString getLastQuery() { return lastQuery; }
    void searchTerm(const QString &term, bool startSearch = true);

private:
    Ui::SearchTab *ui;
    CSearchModel *model;
    CSearchProxyFilterModel *sort;
    QString lastQuery;
    QString selectedUri;

    QScopedPointer<CIndexerSearch> engine;
    QScopedPointer<CTitlesTranslator> titleTran;

    Q_DISABLE_COPY(CSearchTab)

protected:
    void doSearch();
    QString createSpecSnippet(const QString &aFilename, bool forceUntranslated, const QString &auxText);
    QStringList splitQuery(const QString &aQuery);

Q_SIGNALS:
    void startSearch(const QString &searchTerm, const QDir &searchDir);
    void translateTitlesSrc(const QStringList &titles);

public Q_SLOTS:
    void doNewSearch();
    void applySnippetIdx(const QModelIndex& index);
    void showSnippet();
    void execSnippet(const QModelIndex& index);
    void selectDir();
    void searchFinished(const CStringHash &stats, const QString &query);
    void translateTitles();
    void gotTitleTranslation(const QStringList &res);
    void updateProgress(int pos);
    void headerMenu(const QPoint& pos);
    void snippetMenu(const QPoint& pos);
    void applyFilter();
    void applySnippet(const QItemSelection & selected, const QItemSelection & deselected);
    void gotSearchResult(const CStringHash &item);
};

#endif // SEARCHTAB_H
