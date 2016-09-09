#include <QInputDialog>
#include <QMessageBox>
#include <QColorDialog>

#include <QWebEngineCookieStore>

#include "settingsdlg.h"
#include "ui_settingsdlg.h"
#include "mainwindow.h"
#include "globalcontrol.h"
#include "genericfuncs.h"
#include "multiinputdialog.h"
#include "userscript.h"
#include "ui_userscriptdlg.h"

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
    atlToken = ui->atlToken;
    atlSSLProto = ui->atlSSLProto;

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
    maxRecent=ui->spinMaxRecent;
    maxBookmarksCnt=ui->spinMaxBookmarksCnt;
    dontUseNativeFileDialogs=ui->checkDontUseNativeFileDialogs;

    cookiesList.clear();

    connect(ui->buttonHostingDir, &QPushButton::clicked, this, &CSettingsDlg::selectDir);
    connect(ui->buttonBrowser, &QPushButton::clicked, this, &CSettingsDlg::selectBrowser);
    connect(ui->buttonEditor, &QPushButton::clicked, this, &CSettingsDlg::selectEditor);
    connect(ui->buttonDelQr, &QPushButton::clicked, this, &CSettingsDlg::delQrs);
    connect(ui->buttonDelBookmark, &QPushButton::clicked, this, &CSettingsDlg::delBkm);
    connect(ui->buttonCleanHistory, &QPushButton::clicked, this, &CSettingsDlg::clearHistory);
    connect(ui->buttonGoHistory, &QPushButton::clicked, this, &CSettingsDlg::goHistory);
    connect(ui->buttonAdAdd, &QPushButton::clicked, this, &CSettingsDlg::addAd);
    connect(ui->buttonAdDel, &QPushButton::clicked, this, &CSettingsDlg::delAd);
    connect(ui->buttonAdImport, &QPushButton::clicked, this, &CSettingsDlg::importAd);
    connect(ui->buttonAdExport, &QPushButton::clicked, this, &CSettingsDlg::exportAd);
    connect(ui->buttonFontColorOverride, &QPushButton::clicked, this, &CSettingsDlg::fontColorDlg);
    connect(ui->buttonAddDictPath, &QPushButton::clicked, this, &CSettingsDlg::addDictPath);
    connect(ui->buttonDelDictPath, &QPushButton::clicked, this, &CSettingsDlg::delDictPath);
    connect(ui->buttonLoadedDicts, &QPushButton::clicked, this, &CSettingsDlg::showLoadedDicts);
    connect(ui->buttonClearCookies, &QPushButton::clicked, this, &CSettingsDlg::clearCookies);
    connect(ui->buttonEditBookmark, &QPushButton::clicked, this, &CSettingsDlg::editBkm);
    connect(ui->buttonDelCookies, &QPushButton::clicked, this, &CSettingsDlg::delCookies);
    connect(ui->buttonExportCookies, &QPushButton::clicked, this, &CSettingsDlg::exportCookies);
    connect(ui->buttonAdSearchBwd, &QPushButton::clicked, this, &CSettingsDlg::adblockSearchBwd);
    connect(ui->buttonAdSearchFwd, &QPushButton::clicked, this, &CSettingsDlg::adblockSearchFwd);
    connect(ui->buttonAddSearch, &QPushButton::clicked, this, &CSettingsDlg::addSearchEngine);
    connect(ui->buttonDelSearch, &QPushButton::clicked, this, &CSettingsDlg::delSearchEngine);
    connect(ui->buttonAtlClean, &QPushButton::clicked, this, &CSettingsDlg::cleanAtlCerts);
    connect(ui->buttonDefaultSearch, &QPushButton::clicked, this, &CSettingsDlg::setDefaultSearch);
    connect(ui->buttonUserScriptAdd, &QPushButton::clicked, this, &CSettingsDlg::addUserScript);
    connect(ui->buttonUserScriptEdit, &QPushButton::clicked, this, &CSettingsDlg::editUserScript);
    connect(ui->buttonUserScriptDelete, &QPushButton::clicked, this, &CSettingsDlg::deleteUserScript);
    connect(ui->buttonUserScriptImport, &QPushButton::clicked, this, &CSettingsDlg::importUserScript);

    connect(ui->editAdSearch, &QLineEdit::textChanged, this, &CSettingsDlg::adblockSearch);

    connect(ui->listTabs, &QListWidget::currentRowChanged, ui->tabWidget, &QStackedWidget::setCurrentIndex);

    ui->atlSSLProto->addItem("Secure",static_cast<int>(QSsl::SecureProtocols));
    ui->atlSSLProto->addItem("TLS 1.2",static_cast<int>(QSsl::TlsV1_2));
    ui->atlSSLProto->addItem("TLS 1.1",static_cast<int>(QSsl::TlsV1_1));
    ui->atlSSLProto->addItem("TLS 1.0",static_cast<int>(QSsl::TlsV1_0));
    ui->atlSSLProto->addItem("SSL V3",static_cast<int>(QSsl::SslV3));
    ui->atlSSLProto->addItem("SSL V2",static_cast<int>(QSsl::SslV2));
    ui->atlSSLProto->addItem("Any",static_cast<int>(QSsl::AnyProtocol));
    updateAtlCertLabel();

    populateTabList();
    ui->listTabs->setCurrentRow(0);
}

CSettingsDlg::~CSettingsDlg()
{
    delete ui;
}

void CSettingsDlg::populateTabList()
{
    ui->listTabs->clear();

    QListWidgetItem* itm;

    itm = new QListWidgetItem(QIcon::fromTheme("internet-web-browser"),tr("Browser"));
    ui->listTabs->addItem(itm);
    itm = new QListWidgetItem(QIcon::fromTheme("document-preview"),tr("Search"));
    ui->listTabs->addItem(itm);
    itm = new QListWidgetItem(QIcon::fromTheme("document-edit-decrypt-verify"),tr("Translator"));
    ui->listTabs->addItem(itm);
    itm = new QListWidgetItem(QIcon::fromTheme("format-text-color"),tr("Fonts"));
    ui->listTabs->addItem(itm);
    itm = new QListWidgetItem(QIcon::fromTheme("system-run"),tr("Programs"));
    ui->listTabs->addItem(itm);
    itm = new QListWidgetItem(QIcon(":/img/nepomuk"),tr("Query history"));
    ui->listTabs->addItem(itm);
    itm = new QListWidgetItem(QIcon::fromTheme("bookmarks"),tr("Bookmarks"));
    ui->listTabs->addItem(itm);
    itm = new QListWidgetItem(QIcon::fromTheme("view-history"),tr("History"));
    ui->listTabs->addItem(itm);
    itm = new QListWidgetItem(QIcon::fromTheme("preferences-web-browser-adblock"),tr("AdBlock"));
    ui->listTabs->addItem(itm);
    itm = new QListWidgetItem(QIcon::fromTheme("preferences-web-browser-cookies"),tr("Cookies"));
    ui->listTabs->addItem(itm);
    itm = new QListWidgetItem(QIcon::fromTheme("folder-script"),tr("User scripts"));
    ui->listTabs->addItem(itm);
    itm = new QListWidgetItem(QIcon::fromTheme("server-database"),tr("Network & logging"));
    ui->listTabs->addItem(itm);

    int maxw = 0;
    QFontMetrics fm(ui->listTabs->item(0)->font());
    for(int i=0;i<ui->listTabs->count();i++)
        if (fm.width(ui->listTabs->item(i)->text())>maxw)
            maxw=fm.width(ui->listTabs->item(i)->text());

    int maxiconsz = 256;
    QList<int> sz = {8,16,22,32,48,64,128,256};
    for (int i=1;i<sz.count();i++)
        if (sz.at(i)>(5*fm.height()/3)) {
            maxiconsz=sz.at(i-1);
            break;
        }

    ui->listTabs->setIconSize(QSize(maxiconsz,maxiconsz));
    ui->listTabs->setMaximumWidth(maxw+maxiconsz+32);
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
                            arg(data["Page title"], data["Url"]));
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
    int cssRule = 0;
    while (!fs.atEnd()) {
        CAdBlockRule rule = CAdBlockRule(fs.readLine(),fi.fileName());
        // skip CSS rules for speedup search, anyway we do not support CSS rules now.
        if (!rule.isCSSRule())
            adblockList << rule;
        else
            cssRule++;
    }
    f.close();

    updateAdblockList();

    psz = adblockList.count() - psz;
    QMessageBox::information(this,tr("JPReader"),tr("%1 rules imported, %2 CSS rules dropped.").arg(psz).arg(cssRule));
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

    QString fname = getSaveFileNameD(this,tr("Save AdBlock patterns to file"),gSet->settings.savedAuxSaveDir,
                                     tr("Text file (*.txt)"));

    if (fname.isEmpty() || fname.isNull()) return;
    gSet->settings.savedAuxSaveDir = QFileInfo(fname).absolutePath();

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
    QWebEngineCookieStore* wsc = gSet->webProfile->cookieStore();
    if (wsc!=NULL)
        wsc->deleteAllCookies();

    getCookiesFromStore();
}

void CSettingsDlg::getCookiesFromStore()
{
    CNetworkCookieJar* cj = qobject_cast<CNetworkCookieJar *>(gSet->auxNetManager->cookieJar());
    if (cj!=NULL) {
        cookiesList = cj->getAllCookies();
        updateCookiesTable();
    }
}

void CSettingsDlg::delCookies()
{
    QList<int> r = getSelectedRows(ui->tableCookies);
    if (gSet->webProfile->cookieStore()!=NULL) {
        foreach (const int idx, r)
            gSet->webProfile->cookieStore()->deleteCookie(cookiesList.at(idx));

        getCookiesFromStore();
    }
}

void CSettingsDlg::exportCookies()
{
    QList<int> r = getSelectedRows(ui->tableCookies);
    if (r.isEmpty()) {
        QMessageBox::information(this,tr("JPReader"),tr("Please select cookies for export."));
        return;
    }

    QString fname = getSaveFileNameD(this,tr("Save cookies to file"),gSet->settings.savedAuxSaveDir,
                                     tr("Text file, Netscape format (*.txt)"));

    if (fname.isEmpty() || fname.isNull()) return;
    gSet->settings.savedAuxSaveDir = QFileInfo(fname).absolutePath();

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
}

void CSettingsDlg::cleanAtlCerts()
{
    gSet->atlCerts.clear();
    updateAtlCertLabel();
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

void CSettingsDlg::addSearchEngine()
{
    QStrHash data;
    data["Url template"]=QString();
    data["Menu title"]=QString();

    QString hlp = tr("In the url template you can use following substitutions\n"
                     "  %s - search text\n"
                     "  %ps - percent-encoded search text");

    CMultiInputDialog *dlg = new CMultiInputDialog(this,tr("Add new search engine"),data,hlp);
    if (dlg->exec()) {
        data = dlg->getInputData();

        QListWidgetItem* li = new QListWidgetItem(QString("%1 [ %2 ] %3").
                                                  arg(data["Menu title"],
                                                  data["Url template"],
                data["Menu title"]==gSet->settings.defaultSearchEngine ? tr("(default)") : QString()));
        li->setData(Qt::UserRole,data["Menu title"]);
        li->setData(Qt::UserRole+1,data["Url template"]);
        ui->listSearch->addItem(li);
    }
    dlg->deleteLater();
}

void CSettingsDlg::delSearchEngine()
{
    QList<QListWidgetItem *> dl = ui->listSearch->selectedItems();
    foreach (QListWidgetItem* i, dl) {
        ui->listBookmarks->removeItemWidget(i);
        delete i;
    }
}

void CSettingsDlg::updateAtlCertLabel()
{
    if (gSet!=NULL)
        ui->atlCertsLabel->setText(QString("Trusted:\n%1 certificates").arg(gSet->atlCerts.keys().count()));
}

void CSettingsDlg::setDefaultSearch()
{
    QList<QListWidgetItem *> dl = ui->listSearch->selectedItems();
    if (dl.isEmpty()) return;

    gSet->settings.defaultSearchEngine = dl.first()->data(Qt::UserRole).toString();

    setSearchEngines(getSearchEngines());
}

void CSettingsDlg::addUserScript()
{
    QDialog *dlg = new QDialog(this);
    Ui::UserScriptEditorDlg dui;
    dui.setupUi(dlg);

    if (dlg->exec()==QDialog::Accepted) {
        QListWidgetItem *itm = new QListWidgetItem(dui.editTitle->text());
        itm->setData(Qt::UserRole,dui.editSource->toPlainText());
        ui->listUserScripts->addItem(itm);
    }
    dlg->setParent(NULL);
    dlg->deleteLater();
}

void CSettingsDlg::editUserScript()
{
    if (ui->listUserScripts->selectedItems().isEmpty()) return;
    QListWidgetItem *itm = ui->listUserScripts->selectedItems().first();

    QDialog *dlg = new QDialog(this);
    Ui::UserScriptEditorDlg dui;
    dui.setupUi(dlg);

    dui.editTitle->setText(itm->text());
    dui.editSource->setPlainText(itm->data(Qt::UserRole).toString());
    if (dlg->exec()==QDialog::Accepted) {
        itm->setText(dui.editTitle->text());
        itm->setData(Qt::UserRole,dui.editSource->toPlainText());
    }
    dlg->setParent(NULL);
    dlg->deleteLater();
}

void CSettingsDlg::deleteUserScript()
{
    QList<QListWidgetItem *> dl = ui->listUserScripts->selectedItems();
    foreach (QListWidgetItem* i, dl) {
        ui->listUserScripts->removeItemWidget(i);
        delete i;
    }
}

void CSettingsDlg::importUserScript()
{
    QString fname = getOpenFileNameD(this,tr("Import user script from text file"),QDir::homePath(),
                                     tr("JavaScript files (*.js);;All files (*)"));
    if (fname.isEmpty()) return;

    QFile f(fname);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this,tr("JPReader"),tr("Unable to open file"));
        return;
    }
    QTextStream fs(&f);
    QString src = fs.readAll();
    f.close();

    CUserScript us("loader",src);

    QListWidgetItem *itm = new QListWidgetItem(us.getTitle());
    itm->setData(Qt::UserRole,src);
    ui->listUserScripts->addItem(itm);
}

void CSettingsDlg::setUserScripts(const QStrHash& scripts)
{
    ui->listUserScripts->clear();

    QStrHash::const_iterator iterator;
    for (iterator = scripts.begin(); iterator != scripts.end(); ++iterator) {
        QListWidgetItem *itm = new QListWidgetItem(iterator.key());
        itm->setData(Qt::UserRole,iterator.value());
        ui->listUserScripts->addItem(itm);
    }
}

QStrHash CSettingsDlg::getUserScripts()
{
    QStrHash res;
    for (int i=0;i<ui->listUserScripts->count();i++) {
        QListWidgetItem *itm = ui->listUserScripts->item(i);
        res[itm->text()]=itm->data(Qt::UserRole).toString();
    }
    return res;
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

void CSettingsDlg::setBookmarks(QBookmarks bookmarks)
{
    for (int i=0;i<bookmarks.count();i++) {
        QString t = bookmarks.at(i).first;
        QString u = bookmarks.at(i).second.toString();
        QListWidgetItem* li = new QListWidgetItem(QString("%1 [ %2 ]").arg(t, u));
        li->setData(Qt::UserRole,t);
        li->setData(Qt::UserRole+1,u);
        ui->listBookmarks->addItem(li);
    }
}

void CSettingsDlg::setQueryHistory(const QStringList& history)
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
        QListWidgetItem* li = new QListWidgetItem(QString("%1 [ %2 ]").arg(t.title, t.url.toString()));
        li->setData(Qt::UserRole,t.uuid.toString());
        ui->listHistory->addItem(li);
    }
}

void CSettingsDlg::setSearchEngines(const QStrHash& engines)
{
    ui->listSearch->clear();
    foreach (const QString &t, engines.keys()) {
        QListWidgetItem* li = new QListWidgetItem(QString("%1 [ %2 ] %3").
                                                  arg(t,
                                                  engines.value(t),
                t==gSet->settings.defaultSearchEngine ? tr("(default)") : QString()));
        li->setData(Qt::UserRole,t);
        li->setData(Qt::UserRole+1,engines.value(t));
        ui->listSearch->addItem(li);
    }
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

QBookmarks CSettingsDlg::getBookmarks()
{
    QBookmarks bookmarks;
    bookmarks.clear();
    for (int i=0; i<ui->listBookmarks->count(); i++)
        bookmarks << qMakePair(ui->listBookmarks->item(i)->data(Qt::UserRole).toString(),
                               ui->listBookmarks->item(i)->data(Qt::UserRole+1).toUrl());
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

QStrHash CSettingsDlg::getSearchEngines()
{
    QStrHash engines;
    engines.clear();
    for (int i=0; i<ui->listSearch->count(); i++)
        engines[ui->listSearch->item(i)->data(Qt::UserRole).toString()] =
                ui->listSearch->item(i)->data(Qt::UserRole+1).toString();
    return engines;
}
