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

    if (!index.isValid()) return QVariant();

    int idx = index.row();

    if (!m_filter.isEmpty()) {
        if (idx<0 || idx>=m_filterList.count()) return QVariant();
        idx = m_filterList.at(idx);
    }
    if (idx<0 || idx>=m_snippets.count()) return QVariant();

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case 0: return m_snippets[idx][QStringLiteral("title")];
            case 1: return m_snippets[idx][QStringLiteral("relevancyrating")];
            case 2: return m_snippets[idx][QStringLiteral("jp:dir")];
            case 3: return formatSize(m_snippets[idx][QStringLiteral("fbytes")]);
            case 4: return m_snippets[idx][QStringLiteral("filename")];
            default: return QVariant();
        }

    } else if (role == Qt::ToolTipRole || role == Qt::StatusTipRole) {
        if (index.column()==2) // show full path in tooltips
            return m_snippets[idx][QStringLiteral("jp:filepath")];
        if (m_snippets[idx].contains(QStringLiteral("title:saved")))
            return m_snippets[idx][QStringLiteral("title:saved")];

    } else if (role == Qt::UserRole + cpSortRole) {
        switch (index.column()) {
            case 0: return m_snippets[idx][QStringLiteral("title")];
            case 1:
                score = m_snippets[idx][QStringLiteral("relevancyrating")];
                if (score.endsWith('%'))
                    score.remove('%');

                sf = score.toFloat(&ok);
                if (ok)
                    return sf;

                return score;

            case 2: return m_snippets[idx][QStringLiteral("jp:filepath")];
            case 3:
                score = m_snippets[idx][QStringLiteral("fbytes")];
                sc = score.toInt(&ok);
                if (ok)
                    return sc;

                return score;

            case 4: return m_snippets[idx][QStringLiteral("filename")];
            default: return QVariant();
        }

    } else if (role == Qt::UserRole + cpFilterRole) {
        // column calculation removed - filtering only by directory
        return m_snippets[idx][QStringLiteral("jp:dir")];
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
            if (section>=0 && section<rowCount()) {
                if (!m_filter.isEmpty())
                    return QString::number(m_filterList.at(section)+1);

                return QString::number(section+1);
            }
            return QVariant();
        }
        return QVariant();
    }
    return QVariant();
}

int CSearchModel::rowCount(const QModelIndex &) const
{
    if (m_filter.isEmpty())
        return m_snippets.count();

    return m_filterList.count();
}

int CSearchModel::columnCount(const QModelIndex &) const
{
    return 5;
}

CStringHash CSearchModel::getSnippet(int idx) const
{
    if (idx<0 || idx>=rowCount()) return CStringHash();

    if (m_filter.isEmpty())
        return m_snippets.at(idx);

    return m_snippets.at(m_filterList.at(idx));
}

CStringHash CSearchModel::getSnippet(const QModelIndex &index) const
{
    if (!index.isValid()) return CStringHash();
    return getSnippet(index.row());
}

void CSearchModel::setSnippet(int idx, const CStringHash& snippet)
{
    if (idx<0 || idx>=rowCount()) return;

    if (m_filter.isEmpty())
        m_snippets[idx]=snippet;
    else
        m_snippets[m_filterList.at(idx)]=snippet;
        // do not reapply filter - Dir not changed

    emit itemContentsUpdated();
}

QStringList CSearchModel::getDistinctValues(const QString &snippetKey)
{
    QStringList sl;
    if (snippetKey.isEmpty()) return sl;

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
    m_filterList.clear();
    m_filter.clear(); // also clear filter
    endRemoveRows();
}

void CSearchModel::addItem(const CStringHash &srcSnippet)
{
    int posidx = m_snippets.count();
    if (!m_filter.isEmpty()) {
        const QString s = srcSnippet[QStringLiteral("jp:dir")];
        if (s.contains(m_filter)) {
            m_filterList.append(posidx);
            posidx = m_filterList.count();
        } else
            posidx = -1;
    }

    if (posidx>=0)
        beginInsertRows(QModelIndex(),posidx,posidx);

    m_snippets.append(srcSnippet);

    if (posidx>=0)
        endInsertRows();
}

void CSearchModel::addItems(const QVector<CStringHash> &srcSnippets)
{
    for (const auto & item : srcSnippets)
        addItem(item);
}

void CSearchModel::setFilter(const QString &text)
{
    beginRemoveRows(QModelIndex(),0,rowCount()-1);
    m_filter.clear();
    m_filterList.clear();
    QVector<int> filterList;
    endRemoveRows();

    if (text.isEmpty()) {
        beginInsertRows(QModelIndex(),0,rowCount()-1);
        endInsertRows();
        return;
    }

    for (int i=0;i<m_snippets.count();i++) {
        const QVariant v = data(index(i,2),Qt::UserRole + cpFilterRole);
        if (v.toString().contains(text))
            filterList.append(i);
    }

    beginInsertRows(QModelIndex(),0,filterList.count()-1);
    m_filter = text;
    m_filterList = filterList;
    endInsertRows();
}
