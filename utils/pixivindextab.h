#ifndef PIXIVINDEXTAB_H
#define PIXIVINDEXTAB_H

#include <QObject>
#include <QPointer>
#include <QJsonArray>
#include <QUrlQuery>
#include <QSortFilterProxyModel>
#include "utils/specwidgets.h"
#include "translator/titlestranslator.h"
#include "extractors/pixivindexextractor.h"

namespace Ui {
class CPixivIndexTab;
}

class CPixivIndexModel;

class CPixivIndexTab : public CSpecTabContainer
{
    Q_OBJECT

public:
    explicit CPixivIndexTab(QWidget *parent, const QJsonArray &list,
                            CPixivIndexExtractor::ExtractorMode extractorMode,
                            CPixivIndexExtractor::IndexMode indexMode,
                            const QString &indexId, const QUrlQuery &sourceQuery,
                            const QString &extractorFilterDesc, const QUrl &coversOrigin);
    ~CPixivIndexTab() override;
    static CPixivIndexTab* fromJson(QWidget *parentWidget, const QJsonObject& data);
    CPixivIndexExtractor::ExtractorMode extractorMode() const;
    QPointer<QAbstractItemView> table() const;

    const QUrl &coversOrigin() const;

private:
    Q_DISABLE_COPY(CPixivIndexTab)

    QScopedPointer<CTitlesTranslator,QScopedPointerDeleteLater> m_titleTran;
    Ui::CPixivIndexTab *ui { nullptr };
    QPointer<CPixivIndexModel> m_model;
    QPointer<QSortFilterProxyModel> m_proxyModel;
    QPointer<QAbstractItemView> m_table;
    QString m_indexId;
    QString m_title;
    QUrl m_coversOrigin;
    QUrlQuery m_sourceQuery;
    QModelIndex m_currentCoverIndex;
    CPixivIndexExtractor::IndexMode m_indexMode { CPixivIndexExtractor::IndexMode::imWorkIndex };
    CPixivIndexExtractor::ExtractorMode m_extractorMode { CPixivIndexExtractor::ExtractorMode::emNovels };

    void updateWidgets(const QString& extractorFilterDesc);
    void updateCountLabel();
    QStringList jsonToTags(const QJsonArray& tags) const;
    void setCoverLabel(const QModelIndex& index);
    void fetchCoverLabel(const QModelIndex &index, const QUrl& url);
    QString makeNovelInfoBlock(CStringHash* authors,
                               const QString& workId, const QString& workImgUrl,
                               const QString& title, int length,
                               const QString& author, const QString& authorId,
                               const QStringList& tags, const QString& description,
                               const QDateTime &creationDate, const QString &seriesTitle,
                               const QString &seriesId, int bookmarkCount);


public Q_SLOTS:
    void tableSelectionChanged(const QModelIndex &current, const QModelIndex &previous);
    void itemActivated(const QModelIndex &index);
    void coverUpdated(const QModelIndex &index, const QImage &cover);
    void tableContextMenu(const QPoint &pos);
    void htmlReport();
    void saveToFile();
    void linkClicked(const QString& link);
    void processExtractorAction();
    void startTitlesAndTagsTranslation();
    void gotTitlesAndTagsTranslation(const QStringList &res);
    void updateTranslatorProgress(int pos);

private Q_SLOTS:
    void modelSorted(const QList<QPersistentModelIndex> &parents, QAbstractItemModel::LayoutChangeHint hint);
    void comboSortChanged(int index);
    void sortReverseClicked();
    void filterChanged(const QString& filter);
    void mangaSettingsChanged();

Q_SIGNALS:
    void translateTitlesAndTags(const QStringList &titles);

};

class CPixivIndexModel : public QAbstractTableModel
{
    Q_OBJECT
    Q_DISABLE_COPY(CPixivIndexModel)

private:
    int m_maxRowCount { -1 };
    QJsonArray m_list;
    QStringList m_tags;
    QStringList m_translatedTags;
    CStringHash m_authors;
    mutable QSize m_cachedPixmapSize;
    mutable QHash<int,QImage> m_cachedCovers;
    QPointer<CPixivIndexTab> m_tab;

    void updateTags();
    void fetchCover(const QModelIndex &index, const QUrl &url);
    void printCoverInfo(QPainter *painter, const QModelIndex &index) const;

public:
    CPixivIndexModel(QObject *parent, CPixivIndexTab *tab, const QJsonArray &list);
    ~CPixivIndexModel() override;

    CStringHash authors() const;
    bool isEmpty() const;
    int count() const;
    QJsonObject item(const QModelIndex& index) const;
    QString tag(const QModelIndex& index) const;
    QString text(const QModelIndex& index) const;
    QImage cover(const QModelIndex& index) const;
    QStringList getStringsForTranslation() const;
    void setStringsFromTranslation(const QStringList& translated);
    QStringList getTags() const;
    QStringList basicHeaders() const;
    QString getTagForColumn(int column, int *tagNumber = nullptr) const;
    int getColumnForTag(const QString& tag) const;
    void setCoverImage(const QModelIndex &idx, const QString& data);
    QJsonArray toJsonArray() const;
    void overrideRowCount(int maxRowCount = -1);
    QSize preferredGridSize(int iconSize) const;
protected:
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

Q_SIGNALS:
    void coverUpdated(const QModelIndex &index, const QImage &cover);

};

#endif // PIXIVINDEXTAB_H
