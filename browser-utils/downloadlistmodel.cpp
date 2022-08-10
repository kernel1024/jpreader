#include "downloadlistmodel.h"

namespace CDefaults {
const int downloadListColumnCount = 2;
}

CDownloadListModel::CDownloadListModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

bool CDownloadListModel::appendItem(const QString &fileName, const QString &url)
{
    const auto item = qMakePair(fileName,url);
    if (m_data.contains(item))
        return false;

    int row = m_data.count();
    beginInsertRows(QModelIndex(),row,row);
    m_data.append(item);
    endInsertRows();
    return true;
}

QString CDownloadListModel::getFileName(const QModelIndex &index) const
{
    if (!checkIndex(index,CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return QString();

    return m_data.at(index.row()).first;
}

QString CDownloadListModel::getUrl(const QModelIndex &index) const
{
    if (!checkIndex(index,CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return QString();

    return m_data.at(index.row()).second;
}

Qt::ItemFlags CDownloadListModel::flags(const QModelIndex &index) const
{
    if (!checkIndex(index,CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return Qt::NoItemFlags;

    return (Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
}

QVariant CDownloadListModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index,CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return QVariant();

    int idx = index.row();
    auto data = m_data.at(idx);

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case 0: return data.first;
            case 1: return data.second;
            default: return QVariant();
        }
    }

    return QVariant();
}

bool CDownloadListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!checkIndex(index,CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return false;
    if ((flags(index) & Qt::ItemIsEditable) == 0)
        return false;
    if (!value.canConvert<QString>())
        return false;

    int row = index.row();
    switch (role) {
        case Qt::EditRole:
        case Qt::DisplayRole:
            switch (index.column()) {
                case 0: m_data[row].first = value.toString(); break;
                case 1: m_data[row].second = value.toString(); break;
                default: return false;
            }
            Q_EMIT dataChanged(index,index);
            return true;
        default: return false;
    }
    return false;
}

QVariant CDownloadListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch (section) {
            case 0: return tr("File name");
            case 1: return tr("Url");
            default: return QVariant();
        }
    }
    return QVariant();
}

int CDownloadListModel::rowCount(const QModelIndex &parent) const
{
    if (!checkIndex(parent))
        return 0;
    if (parent.isValid())
        return 0;

    return m_data.count();
}

int CDownloadListModel::columnCount(const QModelIndex &parent) const
{
    if (!checkIndex(parent))
        return 0;
    if (parent.isValid())
        return 0;

    return CDefaults::downloadListColumnCount;
}

CDownloadListModel::~CDownloadListModel() = default;

