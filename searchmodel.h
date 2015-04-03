#ifndef CSEARCHMODEL_H
#define CSEARCHMODEL_H

#include <QObject>
#include <QAbstractTableModel>
#include <QTableView>
#include "indexersearch.h"

class CSearchModel : public QAbstractTableModel
{
    QList<QStrHash> snippets;
    QTableView *table;

public:
    CSearchModel(QObject *parent = 0, QTableView *view = 0);
    ~CSearchModel();

//    Qt::ItemFlags flags(const QModelIndex & index) const;
    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    int rowCount( const QModelIndex & parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent) const;
    QStrHash getSnippet(int idx);
    void setSnippet(int idx, QStrHash snippet);
    QStringList getSnippetKeys(int idx);

public slots:
    void deleteAllItems();
    void addItem(const QStrHash& srcSnippet);
    void addItems(const QList<QStrHash> &srcSnippets);

};

#endif // CSEARCHMODEL_H
