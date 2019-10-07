#include <cmath>
#include "searchmodel.h"
#include "genericfuncs.h"

CSearchModel::CSearchModel(QObject *parent, QTableView *view)
    : QAbstractTableModel(parent)
{
    m_snippets.clear();
    m_table = view;
}

CSearchModel::~CSearchModel()
{
    m_snippets.clear();
}

QVariant CSearchModel::data(const QModelIndex &index, int role) const
{
    bool ok;
    int sc;
    float sf;
    QString score;

    if (!checkIndex(index,CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return QVariant();

    int idx = index.row();

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case 0: return m_snippets[idx][QSL("title")];
            case 1: return m_snippets[idx][QSL("relevancyrating")];
            case 2: return m_snippets[idx][QSL("jp:dir")];
            case 3: return CGenericFuncs::formatFileSize(m_snippets[idx][QSL("fbytes")]);
            case 4: return m_snippets[idx][QSL("filename")];
            default: return QVariant();
        }

    } else if (role == Qt::ToolTipRole || role == Qt::StatusTipRole) {
        if (index.column()==2) // show full path in tooltips
            return m_snippets[idx][QSL("jp:filepath")];
        if (m_snippets[idx].contains(QSL("title:saved")))
            return m_snippets[idx][QSL("title:saved")];

    } else if (role == Qt::UserRole + CStructures::cpSortRole) {
        switch (index.column()) {
            case 0: return m_snippets[idx][QSL("title")];
            case 1:
                score = m_snippets[idx][QSL("relevancyrating")];
                if (score.endsWith('%'))
                    score.remove('%');

                sf = score.toFloat(&ok);
                if (ok)
                    return sf;

                return score;

            case 2: return m_snippets[idx][QSL("jp:filepath")];
            case 3:
                score = m_snippets[idx][QSL("fbytes")];
                sc = score.toInt(&ok);
                if (ok)
                    return sc;

                return score;

            case 4: return m_snippets[idx][QSL("filename")];
            default: return QVariant();
        }

    } else if (role == Qt::UserRole + CStructures::cpFilterRole) {
        // column calculation removed - filtering only by directory
        return m_snippets[idx][QSL("jp:dir")];
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
            if (section>=0 && section<rowCount())
                return QString::number(section+1);

            return QVariant();
        }
        return QVariant();
    }
    return QVariant();
}

int CSearchModel::rowCount(const QModelIndex &parent) const
{
    if (!checkIndex(parent))
        return 0;
    if (parent.isValid())
        return 0;

    return m_snippets.count();
}

int CSearchModel::columnCount(const QModelIndex &parent) const
{
    if (!checkIndex(parent))
        return 0;
    if (parent.isValid())
        return 0;

    const int columnsCount = 5;

    return columnsCount;
}

CStringHash CSearchModel::getSnippet(int row) const
{
    return getSnippet(index(row,0));
}

CStringHash CSearchModel::getSnippet(const QModelIndex &index) const
{
    if (!checkIndex(index,CheckIndexOption::IndexIsValid)) return CStringHash();
    return m_snippets.at(index.row());
}

void CSearchModel::setSnippet(int row, const CStringHash& snippet)
{
    if (row<0 || row>=rowCount()) return;

    m_snippets[row]=snippet;

    Q_EMIT itemContentsUpdated();
}

QStringList CSearchModel::getDistinctValues(const QString &snippetKey)
{
    QStringList sl;

    if (snippetKey.isEmpty()) return sl;

    sl.reserve(m_snippets.count());
    for (int i=0;i<m_snippets.count();i++) {
        QString s = m_snippets[i][snippetKey];
        if (!s.isEmpty() && !sl.contains(s))
            sl << s;
    }
    return sl;
}

void CSearchModel::deleteAllItems()
{
    beginRemoveRows(QModelIndex(),0,rowCount()-1);
    m_snippets.clear();
    endRemoveRows();
}

void CSearchModel::addItem(const CStringHash &srcSnippet)
{
    addItems( { srcSnippet } );
}

void CSearchModel::addItems(const QVector<CStringHash> &srcSnippets)
{
    int posidx = m_snippets.count();
    beginInsertRows(QModelIndex(),posidx,posidx+srcSnippets.count()-1);
    m_snippets.append(srcSnippets);
    endInsertRows();
}

void CSearchProxyFilterModel::setFilter(const QString &filter)
{
    m_filter = filter;
    invalidate(); // invalidateFilter segfaulting with filter_changed() and parent() on big lists
}

QVariant CSearchProxyFilterModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    const int headerDarkFactor = 120;
    if (orientation == Qt::Horizontal) {
        if (role == Qt::BackgroundRole && section == 2 && !m_filter.isEmpty()) {
            const QColor c = QApplication::palette("QHeaderView").window().color();
            return c.darker(headerDarkFactor);
        }
    }

    return QSortFilterProxyModel::headerData(section,orientation,role);
}

bool CSearchProxyFilterModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    Q_UNUSED(source_parent)

    if (m_filter.isEmpty())
        return true;

    QModelIndex src = sourceModel()->index(source_row,0);
    if (!src.isValid()) return false;

    const QVariant v = sourceModel()->data(src,Qt::UserRole + CStructures::cpFilterRole);

    return (v.toString().contains(m_filter));
}
