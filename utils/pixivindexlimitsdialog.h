#ifndef PIXIVINDEXLIMITSDIALOG_H
#define PIXIVINDEXLIMITSDIALOG_H

#include <QDialog>
#include "extractors/pixivindexextractor.h"

namespace Ui {
    class CPixivIndexLimitsDialog;
}

class CPixivIndexLimitsDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY(CPixivIndexLimitsDialog)
private:
    Ui::CPixivIndexLimitsDialog *ui;

public:
    explicit CPixivIndexLimitsDialog(QWidget *parent = nullptr);
    ~CPixivIndexLimitsDialog() override;

    void setParams(CPixivIndexExtractor::ExtractorMode exMode,
                   bool isTagSearch,
                   int maxCount,
                   const QDate &dateFrom,
                   const QDate &dateTo,
                   const QString &keywords,
                   CPixivIndexExtractor::TagSearchMode tagMode,
                   bool originalOnly,
                   CStructures::PixivFetchCoversMode fetchCovers,
                   const QString &languageCode,
                   CPixivIndexExtractor::NovelSearchLength novelLength,
                   CPixivIndexExtractor::SearchRating novelRating,
                   CPixivIndexExtractor::ArtworkSearchType artworkType,
                   CPixivIndexExtractor::ArtworkSearchSize artworkSize,
                   CPixivIndexExtractor::ArtworkSearchRatio artworkRatio,
                   const QString &artworkCreationTool,
                   bool hideAIWorks,
                   CPixivIndexExtractor::NovelSearchLengthMode novelLengthMode);

    void getParams(int &maxCount,
                   QDate &dateFrom,
                   QDate &dateTo,
                   QString &keywords,
                   CPixivIndexExtractor::TagSearchMode &tagMode,
                   bool &originalOnly,
                   CStructures::PixivFetchCoversMode &fetchCovers,
                   QString &languageCode,
                   CPixivIndexExtractor::NovelSearchLength &novelLength,
                   CPixivIndexExtractor::SearchRating &novelRating,
                   CPixivIndexExtractor::ArtworkSearchType &artworkType,
                   CPixivIndexExtractor::ArtworkSearchSize &artworkSize,
                   CPixivIndexExtractor::ArtworkSearchRatio &artworkRatio,
                   QString &artworkCreationTool,
                   bool &hideAIWorks,
                   CPixivIndexExtractor::NovelSearchLengthMode &novelLengthMode);
};

#endif // PIXIVINDEXLIMITSDIALOG_H
