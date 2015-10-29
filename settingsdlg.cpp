#include <QInputDialog>
#include <QMessageBox>
#include <QColorDialog>

#ifdef WEBENGINE_56
#include <QWebEngineCookieStoreClient>
#endif

#include "settingsdlg.h"
#include "ui_settingsdlg.h"
#include "mainwindow.h"
#include "globalcontrol.h"
#include "genericfuncs.h"
#include "multiinputdialog.h"

CSettingsDlg::CSettingsDlg(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDlg)
{
    ui->setupUi(this);

    adblockSearchIdx = 0;

    hostingUrl=ui->editHostingUrl;
    hostingDir=ui->editHostingDir;
    maxLimit=ui->spinMaxLimit;
    maxHistory=ui->spinMaxHistory;
    browser=ui->editBrowser;
    editor=ui->editEditor;
    rbGoogle=ui->radioGoogle;
    cbSCP=ui->checkSCP;
    scpParams=ui->editScpParams;
    scpHost=ui->editScpHost;
    atlHost=ui->atlHost;
    atlPort=ui->atlPort;
    atlRetryCount=ui->atlRetryCnt;
    atlRetryTimeout=ui->atlRetryTimeout;
    rbAtlas=ui->radioAtlas;
    maxRecycled=ui->spinMaxRecycled;
    useJS=ui->checkJS;
    useAd=ui->checkUseAd;
    useOverrideFont=ui->checkTransFont;
    fontOverride=ui->transFont;
    fontOverrideSize=ui->transFontSize;
    autoloadImages=ui->checkAutoloadImages;
    enablePlugins=ui->checkEnablePlugins;
    overrideStdFonts=ui->checkStdFonts;
    fontStandard=ui->fontStandard;
    fontFixed=ui->fontFixed;
    fontSerif=ui->fontSerif;
    fontSansSerif=ui->fontSansSerif;
    overrideFontColor=ui->checkFontColorOverride;
    overridedFontColor=Qt::black;
    gctxHotkey=ui->gctxHotkey;
    searchRecoll=ui->radioSearchRecoll;
    searchNone=ui->radioSearchNone;
    searchBaloo5=ui->radioSearchBaloo5;
    debugLogNetReq=ui->checkLogNetworkRequests;
    debugDumpHtml=ui->checkDumpHtml;
    visualShowTabCloseButtons=ui->checkTabCloseBtn;
    proxyHost=ui->editProxyHost;
    proxyLogin=ui->editProxyLogin;
    proxyPassword=ui->editProxyPassword;
    proxyPort=ui->spinProxyPort;
    proxyUse=ui->checkUseProxy;
    proxyType=ui->listProxyType;
    proxyUseTranslator=ui->checkUseProxyTranslator;
    emptyRestore=ui->checkEmptyRestore;
    rbBingAPI=ui->radioBingAPI;
    bingID=ui->editBingID;
    bingKey=ui->editBingKey;
    createCoredumps=ui->checkCreateCoredumps;
    overrideUserAgent=ui->checkUserAgent;
    userAgent=ui->editUserAgent;
    dictPaths=ui->listDictPaths;
    ignoreSSLErrors=ui->checkIgnoreSSLErrors;
    visualShowFavicons=ui->checkTabFavicon;
    rbYandexAPI=ui->radioYandexAPI;
    yandexKey=ui->editYandexKey;
    cacheSize=ui->spinCacheSize;
    jsLogConsole=ui->checkJSLogConsole;

    cookiesList.clear();

    connect(ui->buttonHostingDir,SIGNAL(clicked()),this,SLOT(selectDir()));
    connect(ui->buttonBrowser,SIGNAL(clicked()),this,SLOT(selectBrowser()));
    connect(ui->buttonEditor,SIGNAL(clicked()),this,SLOT(selectEditor()));
	connect(ui->buttonDelQr,SIGNAL(clicked()),this,SLOT(delQrs()));
	connect(ui->buttonDelBookmark,SIGNAL(clicked()),this,SLOT(delBkm()));
    connect(ui->buttonCleanHistory,SIGNAL(clicked()),this,SLOT(clearHistory()));
    connect(ui->buttonGoHistory,SIGNAL(clicked()),this,SLOT(goHistory()));
    connect(ui->buttonAdAdd,SIGNAL(clicked()),this,SLOT(addAd()));
    connect(ui->buttonAdDel,SIGNAL(clicked()),this,SLOT(delAd()));
    connect(ui->buttonAdImport,SIGNAL(clicked()),this,SLOT(importAd()));
    connect(ui->buttonAdExport,SIGNAL(clicked()),this,SLOT(exportAd()));
    connect(ui->buttonFontColorOverride,SIGNAL(clicked()),this,SLOT(fontColorDlg()));
    connect(ui->buttonAddDictPath,SIGNAL(clicked()),this,SLOT(addDictPath()));
    connect(ui->buttonDelDictPath,SIGNAL(clicked()),this,SLOT(delDictPath()));
    connect(ui->buttonLoadedDicts,SIGNAL(clicked()),this,SLOT(showLoadedDicts()));
    connect(ui->buttonClearCookies,SIGNAL(clicked()),this,SLOT(clearCookies()));
    connect(ui->buttonEditBookmark,SIGNAL(clicked()),this,SLOT(editBkm()));
    connect(ui->buttonDelCookies,SIGNAL(clicked()),this,SLOT(delCookies()));
    connect(ui->buttonExportCookies,SIGNAL(clicked()),this,SLOT(exportCookies()));
    connect(ui->editAdSearch,SIGNAL(textChanged(QString)),this,SLOT(adblockSearch(QString)));
    connect(ui->buttonAdSearchBwd,SIGNAL(clicked()),this,SLOT(adblockSearchBwd()));
    connect(ui->buttonAdSearchFwd,SIGNAL(clicked()),this,SLOT(adblockSearchFwd()));

#ifndef WEBENGINE_56
    QString we56 = tr("QtWebEngine 5.6 or above need for this feature");
    ui->groupBoxProxy->setEnabled(false);
    ui->groupBoxProxy->setToolTip(we56);

    ui->treeAdblock->setEnabled(false);
    ui->treeAdblock->setToolTip(we56);
    ui->buttonAdAdd->setEnabled(false);
    ui->buttonAdDel->setEnabled(false);
    ui->buttonAdImport->setEnabled(false);
    ui->buttonAdExport->setEnabled(false);
    ui->checkUseAd->setEnabled(false);

    ui->tableCookies->setEnabled(false);
    ui->tableCookies->setToolTip(we56);
    ui->buttonDelCookies->setEnabled(false);
    ui->buttonExportCookies->setEnabled(false);
    ui->buttonClearCookies->setEnabled(false);
#endif
}

CSettingsDlg::~CSettingsDlg()
{
    delete ui;
}

void CSettingsDlg::updateCookiesTable()
{
    QTableWidget *table = ui->tableCookies;

    table->setSortingEnabled(false);
    table->clear();
    table->setColumnCount(5);
    table->setRowCount(cookiesList.count());
    table->setHorizontalHeaderLabels(QStringList() << tr("Domain") << tr("Name")
                                 << tr("Path") << tr("Expiration") << tr("Value"));
    for (int i=0;i<cookiesList.count();i++) {

        QString s = cookiesList.at(i).domain();
        QTableWidgetItem* item = new QTableWidgetItem(s);
        item->setData(Qt::UserRole+1,i);
        table->setItem(i,0,item);

        s = QString::fromUtf8(cookiesList.at(i).name());
        item = new QTableWidgetItem(s);
        item->setData(Qt::UserRole+1,i);
        table->setItem(i,1,item);

        s = cookiesList.at(i).path();
        item = new QTableWidgetItem(s);
        item->setData(Qt::UserRole+1,i);
        table->setItem(i,2,item);

        s = cookiesList.at(i).expirationDate().toString(Qt::DefaultLocaleShortDate);
        item = new QTableWidgetItem(s);
        item->setData(Qt::UserRole+1,i);
        table->setItem(i,3,item);

        s = QString::fromUtf8(cookiesList.at(i).value());
        item = new QTableWidgetItem(s);
        item->setData(Qt::UserRole+1,i);
        table->setItem(i,4,item);
    }
    table->resizeColumnToContents(0);
    table->resizeColumnToContents(1);
    table->resizeColumnToContents(2);
    table->resizeColumnToContents(3);
    table->setSortingEnabled(true);
}

void CSettingsDlg::updateAdblockList()
{
    ui->treeAdblock->setHeaderLabels(QStringList() << tr("AdBlock patterns"));
    QHash<QString,QTreeWidgetItem*> cats;
    ui->treeAdblock->clear();
    for (int i=0;i<adblockList.count();i++) {
        QTreeWidgetItem* tli = NULL;
        QString cat = adblockList.at(i).listID();
        if (cats.keys().contains(cat))
            tli = cats.value(cat);
        else {
            tli = new QTreeWidgetItem(ui->treeAdblock);
            tli->setText(0,cat);
            cats[cat] = tli;
        }

        QTreeWidgetItem *item = new QTreeWidgetItem(tli);
        item->setText(0,adblockList.at(i).filter());
        item->setData(0,Qt::UserRole+1,i);
    }
}

void CSettingsDlg::selectDir()
{
    QString dir = getExistingDirectoryD(this,tr("Hosting directory"),ui->editHostingDir->text());
    if (!dir.isEmpty()) ui->editHostingDir->setText(dir);
}

void CSettingsDlg::selectBrowser()
{
    QString s = getOpenFileNameD(this,tr("Select browser"),ui->editBrowser->text());
    if (!s.isEmpty()) ui->editBrowser->setText(s);
}

void CSettingsDlg::selectEditor()
{
    QString s = getOpenFileNameD(this,tr("Select editor"),ui->editEditor->text());
    if (!s.isEmpty()) ui->editEditor->setText(s);
}

void CSettingsDlg::delQrs()
{
    QList<QListWidgetItem *> dl = ui->listQr->selectedItems();
	foreach (QListWidgetItem* i, dl) {
        ui->listQr->removeItemWidget(i);
		delete i;
	}
}

void CSettingsDlg::delBkm()
{
    QList<QListWidgetItem *> dl = ui->listBookmarks->selectedItems();
	foreach (QListWidgetItem* i, dl) {
        ui->listBookmarks->removeItemWidget(i);
		delete i;
    }
}

void CSettingsDlg::editBkm()
{
    QList<QListWidgetItem *> dl = ui->listBookmarks->selectedItems();
    if (dl.isEmpty()) return;

    QStrHash data;
    data["Page title"]=dl.first()->data(Qt::UserRole).toString();
    data["Url"]=dl.first()->data(Qt::UserRole+1).toUrl().toString();

    CMultiInputDialog *dlg = new CMultiInputDialog(this,tr("Edit bookmark"),data);
    if (dlg->exec()) {
        data = dlg->getInputData();
        dl.first()->setData(Qt::UserRole,data["Page title"]);
        dl.first()->setData(Qt::UserRole+1,QUrl(data["Url"]));
        dl.first()->setText(QString("%1 [ %2 ]").
                            arg(data["Page title"]).
                            arg(data["Url"]));
    }
    dlg->deleteLater();
}

void CSettingsDlg::clearHistory()
{
    if (gSet!=NULL) {
        gSet->mainHistory.clear();
        gSet->updateAllHistoryLists();
    }
    ui->listHistory->clear();
}

void CSettingsDlg::goHistory()
{
    QListWidgetItem* it = ui->listHistory->currentItem();
    if (it==NULL || gSet->activeWindow==NULL) return;
    QUuid idx(it->data(Qt::UserRole).toString());
    gSet->activeWindow->goHistory(idx);
}

void CSettingsDlg::addAd()
{
    bool ok;
    QString s = QInputDialog::getText(this,tr("Add AdBlock rule"),tr("Filter template"),
                                      QLineEdit::Normal,"",&ok);
    if (!ok) return;

    adblockList << CAdBlockRule(s,QString());
    updateAdblockList();
}

void CSettingsDlg::delAd()
{
    QList<int> r;
    foreach (QTreeWidgetItem* i, ui->treeAdblock->selectedItems())
        r << i->data(0,Qt::UserRole+1).toInt();

    QList<CAdBlockRule> tmp = adblockList;

    foreach (const int idx, r)
        adblockList.removeOne(tmp.at(idx));

    tmp.clear();

    updateAdblockList();
}

void CSettingsDlg::importAd()
{
    QString fname = getOpenFileNameD(this,tr("Import rules from text file"),QDir::homePath());
    if (fname.isEmpty()) return;

    QFile f(fname);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this,tr("JPReader"),tr("Unable to open file"));
        return;
    }
    QTextStream fs(&f);

    QFileInfo fi(fname);

    int psz = adblockList.count();
    while (!fs.atEnd()) {
        adblockList << CAdBlockRule(fs.readLine(),fi.fileName());
    }
    f.close();

    updateAdblockList();

    psz = adblockList.count() - psz;
    QMessageBox::information(this,tr("JPReader"),tr("%1 rules imported.").arg(psz));
}

void CSettingsDlg::exportAd()
{
    if (ui->treeAdblock->selectedItems().isEmpty()) {
        QMessageBox::information(this,tr("JPReader"),tr("Please select patterns for export."));
        return;
    }

    QList<int> r;
    foreach (QTreeWidgetItem* i, ui->treeAdblock->selectedItems())
        r << i->data(0,Qt::UserRole+1).toInt();

    QString fname = getSaveFileNameD(this,tr("Save AdBlock patterns to file"),gSet->savedAuxSaveDir,
                                     tr("Text file (*.txt)"));

    if (fname.isEmpty() || fname.isNull()) return;
    gSet->savedAuxSaveDir = QFileInfo(fname).absolutePath();

    QFile f(fname);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this,tr("JPReader"),tr("Unable to create file"));
        return;
    }
    QTextStream fs(&f);

    for (int i=0;i<r.count();i++)
        fs << adblockList.at(r.at(i)).filter() << endl;

    fs.flush();
    f.close();
}

void CSettingsDlg::fontColorDlg()
{
    QColor c = QColorDialog::getColor(overridedFontColor,this);
    if (!c.isValid()) return;
    updateFontColorPreview(c);
}

void CSettingsDlg::addDictPath()
{
    QString s = getExistingDirectoryD(this,tr("Select directory"));
    if (!s.isEmpty())
        ui->listDictPaths->addItem(s);
}

void CSettingsDlg::delDictPath()
{
    int idx = ui->listDictPaths->currentRow();
    if (idx<0 || idx>=ui->listDictPaths->count()) return;
    QListWidgetItem *a = ui->listDictPaths->takeItem(idx);
    delete a;
}

void CSettingsDlg::showLoadedDicts()
{
    QString msg = tr("No dictionaries loaded.");
    if (!loadedDicts.isEmpty())
        msg = tr("Loaded %1 dictionaries:\n").arg(loadedDicts.count())+loadedDicts.join('\n');
    QMessageBox::information(this,tr("JPReader"),msg);
}

void CSettingsDlg::clearCookies()
{
#ifdef WEBENGINE_56
    QWebEngineCookieStoreClient* wsc = gSet->webProfile->cookieStoreClient();
    if (wsc!=NULL)
        wsc->deleteAllCookies();

    getCookiesFromStore();
#endif
}

void CSettingsDlg::getCookiesFromStore()
{
#ifdef WEBENGINE_56
    if (gSet->webProfile->cookieStoreClient()!=NULL) {
        gSet->webProfile->cookieStoreClient()->getAllCookies([this](const QByteArray& cookies) {
            setCookies(cookies);
        });
    }
#endif
}

void CSettingsDlg::delCookies()
{
#ifdef WEBENGINE_56
    QList<int> r = getSelectedRows(ui->tableCookies);
    if (gSet->webProfile->cookieStoreClient()!=NULL) {
        foreach (const int idx, r)
            gSet->webProfile->cookieStoreClient()->deleteCookie(cookiesList.at(idx));

        getCookiesFromStore();
    }
#endif
}

void CSettingsDlg::exportCookies()
{
#ifdef WEBENGINE_56
    QList<int> r = getSelectedRows(ui->tableCookies);
    if (r.isEmpty()) {
        QMessageBox::information(this,tr("JPReader"),tr("Please select cookies for export."));
        return;
    }

    QString fname = getSaveFileNameD(this,tr("Save cookies to file"),gSet->savedAuxSaveDir,
                                     tr("Text file, Netscape format (*.txt)"));

    if (fname.isEmpty() || fname.isNull()) return;
    gSet->savedAuxSaveDir = QFileInfo(fname).absolutePath();

    QFile f(fname);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this,tr("JPReader"),tr("Unable to create file."));
        return;
    }
    QTextStream fs(&f);

    for (int i=0;i<r.count();i++) {
        int idx = r.at(i);
        fs << cookiesList.at(idx).domain()
           << "\t"
           << bool2str2(cookiesList.at(idx).domain().startsWith("."))
           << "\t"
           << cookiesList.at(idx).path()
           << "\t"
           << bool2str2(cookiesList.at(idx).isSecure())
           << "\t"
           << cookiesList.at(idx).expirationDate().toTime_t()
           << "\t"
           << cookiesList.at(idx).name()
           << "\t"
           << cookiesList.at(idx).value()
           << endl;
    }
    fs.flush();
    f.close();
#endif
}

void CSettingsDlg::adblockSearch(const QString &text)
{
    QString s = text.trimmed();
    if (s.isEmpty()) return;
    QList<QTreeWidgetItem *> it = ui->treeAdblock->findItems(s,Qt::MatchContains | Qt::MatchRecursive);
    if (it.isEmpty()) return;
    adblockSearchIdx = 0;
    adblockFocusSearchedRule(it);
}

void CSettingsDlg::adblockSearchBwd()
{
    QString s = ui->editAdSearch->text();
    if (s.isEmpty()) {
        adblockSearchIdx = 0;
        return;
    }
    QList<QTreeWidgetItem *> it = ui->treeAdblock->findItems(s,Qt::MatchContains | Qt::MatchRecursive);
    if (it.isEmpty()) {
        adblockSearchIdx = 0;
        return;
    }
    if (adblockSearchIdx>0)
        adblockSearchIdx--;
    adblockFocusSearchedRule(it);
}

void CSettingsDlg::adblockSearchFwd()
{
    QString s = ui->editAdSearch->text();
    if (s.isEmpty()) {
        adblockSearchIdx = 0;
        return;
    }
    QList<QTreeWidgetItem *> it = ui->treeAdblock->findItems(s,Qt::MatchContains | Qt::MatchRecursive);
    if (it.isEmpty()) {
        adblockSearchIdx = 0;
        return;
    }
    if (adblockSearchIdx<(it.count()-1))
        adblockSearchIdx++;
    adblockFocusSearchedRule(it);
}

void CSettingsDlg::adblockFocusSearchedRule(QList<QTreeWidgetItem *> items)
{
    if (adblockSearchIdx>=0 && adblockSearchIdx<items.count())
        ui->treeAdblock->setCurrentItem(items.at(adblockSearchIdx));
}

void CSettingsDlg::updateFontColorPreview(const QColor &c)
{
    overridedFontColor = c;
    ui->frameFontColorOverride->setStyleSheet(QString("QFrame { background: %1; }")
                                              .arg(overridedFontColor.name(QColor::HexRgb)));
}

QColor CSettingsDlg::getOverridedFontColor()
{
    return overridedFontColor;
}

void CSettingsDlg::setBookmarks(QBookmarksMap bookmarks)
{
    foreach (const QString &t, bookmarks.keys()) {
        QListWidgetItem* li = new QListWidgetItem(QString("%1 [ %2 ]").
                                                  arg(t).
                                                  arg(bookmarks.value(t).toString()));
        li->setData(Qt::UserRole,t);
        li->setData(Qt::UserRole+1,bookmarks.value(t));
        ui->listBookmarks->addItem(li);
    }
}

void CSettingsDlg::setQueryHistory(QStringList history)
{
    ui->listQr->clear();
    for (int i=0;i<history.count();i++)
        ui->listQr->addItem(history.at(i));
}

void CSettingsDlg::setAdblock(QList<CAdBlockRule> adblock)
{
    adblockList = adblock;
    updateAdblockList();
}

void CSettingsDlg::setMainHistory(QUHList history)
{
    foreach (const UrlHolder &t, history) {
        QListWidgetItem* li = new QListWidgetItem(QString("%1 [ %2 ]").arg(t.title).
                                                  arg(t.url.toString()));
        li->setData(Qt::UserRole,t.uuid.toString());
        ui->listHistory->addItem(li);
    }
}

void CSettingsDlg::setCookies(const QByteArray &cookies)
{
    cookiesList = QNetworkCookie::parseCookies(cookies);
    updateCookiesTable();
}

QList<int> CSettingsDlg::getSelectedRows(QTableWidget *table)
{
    QList<int> res;
    res.clear();
    foreach (const QTableWidgetItem *i, table->selectedItems()) {
        if (!res.contains(i->data(Qt::UserRole+1).toInt()))
            res << i->data(Qt::UserRole+1).toInt();
    }
    return res;
}

QBookmarksMap CSettingsDlg::getBookmarks()
{
    QBookmarksMap bookmarks;
    bookmarks.clear();
    for (int i=0; i<ui->listBookmarks->count(); i++)
        bookmarks[ui->listBookmarks->item(i)->data(Qt::UserRole).toString()] =
                ui->listBookmarks->item(i)->data(Qt::UserRole+1).toUrl();
    return bookmarks;
}

QStringList CSettingsDlg::getQueryHistory()
{
    QStringList sl;
    sl.clear();
    for (int i=0; i<ui->listQr->count();i++)
        sl << ui->listQr->item(i)->text();
    return sl;
}

QList<CAdBlockRule> CSettingsDlg::getAdblock()
{
    return adblockList;
}
