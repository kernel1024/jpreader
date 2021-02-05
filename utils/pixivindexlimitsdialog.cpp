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

    ui->comboMode->addItem(tr("Tags (partial match)"));
    ui->comboMode->addItem(tr("Tags (perfect match)"));
    ui->comboMode->addItem(tr("Text"));
    ui->comboMode->addItem(tr("Tags, titles, captions"));

    ui->comboLength->addItem(tr("All"));
    ui->comboLength->addItem(tr("Flash (less than 5000)"));
    ui->comboLength->addItem(tr("Short (5000 - 20000)"));
    ui->comboLength->addItem(tr("Medium (20000 - 80000)"));
    ui->comboLength->addItem(tr("Long (80000+)"));

    ui->comboRating->addItem(tr("All"));
    ui->comboRating->addItem(tr("Safe"));
    ui->comboRating->addItem(tr("R-18"));
}

CPixivIndexLimitsDialog::~CPixivIndexLimitsDialog()
{
    delete ui;

}

void CPixivIndexLimitsDialog::setParams(const QString &title, const QString &groupTitle, bool isTagSearch,
                                        int maxCount, const QDate &dateFrom, const QDate &dateTo,
                                        const QString &keywords, CPixivIndexExtractor::TagSearchMode tagMode,
                                        bool originalOnly, bool fetchCovers, const QString &languageCode,
                                        CPixivIndexExtractor::NovelSearchLength novelLength,
                                        CPixivIndexExtractor::NovelSearchRating novelRating)
{
    setWindowTitle(title);
    ui->groupBox->setTitle(groupTitle);

    ui->comboLanguage->setEnabled(isTagSearch);
    ui->comboMode->setEnabled(isTagSearch);
    ui->comboLength->setEnabled(isTagSearch);
    ui->checkOriginalOnly->setEnabled(isTagSearch);
    ui->editKeywords->setEnabled(isTagSearch);

    ui->comboMode->setCurrentIndex(static_cast<int>(tagMode));
    ui->comboLength->setCurrentIndex(static_cast<int>(novelLength));
    ui->comboRating->setCurrentIndex(static_cast<int>(novelRating));
    ui->comboLanguage->setCurrentIndex(0);
    if (!languageCode.isEmpty()) {
        int idx = ui->comboLanguage->findData(QVariant::fromValue(languageCode));
        if (idx>0) ui->comboLanguage->setCurrentIndex(idx);
    }

    ui->editKeywords->clear();
    ui->editKeywords->addItems(gSet->history()->pixivKeywords());
    ui->editKeywords->setCurrentText(keywords);

    ui->checkOriginalOnly->setChecked(originalOnly);
    ui->checkFetchCovers->setChecked(fetchCovers);
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
                                        CPixivIndexExtractor::TagSearchMode &tagMode, bool &originalOnly, bool &fetchCovers,
                                        QString &languageCode, CPixivIndexExtractor::NovelSearchLength &novelLength,
                                        CPixivIndexExtractor::NovelSearchRating &novelRating)
{
    keywords = ui->editKeywords->currentText();
    originalOnly = ui->checkOriginalOnly->isChecked();
    fetchCovers = ui->checkFetchCovers->isChecked();
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
    novelRating = static_cast<CPixivIndexExtractor::NovelSearchRating>(ui->comboRating->currentIndex());

    gSet->history()->appendPixivKeywords(keywords);
}
