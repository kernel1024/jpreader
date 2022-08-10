#ifndef CDOWNLOADLISTMODEL_H
#define CDOWNLOADLISTMODEL_H

#include <QAbstractTableModel>
#include <QObject>
#include <QString>
#include <QUrl>

class CDownloadListModel : public QAbstractTableModel
{
    Q_OBJECT
    Q_DISABLE_COPY(CDownloadListModel)

private:
    QList<QPair<QString,QString> > m_data;

public:
    explicit CDownloadListModel(QObject *parent = nullptr);
    ~CDownloadListModel() override;

    bool appendItem(const QString& fileName, const QString &url);
    QString getFileName(const QModelIndex & index) const;
    QString getUrl(const QModelIndex & index) const;

protected:
    Qt::ItemFlags flags(const QModelIndex & index) const override;
    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    int rowCount( const QModelIndex & parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent) const override;

};

#endif // CDOWNLOADLISTMODEL_H
