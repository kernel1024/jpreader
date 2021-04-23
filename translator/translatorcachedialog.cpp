#include <QJsonDocument>
#include <QJsonObject>

#include "translatorcachedialog.h"
#include "translatorcache.h"
#include "global/control.h"
#include "global/network.h"
#include "browser/browser.h"
#include "utils/specwidgets.h"
#include "ui_translatorcachedialog.h"

CTranslatorCacheDialog::CTranslatorCacheDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::CTranslatorCacheDialog)
{
    ui->setupUi(this);

    connect(ui->buttonClearCache,&QPushButton::clicked,[this](){
        gSet->net()->clearTranslatorCache();
        updateCacheList();
    });

    connect(ui->table,&QTableWidget::itemActivated,this,&CTranslatorCacheDialog::tableItemActivated);

    updateCacheList();
}

CTranslatorCacheDialog::~CTranslatorCacheDialog()
{
    delete ui;
}

void CTranslatorCacheDialog::updateCacheList()
{
    const QStringList tableHeaders({ tr("Title"), tr("Date"), tr("URL"), tr("Length"),
                                   tr("Engine"), tr("Direction") });

    ui->table->clear();
    ui->labelCount->clear();

    const QDir cachePath = gSet->translatorCache()->getCachePath();
    if (!cachePath.exists()) return;
    const QFileInfoList list = cachePath.entryInfoList(
                                   { QSL("*.info") },
                                   QDir::Files | QDir::Writable | QDir::Readable);
    QList<QJsonObject> infoList;
    for (const auto &file : list) {
        QFile f(file.absoluteFilePath());
        if (f.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            if (!doc.isNull() && doc.isObject()) {
                QJsonObject obj = doc.object();
                obj.insert(QSL("#filedate"),file.lastModified().toString(Qt::ISODateWithMs));
                obj.insert(QSL("#md5"),file.baseName());
                infoList.append(obj);
            }
        }
    }

    ui->table->setRowCount(infoList.count());
    ui->table->setColumnCount(tableHeaders.count());
    ui->table->setHorizontalHeaderLabels(tableHeaders);
    int row = 0;
    for (const auto &item : qAsConst(infoList)) {
        auto *itm = new QTableWidgetItem(item.value(QSL("title")).toString());
        itm->setData(Qt::UserRole,item.value(QSL("#md5")).toString());
        ui->table->setItem(row,0,itm);
        ui->table->setItem(row,1,new CDateTimeTableWidgetItem(
                               QDateTime::fromString(item.value(QSL("#filedate")).toString(),Qt::ISODateWithMs)));
        ui->table->setItem(row,2,new QTableWidgetItem(item.value(QSL("origin")).toString(tr("(none)"))));
        ui->table->setItem(row,3,new QTableWidgetItem(QSL("%1").arg(item.value(QSL("length")).toInt())));
        ui->table->setItem(row,4,new QTableWidgetItem(item.value(QSL("engine")).toString()));
        ui->table->setItem(row,5,new QTableWidgetItem(item.value(QSL("langpair")).toString()));
        row++;
    }
    ui->table->resizeColumnsToContents();

    ui->labelCount->setText(tr("%1 cache entries found.").arg(infoList.count()));
}

void CTranslatorCacheDialog::tableItemActivated(QTableWidgetItem *item)
{
    if (item == nullptr) return;
    int row = item->row();
    QTableWidgetItem *itm = ui->table->item(row,0);
    if (itm == nullptr) return;
    const QString md5 = itm->data(Qt::UserRole).toString();

    const QString html = gSet->translatorCache()->cachedTranslatorResult(md5);
    if (!html.isEmpty()) {
        auto *snv = new CBrowserTab(gSet->activeWindow(),QUrl(),QStringList(),true,html);
        snv->setTranslationRestriction();
    }
}
