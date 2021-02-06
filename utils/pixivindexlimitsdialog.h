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
private:
    Ui::CPixivIndexLimitsDialog *ui;

public:
    explicit CPixivIndexLimitsDialog(QWidget *parent = nullptr);
    ~CPixivIndexLimitsDialog() override;

    void setParams(const QString& title,
                   const QString& groupTitle, bool isTagSearch,
                   int maxCount, const QDate &dateFrom, const QDate &dateTo, const QString &keywords,
                   CPixivIndexExtractor::TagSearchMode tagMode, bool originalOnly,
                   CStructures::PixivFetchCoversMode fetchCovers, const QString &languageCode,
                   CPixivIndexExtractor::NovelSearchLength novelLength,
                   CPixivIndexExtractor::NovelSearchRating novelRating);

    void getParams(int &maxCount, QDate &dateFrom, QDate &dateTo, QString &keywords,
                   CPixivIndexExtractor::TagSearchMode &tagMode, bool &originalOnly,
                   CStructures::PixivFetchCoversMode &fetchCovers, QString &languageCode,
                   CPixivIndexExtractor::NovelSearchLength &novelLength,
                   CPixivIndexExtractor::NovelSearchRating &novelRating);

};

#endif // PIXIVINDEXLIMITSDIALOG_H
