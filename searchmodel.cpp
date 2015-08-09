#include <math.h>
#include "searchmodel.h"

CSearchModel::CSearchModel(QObject *parent, QTableView *view)
    : QAbstractTableModel(parent)
{
    snippets.clear();
    table = view;
}

CSearchModel::~CSearchModel()
{
    snippets.clear();
}

QVariant CSearchModel::data(const QModelIndex &index, int role) const
{
    bool ok;
    int sc;
    float sf;
    QString score,ps;

    if (!index.isValid()) return QVariant();

    int idx = index.row();

    if (idx<0 || idx>=snippets.count()) return QVariant();

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case 0: return snippets[idx]["dc:title"];
            case 1:
                score = snippets[idx]["Score"];
                ps.clear();
                if (snippets[idx]["relMode"]=="count")
                    sc = ceil(score.toFloat(&ok));
                else {
                    sc = score.toFloat(&ok)*100;
                    ps = "%";
                }
                if (ok)
                    return tr("%1%2").arg(sc).arg(ps);
                else
                    return score;
                break;
            case 2: return snippets[idx]["Dir"];
            case 3: return snippets[idx]["FileSize"];
            case 4: return snippets[idx]["OnlyFilename"];
            default: return QVariant();
        }
        return QVariant();

    } else if (role == Qt::ToolTipRole || role == Qt::StatusTipRole) {
        if (snippets[idx].contains("dc:title:saved"))
            return snippets[idx]["dc:title:saved"];

    } else if (role == Qt::UserRole + cpSortRole) {
        switch (index.column()) {
            case 0: return snippets[idx]["dc:title"];
            case 1:
                score = snippets[idx]["Score"];
                sf = score.toFloat(&ok);
                if (ok)
                    return sf;
                else
                    return score;
                break;
            case 2: return snippets[idx]["FilePath"];
            case 3:
                score = snippets[idx]["FileSizeNum"];
                sc = score.toInt(&ok);
                if (ok)
                    return sc;
                else
                    return score;
            case 4: return snippets[idx]["OnlyFilename"];
            default: return QVariant();
        }
        return QVariant();

    } else if (role == Qt::UserRole + cpFilterRole) {
        // column calculation removed - filtering only by directory
        return snippets[idx]["Dir"];
    }
    return QVariant();
}

QVariant CSearchModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal) {
        if (role == Qt::DisplayRole) {
            switch (section) {
                case 0: return tr("Title");
                case 1: return tr("Rel.");
                case 2: return tr("Group");
                case 3: return tr("Size");
                case 4: return tr("Filename");
                default: return QVariant();
            }
            return QVariant();
        }
        return QVariant();
    }
    if (orientation == Qt::Vertical) {
        if (role == Qt::DisplayRole) {
            if (section>=0 && section<snippets.count())
                return tr("%1").arg(section+1);
            return QVariant();
        }
        return QVariant();
    }
    return QVariant();
}

int CSearchModel::rowCount(const QModelIndex &) const
{
    return snippets.count();
}

int CSearchModel::columnCount(const QModelIndex &) const
{
    return 5;
}

QStrHash CSearchModel::getSnippet(int idx)
{
    if (idx>=0 && idx<snippets.count())
        return snippets.at(idx);
    else
        return QStrHash();
}

void CSearchModel::setSnippet(int idx, QStrHash snippet)
{
    if (idx<0 || idx>=snippets.count()) return;
    snippets[idx]=snippet;
    emit itemContentsUpdated();
}

QStringList CSearchModel::getSnippetKeys(int idx)
{
    return snippets[idx].keys();
}

QStringList CSearchModel::getDistinctValues(QString snippetKey)
{
    QStringList sl;
    sl.clear();
    if (snippetKey.isEmpty()) return sl;

    for (int i=0;i<snippets.count();i++) {
        QString s = snippets[i][snippetKey];
        if (!s.isEmpty() && !sl.contains(s))
            sl << s;
    }
    return sl;
}

void CSearchModel::deleteAllItems()
{
    beginRemoveRows(QModelIndex(),0,snippets.count()-1);
    snippets.clear();
    endRemoveRows();
}

void CSearchModel::addItem(const QStrHash &srcSnippet)
{
    QList<QStrHash> l;
    l.append(srcSnippet);
    addItems(l);
}

void CSearchModel::addItems(const QList<QStrHash> &srcSnippets)
{
    int posidx = snippets.count();
    beginInsertRows(QModelIndex(),posidx,posidx+srcSnippets.count()-1);
    snippets.append(srcSnippets);
    endInsertRows();
    if (table!=NULL) {
        table->resizeColumnsToContents();
        for (int i=0;i<columnCount(QModelIndex());i++) {
            if (table->columnWidth(i)>400)
                table->setColumnWidth(i,400);
        }
    }
}
