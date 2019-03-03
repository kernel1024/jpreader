#include <cmath>
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
            case 0: return snippets[idx][QStringLiteral("dc:title")];
            case 1:
                score = snippets[idx][QStringLiteral("Score")];
                ps.clear();
                if (snippets[idx][QStringLiteral("relMode")].startsWith(QStringLiteral("count")))
                    sc = static_cast<int>(ceil(score.toDouble(&ok)));
                else {
                    sc = static_cast<int>(score.toDouble(&ok)*100.0);
                    ps = QStringLiteral("%");
                }
                if (ok)
                    return QStringLiteral("%1%2").arg(sc).arg(ps);

                return score;

            case 2: return snippets[idx][QStringLiteral("Dir")];
            case 3: return snippets[idx][QStringLiteral("FileSize")];
            case 4: return snippets[idx][QStringLiteral("OnlyFilename")];
            default: return QVariant();
        }

    } else if (role == Qt::ToolTipRole || role == Qt::StatusTipRole) {
        if (index.column()==2) // show full path in tooltips
            return snippets[idx][QStringLiteral("FilePath")];
        if (snippets[idx].contains(QStringLiteral("dc:title:saved")))
            return snippets[idx][QStringLiteral("dc:title:saved")];
    } else if (role == Qt::UserRole + cpSortRole) {
        switch (index.column()) {
            case 0: return snippets[idx][QStringLiteral("dc:title")];
            case 1:
                score = snippets[idx][QStringLiteral("Score")];
                sf = score.toFloat(&ok);
                if (ok)
                    return sf;

                return score;

            case 2: return snippets[idx][QStringLiteral("FilePath")];
            case 3:
                score = snippets[idx][QStringLiteral("FileSizeNum")];
                sc = score.toInt(&ok);
                if (ok)
                    return sc;

                return score;

            case 4: return snippets[idx][QStringLiteral("OnlyFilename")];
            default: return QVariant();
        }

    } else if (role == Qt::UserRole + cpFilterRole) {
        // column calculation removed - filtering only by directory
        return snippets[idx][QStringLiteral("Dir")];
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
        }
        return QVariant();
    }
    if (orientation == Qt::Vertical) {
        if (role == Qt::DisplayRole) {
            if (section>=0 && section<snippets.count())
                return QString::number(section+1);
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

CStringHash CSearchModel::getSnippet(int idx) const
{
    if (idx>=0 && idx<snippets.count())
        return snippets.at(idx);

    return CStringHash();
}

void CSearchModel::setSnippet(int idx, const CStringHash& snippet)
{
    if (idx<0 || idx>=snippets.count()) return;
    snippets[idx]=snippet;
    emit itemContentsUpdated();
}

QStringList CSearchModel::getSnippetKeys(int idx)
{
    return snippets.at(idx).keys();
}

QStringList CSearchModel::getDistinctValues(const QString &snippetKey)
{
    QStringList sl;
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

void CSearchModel::addItem(const CStringHash &srcSnippet)
{
    QVector<CStringHash> l;
    l.append(srcSnippet);
    addItems(l);
}

void CSearchModel::addItems(const QVector<CStringHash> &srcSnippets)
{
    int posidx = snippets.count();
    beginInsertRows(QModelIndex(),posidx,posidx+srcSnippets.count()-1);
    snippets.append(srcSnippets);
    endInsertRows();
}
