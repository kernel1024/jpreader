#ifndef PIXIVINDEXTAB_H
#define PIXIVINDEXTAB_H

#include <QObject>
#include <QJsonObject>
#include <QUrlQuery>
#include "specwidgets.h"
#include "extractors/pixivindexextractor.h"

namespace Ui {
class CPixivIndexTab;
}

class CPixivTableModel;

class CPixivIndexTab : public CSpecTabContainer
{
    Q_OBJECT

public:
    explicit CPixivIndexTab(QWidget *parent, const QVector<QJsonObject> &list,
                            CPixivIndexExtractor::IndexMode indexMode,
                            const QString &indexId, const QUrlQuery &sourceQuery);
    ~CPixivIndexTab() override;
    QString title() const;

private:
    Q_DISABLE_COPY(CPixivIndexTab)

    Ui::CPixivIndexTab *ui { nullptr };
    CPixivTableModel *m_model { nullptr };
    QString m_indexId;
    QString m_title;
    QUrlQuery m_sourceQuery;
    CPixivIndexExtractor::IndexMode m_indexMode { CPixivIndexExtractor::IndexMode::WorkIndex };

    void updateWidgets();
    QString makeNovelInfoBlock(CStringHash* authors,
                               const QString& workId, const QString& workImgUrl,
                               const QString& title, int length,
                               const QString& author, const QString& authorId,
                               const QStringList& tags, const QString& description,
                               const QDateTime &creationDate, const QString &seriesTitle,
                               const QString &seriesId, int bookmarkCount);


public Q_SLOTS:
    void tableSelectionChanged(const QModelIndex &current, const QModelIndex &previous);
    void novelActivated(const QModelIndex &index);
    void tableContextMenu(const QPoint &pos);
    void htmlReport();
    void linkClicked(const QString& link);
    void processExtractorAction();
};

class CPixivTableModel : public QAbstractTableModel
{
    Q_OBJECT

private:
    QVector<QJsonObject> m_list;
    QStringList m_tags;
    CStringHash m_authors;

    void updateTags();
    QStringList basicHeaders() const;

public:
    CPixivTableModel(QObject *parent, const QVector<QJsonObject> &list);
    ~CPixivTableModel() override;

    CStringHash authors() const;
    bool isEmpty() const;
    QJsonObject item(const QModelIndex& index) const;
    QString tag(const QModelIndex& index) const;

protected:
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
};

#endif // PIXIVINDEXTAB_H
