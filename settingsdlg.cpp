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
    connect(ui->buttonFontColorOverride,SIGNAL(clicked()),this,SLOT(fontColorDlg()));
    connect(ui->buttonAddDictPath,SIGNAL(clicked()),this,SLOT(addDictPath()));
    connect(ui->buttonDelDictPath,SIGNAL(clicked()),this,SLOT(delDictPath()));
    connect(ui->buttonLoadedDicts,SIGNAL(clicked()),this,SLOT(showLoadedDicts()));
    connect(ui->buttonClearCookies,SIGNAL(clicked()),this,SLOT(clearCookies()));
    connect(ui->buttonEditBookmark,SIGNAL(clicked()),this,SLOT(editBkm()));

#ifndef WEBENGINE_56
    ui->buttonClearCookies->setEnabled(false);
#endif
}

CSettingsDlg::~CSettingsDlg()
{
    delete ui;
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
    QString s = QInputDialog::getText(this,tr("Add adblock filter"),tr("Filter template"),
                                      QLineEdit::Normal,"",&ok);
    if (!ok) return;
    ui->listAdblock->addItem(s);
}

void CSettingsDlg::delAd()
{
    QList<QListWidgetItem *> dl = ui->listAdblock->selectedItems();
    foreach (QListWidgetItem* i, dl) {
        ui->listAdblock->removeItemWidget(i);
        delete i;
    }
}

void CSettingsDlg::importAd()
{
    QString fname = getOpenFileNameD(this,tr("Import rules from text file"),QDir::homePath());
    if (fname.isEmpty()) return;

    QFile ini(fname);
    if (!ini.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this,tr("JPReader"),tr("Unable to open file"));
        return;
    }
    QTextStream inird(&ini);
    QStringList lnks;
    lnks.clear();
    while (!inird.atEnd()) {
        QString s = inird.readLine();
        if (!s.isEmpty())
            lnks << s;
    }
    ini.close();
    ui->listAdblock->addItems(lnks);
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
#endif
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

void CSettingsDlg::setAdblock(QStringList adblock)
{
    ui->listAdblock->clear();
    ui->listAdblock->addItems(adblock);
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

QStringList CSettingsDlg::getAdblock()
{
    QStringList sl;
    sl.clear();
    for (int i=0;i<ui->listAdblock->count();i++)
        sl << ui->listAdblock->item(i)->text();
    sl.sort();
    return sl;
}
