#ifndef CSEARCHMODEL_H
#define CSEARCHMODEL_H

#include <QObject>
#include <QAbstractTableModel>
#include <QTableView>
#include "indexersearch.h"

#define cpSortRole 1
#define cpFilterRole 2

class CSearchModel : public QAbstractTableModel
{
    Q_OBJECT

private:
    QVector<CStringHash> m_snippets;
    QTableView *m_table;

public:
    CSearchModel(QObject *parent = nullptr, QTableView *view = nullptr);
    ~CSearchModel();

    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    int rowCount( const QModelIndex & parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent) const;
    CStringHash getSnippet(int idx) const;
    CStringHash getSnippet(const QModelIndex& index) const;
    void setSnippet(int idx, const CStringHash &snippet);
    QStringList getDistinctValues(const QString &snippetKey);

signals:
    void itemContentsUpdated();

public slots:
    void deleteAllItems();
    void addItem(const CStringHash& srcSnippet);
    void addItems(const QVector<CStringHash> &srcSnippets);

};

class CSearchProxyFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
private:
    QString m_filter;
public:
    CSearchProxyFilterModel(QObject* parent = nullptr)
        : QSortFilterProxyModel(parent) {}
    virtual ~CSearchProxyFilterModel() = default;
    void setFilter(const QString& filter);
protected:
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    virtual bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const;
};

#endif // CSEARCHMODEL_H
