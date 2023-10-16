#include <QLineEdit>
#include "global/control.h"
#include "global/network.h"
#include "global/history.h"
#include "pixivindexlimitsdialog.h"
#include "ui_pixivindexlimitsdialog.h"

CPixivIndexLimitsDialog::CPixivIndexLimitsDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::CPixivIndexLimitsDialog)
{
    ui->setupUi(this);

    ui->comboLanguage->addItem(tr("Any language"),QVariant::fromValue(QString()));
    const QStringList languages = gSet->net()->getLanguageCodes();
    for (const auto &bcp : languages) {
        ui->comboLanguage->addItem(gSet->net()->getLanguageName(bcp),QVariant::fromValue(bcp));
    }
}

CPixivIndexLimitsDialog::~CPixivIndexLimitsDialog()
{
    delete ui;

}

void CPixivIndexLimitsDialog::setParams(CPixivIndexExtractor::ExtractorMode exMode,
                                        bool isTagSearch,
                                        int maxCount, const QDate &dateFrom, const QDate &dateTo,
                                        const QString &keywords, CPixivIndexExtractor::TagSearchMode tagMode,
                                        bool originalOnly, CStructures::PixivFetchCoversMode fetchCovers,
                                        const QString &languageCode,
                                        CPixivIndexExtractor::NovelSearchLength novelLength,
                                        CPixivIndexExtractor::SearchRating novelRating,
                                        CPixivIndexExtractor::ArtworkSearchType artworkType,
                                        CPixivIndexExtractor::ArtworkSearchSize artworkSize,
                                        CPixivIndexExtractor::ArtworkSearchRatio artworkRatio,
                                        const QString &artworkCreationTool,
                                        bool hideAIWorks,
                                        CPixivIndexExtractor::NovelSearchLengthMode novelLengthMode)
{
    ui->exModeStack->setCurrentIndex((exMode == CPixivIndexExtractor::emNovels) ? 0 : 1);

    ui->comboLanguage->setEnabled(isTagSearch);
    ui->comboMode->setEnabled(isTagSearch);
    ui->comboLength->setEnabled(isTagSearch);
    ui->checkOriginalOnly->setEnabled(isTagSearch);
    ui->editKeywords->setEnabled(isTagSearch);

    ui->comboMode->setCurrentIndex(static_cast<int>(tagMode));
    ui->comboLength->setCurrentIndex(static_cast<int>(novelLength));
    ui->comboRating->setCurrentIndex(static_cast<int>(novelRating));
    ui->comboFetchCovers->setCurrentIndex(static_cast<int>(fetchCovers));
    ui->comboLengthMode->setCurrentIndex(static_cast<int>(novelLengthMode));

    ui->checkHideAIWork->setChecked(hideAIWorks);

    ui->comboArtworkType->setCurrentIndex(static_cast<int>(artworkType));
    ui->comboArtworkSize->setCurrentIndex(static_cast<int>(artworkSize));
    ui->comboArtworkRatio->setCurrentIndex(static_cast<int>(artworkRatio));
    ui->editArtworkTool->setEditText(artworkCreationTool);

    ui->comboLanguage->setCurrentIndex(0);
    if (!languageCode.isEmpty()) {
        int idx = ui->comboLanguage->findData(QVariant::fromValue(languageCode));
        if (idx>0) ui->comboLanguage->setCurrentIndex(idx);
    }

    ui->editKeywords->clear();
    ui->editKeywords->addItems(gSet->history()->pixivKeywords());
    ui->editKeywords->setCurrentText(keywords);

    ui->checkOriginalOnly->setChecked(originalOnly);
    ui->spinMaxCount->setValue(maxCount);
    if (dateFrom.isValid()) {
        ui->dateFrom->setDate(dateFrom);
        ui->checkDateFrom->setChecked(true);
    } else {
        ui->dateFrom->setDate(QDate::currentDate());
    }
    if (dateTo.isValid()) {
        ui->dateTo->setDate(dateTo);
        ui->checkDateTo->setChecked(true);
    } else {
        ui->dateTo->setDate(QDate::currentDate());
    }
}

void CPixivIndexLimitsDialog::getParams(int &maxCount, QDate &dateFrom, QDate &dateTo, QString &keywords,
                                        CPixivIndexExtractor::TagSearchMode &tagMode, bool &originalOnly,
                                        CStructures::PixivFetchCoversMode &fetchCovers,
                                        QString &languageCode,
                                        CPixivIndexExtractor::NovelSearchLength &novelLength,
                                        CPixivIndexExtractor::SearchRating &novelRating,
                                        CPixivIndexExtractor::ArtworkSearchType &artworkType,
                                        CPixivIndexExtractor::ArtworkSearchSize &artworkSize,
                                        CPixivIndexExtractor::ArtworkSearchRatio &artworkRatio,
                                        QString &artworkCreationTool,
                                        bool &hideAIWorks,
                                        CPixivIndexExtractor::NovelSearchLengthMode &novelLengthMode)
{
    keywords = ui->editKeywords->currentText();
    originalOnly = ui->checkOriginalOnly->isChecked();
    maxCount = ui->spinMaxCount->value();

    if (ui->checkDateFrom->isChecked()) {
        dateFrom = ui->dateFrom->date();
    } else {
        dateFrom = QDate();
    }
    if (ui->checkDateTo->isChecked()) {
        dateTo = ui->dateTo->date();
    } else {
        dateTo = QDate();
    }

    languageCode = ui->comboLanguage->currentData().toString();
    tagMode = static_cast<CPixivIndexExtractor::TagSearchMode>(ui->comboMode->currentIndex());
    novelLength = static_cast<CPixivIndexExtractor::NovelSearchLength>(ui->comboLength->currentIndex());
    novelRating = static_cast<CPixivIndexExtractor::SearchRating>(ui->comboRating->currentIndex());
    fetchCovers = static_cast<CStructures::PixivFetchCoversMode>(ui->comboFetchCovers->currentIndex());
    novelLengthMode = static_cast<CPixivIndexExtractor::NovelSearchLengthMode>(
        ui->comboLengthMode->currentIndex());

    hideAIWorks = ui->checkHideAIWork->isChecked();

    artworkType = static_cast<CPixivIndexExtractor::ArtworkSearchType>(ui->comboArtworkType->currentIndex());
    artworkSize = static_cast<CPixivIndexExtractor::ArtworkSearchSize>(ui->comboArtworkSize->currentIndex());
    artworkRatio = static_cast<CPixivIndexExtractor::ArtworkSearchRatio>(ui->comboArtworkRatio->currentIndex());
    artworkCreationTool = ui->editArtworkTool->lineEdit()->text();

    gSet->history()->appendPixivKeywords(keywords);
}
