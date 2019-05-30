#ifndef CSEARCHMODEL_H
#define CSEARCHMODEL_H

#include <QObject>
#include <QAbstractTableModel>
#include <QTableView>
#include "indexersearch.h"

const int cpSortRole = 1;
const int cpFilterRole = 2;

class CSearchModel : public QAbstractTableModel
{
    Q_OBJECT

private:
    QVector<CStringHash> m_snippets;
    QTableView *m_table;

    Q_DISABLE_COPY(CSearchModel)

public:
    explicit CSearchModel(QObject *parent = nullptr, QTableView *view = nullptr);
    ~CSearchModel() override;

    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    int rowCount( const QModelIndex & parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent) const override;
    CStringHash getSnippet(int idx) const;
    CStringHash getSnippet(const QModelIndex& index) const;
    void setSnippet(int idx, const CStringHash &snippet);
    QStringList getDistinctValues(const QString &snippetKey);

Q_SIGNALS:
    void itemContentsUpdated();

public Q_SLOTS:
    void deleteAllItems();
    void addItem(const CStringHash& srcSnippet);
    void addItems(const QVector<CStringHash> &srcSnippets);

};

class CSearchProxyFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
private:
    QString m_filter;

    Q_DISABLE_COPY(CSearchProxyFilterModel)

public:
    explicit CSearchProxyFilterModel(QObject* parent = nullptr)
        : QSortFilterProxyModel(parent) {}
    ~CSearchProxyFilterModel() override = default;
    void setFilter(const QString& filter);

protected:
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

};

#endif // CSEARCHMODEL_H
