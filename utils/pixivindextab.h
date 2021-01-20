#ifndef PIXIVINDEXTAB_H
#define PIXIVINDEXTAB_H

#include <QObject>
#include <QPointer>
#include <QJsonObject>
#include <QUrlQuery>
#include <QSortFilterProxyModel>
#include "utils/specwidgets.h"
#include "translator/titlestranslator.h"
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
                            const QString &indexId, const QUrlQuery &sourceQuery,
                            const QString &extractorFilterDesc);
    ~CPixivIndexTab() override;
    QString title() const;

private:
    Q_DISABLE_COPY(CPixivIndexTab)

    QScopedPointer<CTitlesTranslator,QScopedPointerDeleteLater> m_titleTran;
    Ui::CPixivIndexTab *ui { nullptr };
    QPointer<CPixivTableModel> m_model;
    QPointer<QSortFilterProxyModel> m_proxyModel;
    QString m_indexId;
    QString m_title;
    QUrlQuery m_sourceQuery;
    CPixivIndexExtractor::IndexMode m_indexMode { CPixivIndexExtractor::IndexMode::WorkIndex };

    void updateWidgets(const QString& extractorFilterDesc);
    void updateCountLabel();
    QStringList jsonToTags(const QJsonArray& tags) const;
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
    void startTitlesAndTagsTranslation();
    void gotTitlesAndTagsTranslation(const QStringList &res);
    void updateTranslatorProgress(int pos);

private Q_SLOTS:
    void modelSorted(const QList<QPersistentModelIndex> &parents, QAbstractItemModel::LayoutChangeHint hint);
    void comboSortChanged(int index);
    void filterChanged(const QString& filter);

Q_SIGNALS:
    void translateTitlesAndTags(const QStringList &titles);

};

class CPixivTableModel : public QAbstractTableModel
{
    Q_OBJECT

private:
    QVector<QJsonObject> m_list;
    QStringList m_tags;
    QStringList m_translatedTags;
    CStringHash m_authors;

    void updateTags();
    QStringList basicHeaders() const;

public:
    CPixivTableModel(QObject *parent, const QVector<QJsonObject> &list);
    ~CPixivTableModel() override;

    CStringHash authors() const;
    bool isEmpty() const;
    int count() const;
    QJsonObject item(const QModelIndex& index) const;
    QString tag(const QModelIndex& index) const;
    QString text(const QModelIndex& index) const;
    QStringList getStringsForTranslation() const;
    void setStringsFromTranslation(const QStringList& translated);
    QStringList getTags() const;
    QString getTagForColumn(int column, int *tagNumber = nullptr) const;
    int getColumnForTag(const QString& tag) const;

protected:
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
};

#endif // PIXIVINDEXTAB_H
