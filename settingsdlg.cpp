#include <QInputDialog>
#include <QMessageBox>
#include <QColorDialog>
#include <QWebEngineProfile>
#include <QWebEngineCookieStore>
#include <QWebEngineSettings>
#include <goldendictlib/goldendictmgr.hh>

#include "settingsdlg.h"
#include "ui_settingsdlg.h"
#include "mainwindow.h"
#include "globalcontrol.h"
#include "globalprivate.h"
#include "genericfuncs.h"
#include "multiinputdialog.h"
#include "userscript.h"
#include "ui_userscriptdlg.h"

CSettingsDlg::CSettingsDlg(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDlg)
{
    ui->setupUi(this);

    transModel = new CLangPairModel(this, gSet->m_settings->translatorPairs, ui->listTransDirections);
    ui->listTransDirections->setModel(transModel);
    ui->listTransDirections->setItemDelegate(new CLangPairDelegate());

    connect(ui->buttonHostingDir, &QPushButton::clicked, this, &CSettingsDlg::selectDir);
    connect(ui->buttonBrowser, &QPushButton::clicked, this, &CSettingsDlg::selectBrowser);
    connect(ui->buttonEditor, &QPushButton::clicked, this, &CSettingsDlg::selectEditor);
    connect(ui->buttonDelQr, &QPushButton::clicked, this, &CSettingsDlg::delQrs);
    connect(ui->buttonCleanHistory, &QPushButton::clicked, this, &CSettingsDlg::clearHistory);
    connect(ui->buttonGoHistory, &QPushButton::clicked, this, &CSettingsDlg::goHistory);
    connect(ui->buttonAdAdd, &QPushButton::clicked, this, &CSettingsDlg::addAd);
    connect(ui->buttonAdDel, &QPushButton::clicked, this, &CSettingsDlg::delAd);
    connect(ui->buttonAdDelAll, &QPushButton::clicked, this, &CSettingsDlg::delAdAll);
    connect(ui->buttonAdImport, &QPushButton::clicked, this, &CSettingsDlg::importAd);
    connect(ui->buttonAdExport, &QPushButton::clicked, this, &CSettingsDlg::exportAd);
    connect(ui->buttonFontColorOverride, &QPushButton::clicked, this, &CSettingsDlg::fontColorDlg);
    connect(ui->buttonAddDictPath, &QPushButton::clicked, this, &CSettingsDlg::addDictPath);
    connect(ui->buttonDelDictPath, &QPushButton::clicked, this, &CSettingsDlg::delDictPath);
    connect(ui->buttonLoadedDicts, &QPushButton::clicked, this, &CSettingsDlg::showLoadedDicts);
    connect(ui->buttonClearCookies, &QPushButton::clicked, this, &CSettingsDlg::clearCookies);
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
    connect(ui->buttonAddTrDir, &QPushButton::clicked, transModel, &CLangPairModel::addItem);
    connect(ui->buttonDelTrDir, &QPushButton::clicked, transModel, &CLangPairModel::deleteItem);
    connect(ui->buttonAddNoScript, &QPushButton::clicked, this, &CSettingsDlg::addNoScriptHost);
    connect(ui->buttonDelNoScript, &QPushButton::clicked, this, &CSettingsDlg::delNoScriptHost);

    connect(ui->editAdSearch, &QLineEdit::textChanged, this, &CSettingsDlg::adblockSearch);

    connect(ui->listTabs, &QListWidget::currentRowChanged,
            ui->tabWidget, &QStackedWidget::setCurrentIndex);

    ui->atlSSLProto->addItem(QStringLiteral("Secure"),static_cast<int>(QSsl::SecureProtocols));
    ui->atlSSLProto->addItem(QStringLiteral("TLS 1.2"),static_cast<int>(QSsl::TlsV1_2));
    ui->atlSSLProto->addItem(QStringLiteral("TLS 1.1"),static_cast<int>(QSsl::TlsV1_1));
    ui->atlSSLProto->addItem(QStringLiteral("TLS 1.0"),static_cast<int>(QSsl::TlsV1_0));
    ui->atlSSLProto->addItem(QStringLiteral("SSL V3"),static_cast<int>(QSsl::SslV3));
    ui->atlSSLProto->addItem(QStringLiteral("SSL V2"),static_cast<int>(QSsl::SslV2));
    ui->atlSSLProto->addItem(QStringLiteral("Any"),static_cast<int>(QSsl::AnyProtocol));
    updateAtlCertLabel();

    populateTabList();
    ui->listTabs->setCurrentRow(0);
}

CSettingsDlg::~CSettingsDlg()
{
    delete ui;
}

void CSettingsDlg::loadFromGlobal()
{
    if (gSet==nullptr) return;

    ui->editHostingDir->setText(gSet->m_settings->hostingDir);
    ui->editHostingUrl->setText(gSet->m_settings->hostingUrl);
    ui->spinMaxLimit->setValue(gSet->m_settings->maxSearchLimit);
    ui->spinMaxHistory->setValue(gSet->m_settings->maxHistory);
    ui->spinMaxRecent->setValue(gSet->m_settings->maxRecent);
    ui->editEditor->setText(gSet->m_settings->sysEditor);
    ui->editBrowser->setText(gSet->m_settings->sysBrowser);
    ui->spinMaxRecycled->setValue(gSet->m_settings->maxRecycled);
    ui->spinMaxWhiteListItems->setValue(gSet->m_settings->maxAdblockWhiteList);
    ui->checkJS->setChecked(gSet->webProfile()->settings()->
                            testAttribute(QWebEngineSettings::JavascriptEnabled));
    ui->checkAutoloadImages->setChecked(gSet->webProfile()->settings()->
                                        testAttribute(QWebEngineSettings::AutoLoadImages));
    ui->checkEnablePlugins->setChecked(gSet->webProfile()->settings()->
                                       testAttribute(QWebEngineSettings::PluginsEnabled));

    switch (gSet->m_settings->translatorEngine) {
        case teGoogle: ui->radioGoogle->setChecked(true); break;
        case teAtlas: ui->radioAtlas->setChecked(true); break;
        case teBingAPI: ui->radioBingAPI->setChecked(true); break;
        case teYandexAPI: ui->radioYandexAPI->setChecked(true); break;
        case teGoogleGTX: ui->radioGoogleGTX->setChecked(true); break;
    }
    ui->editScpHost->clear();
    ui->editScpHost->addItems(gSet->m_settings->scpHostHistory);
    ui->editScpHost->setEditText(gSet->m_settings->scpHost);
    ui->editScpParams->setText(gSet->m_settings->scpParams);
    ui->checkSCP->setChecked(gSet->m_settings->useScp);

    ui->atlHost->clear();
    ui->atlHost->addItems(gSet->m_settings->atlHostHistory);
    ui->atlHost->setEditText(gSet->m_settings->atlHost);
    ui->atlPort->setValue(gSet->m_settings->atlPort);
    ui->atlRetryCnt->setValue(gSet->m_settings->atlTcpRetryCount);
    ui->atlRetryTimeout->setValue(gSet->m_settings->atlTcpTimeout);
    ui->atlToken->setText(gSet->m_settings->atlToken);
    int idx = ui->atlSSLProto->findData(gSet->m_settings->atlProto);
    if (idx<0 || idx>=ui->atlSSLProto->count())
        idx = 0;
    ui->atlSSLProto->setCurrentIndex(idx);
    updateAtlCertLabel();

    ui->editBingKey->setText(gSet->m_settings->bingKey);
    ui->editYandexKey->setText(gSet->m_settings->yandexKey);
    ui->checkEmptyRestore->setChecked(gSet->m_settings->emptyRestore);
    ui->checkJSLogConsole->setChecked(gSet->m_settings->jsLogConsole);

    ui->checkUseAd->setChecked(gSet->m_settings->useAdblock);
    ui->checkUseNoScript->setChecked(gSet->m_settings->useNoScript);

    ui->checkTransFont->setChecked(gSet->m_ui->useOverrideFont());
    ui->checkStdFonts->setChecked(gSet->m_settings->overrideStdFonts);
    ui->transFont->setCurrentFont(gSet->m_settings->overrideFont);
    QFont f = QApplication::font();
    f.setFamily(gSet->m_settings->fontStandard);
    ui->fontStandard->setCurrentFont(f);
    f.setFamily(gSet->m_settings->fontFixed);
    ui->fontFixed->setCurrentFont(f);
    f.setFamily(gSet->m_settings->fontSerif);
    ui->fontSerif->setCurrentFont(f);
    f.setFamily(gSet->m_settings->fontSansSerif);
    ui->fontSansSerif->setCurrentFont(f);
    ui->transFontSize->setValue(gSet->m_settings->overrideFont.pointSize());
    ui->transFont->setEnabled(gSet->m_ui->useOverrideFont());
    ui->transFontSize->setEnabled(gSet->m_ui->useOverrideFont());
    ui->fontStandard->setEnabled(gSet->m_settings->overrideStdFonts);
    ui->fontFixed->setEnabled(gSet->m_settings->overrideStdFonts);
    ui->fontSerif->setEnabled(gSet->m_settings->overrideStdFonts);
    ui->fontSansSerif->setEnabled(gSet->m_settings->overrideStdFonts);
    ui->checkFontColorOverride->setChecked(gSet->m_ui->forceFontColor());
    updateFontColorPreview(gSet->m_settings->forcedFontColor);

    ui->gctxHotkey->setKeySequence(gSet->m_ui->gctxTranHotkey.shortcut());
    ui->checkCreateCoredumps->setChecked(gSet->m_settings->createCoredumps);
#ifndef WITH_RECOLL
    ui->radioSearchRecoll->setEnabled(false);
#endif
#ifndef WITH_BALOO5
    ui->radioSearchBaloo5->setEnabled(false);
#endif
    if ((gSet->m_settings->searchEngine==seRecoll) && (ui->radioSearchRecoll->isEnabled())) {
        ui->radioSearchRecoll->setChecked(true);
    } else if ((gSet->m_settings->searchEngine==seBaloo5) && (ui->radioSearchBaloo5->isEnabled())) {
        ui->radioSearchBaloo5->setChecked(true);
    } else {
        ui->radioSearchNone->setChecked(true);
    }

    ui->checkTabCloseBtn->setChecked(gSet->m_settings->showTabCloseButtons);

    ui->checkLogNetworkRequests->setChecked(gSet->m_ui->actionLogNetRequests->isChecked());
    ui->checkDumpHtml->setChecked(gSet->m_settings->debugDumpHtml);

    ui->checkUserAgent->setChecked(gSet->m_settings->overrideUserAgent);
    ui->editUserAgent->setEnabled(gSet->m_settings->overrideUserAgent);
    ui->editUserAgent->clear();
    ui->editUserAgent->addItems(gSet->m_settings->userAgentHistory);
    ui->editUserAgent->lineEdit()->setText(gSet->m_settings->userAgent);

    ui->listDictPaths->clear();
    ui->listDictPaths->addItems(gSet->m_settings->dictPaths);

    loadedDicts.clear();
    loadedDicts.append(gSet->d_func()->dictManager->getLoadedDictionaries());

    ui->checkDontUseNativeFileDialogs->setChecked(gSet->m_settings->dontUseNativeFileDialog);
    ui->spinCacheSize->setValue(gSet->webProfile()->httpCacheMaximumSize()/oneMB);
    ui->checkIgnoreSSLErrors->setChecked(gSet->m_settings->ignoreSSLErrors);
    ui->checkTabFavicon->setChecked(gSet->webProfile()->settings()->
                                    testAttribute(QWebEngineSettings::AutoLoadIconsForPage));

    setSearchEngines(gSet->d_func()->ctxSearchEngines);

    ui->checkPdfExtractImages->setChecked(gSet->m_settings->pdfExtractImages);
    ui->spinPdfImageQuality->setValue(gSet->m_settings->pdfImageQuality);
    ui->spinPdfImageMaxSize->setValue(gSet->m_settings->pdfImageMaxSize);

    ui->checkPixivFetchImages->setChecked(gSet->m_settings->pixivFetchImages);

    // flip proxy use check, for updating controls enabling logic
    ui->checkUseProxy->setChecked(true);
    ui->checkUseProxy->setChecked(false);

    ui->editProxyHost->setText(gSet->m_settings->proxyHost);
    ui->spinProxyPort->setValue(gSet->m_settings->proxyPort);
    ui->editProxyLogin->setText(gSet->m_settings->proxyLogin);
    ui->editProxyPassword->setText(gSet->m_settings->proxyPassword);
    ui->checkUseProxy->setChecked(gSet->m_settings->proxyUse);
    ui->checkUseProxyTranslator->setChecked(gSet->m_settings->proxyUseTranslator);
    switch (gSet->m_settings->proxyType) {
        case QNetworkProxy::HttpCachingProxy: ui->listProxyType->setCurrentIndex(0); break;
        case QNetworkProxy::HttpProxy: ui->listProxyType->setCurrentIndex(1); break;
        case QNetworkProxy::Socks5Proxy: ui->listProxyType->setCurrentIndex(2); break;
        case QNetworkProxy::NoProxy:
        case QNetworkProxy::DefaultProxy:
        case QNetworkProxy::FtpCachingProxy:
            ui->listProxyType->setCurrentIndex(0);
            break;
    }

    getCookiesFromStore();
}

void CSettingsDlg::saveToGlobal(QStringList &dictPaths)
{
    gSet->m_settings->hostingDir=ui->editHostingDir->text();
    gSet->m_settings->hostingUrl=ui->editHostingUrl->text();
    gSet->m_settings->maxSearchLimit=ui->spinMaxLimit->value();
    gSet->m_settings->maxHistory=ui->spinMaxHistory->value();
    gSet->m_settings->sysEditor=ui->editEditor->text();
    gSet->m_settings->sysBrowser=ui->editBrowser->text();
    gSet->m_settings->maxRecycled=ui->spinMaxRecycled->value();
    gSet->m_settings->maxRecent=ui->spinMaxRecent->value();
    gSet->m_settings->maxAdblockWhiteList=ui->spinMaxWhiteListItems->value();

    gSet->webProfile()->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled,
                                                 ui->checkJS->isChecked());
    gSet->m_ui->actionJSUsage->setChecked(ui->checkJS->isChecked());
    gSet->webProfile()->settings()->setAttribute(QWebEngineSettings::AutoLoadImages,
                                                 ui->checkAutoloadImages->isChecked());
    gSet->webProfile()->settings()->setAttribute(QWebEngineSettings::PluginsEnabled,
                                                 ui->checkEnablePlugins->isChecked());

    if (ui->radioGoogle->isChecked()) gSet->m_settings->translatorEngine=teGoogle;
    else if (ui->radioAtlas->isChecked()) gSet->m_settings->translatorEngine=teAtlas;
    else if (ui->radioBingAPI->isChecked()) gSet->m_settings->translatorEngine=teBingAPI;
    else if (ui->radioYandexAPI->isChecked()) gSet->m_settings->translatorEngine=teYandexAPI;
    else if (ui->radioGoogleGTX->isChecked()) gSet->m_settings->translatorEngine=teGoogleGTX;
    else gSet->m_settings->translatorEngine=teAtlas;
    gSet->m_settings->useScp=ui->checkSCP->isChecked();
    gSet->m_settings->scpHost=ui->editScpHost->lineEdit()->text();
    gSet->m_settings->scpParams=ui->editScpParams->text();
    gSet->m_settings->atlHost=ui->atlHost->lineEdit()->text();
    gSet->m_settings->atlPort=static_cast<quint16>(ui->atlPort->value());
    gSet->m_settings->atlTcpRetryCount=ui->atlRetryCnt->value();
    gSet->m_settings->atlTcpTimeout=ui->atlRetryTimeout->value();
    gSet->m_settings->atlToken=ui->atlToken->text();
    gSet->m_settings->atlProto=static_cast<QSsl::SslProtocol>(ui->atlSSLProto->currentData().toInt());
    gSet->m_settings->bingKey=ui->editBingKey->text();
    gSet->m_settings->yandexKey=ui->editYandexKey->text();
    gSet->m_settings->emptyRestore=ui->checkEmptyRestore->isChecked();
    gSet->m_settings->jsLogConsole=ui->checkJSLogConsole->isChecked();

    gSet->m_ui->actionOverrideFont->setChecked(ui->checkTransFont->isChecked());
    gSet->m_settings->overrideStdFonts=ui->checkStdFonts->isChecked();
    gSet->m_settings->overrideFont=ui->transFont->currentFont();
    gSet->m_settings->overrideFont.setPointSize(ui->transFontSize->value());
    gSet->m_settings->fontStandard=ui->fontStandard->currentFont().family();
    gSet->m_settings->fontFixed=ui->fontFixed->currentFont().family();
    gSet->m_settings->fontSerif=ui->fontSerif->currentFont().family();
    gSet->m_settings->fontSansSerif=ui->fontSansSerif->currentFont().family();
    gSet->m_ui->actionOverrideFontColor->setChecked(ui->checkFontColorOverride->isChecked());

    gSet->m_settings->useAdblock=ui->checkUseAd->isChecked();
    gSet->m_settings->useNoScript=ui->checkUseNoScript->isChecked();

    gSet->m_ui->gctxTranHotkey.setShortcut(ui->gctxHotkey->keySequence());

    gSet->m_settings->createCoredumps=ui->checkCreateCoredumps->isChecked();
    if (ui->radioSearchRecoll->isChecked()) {
        gSet->m_settings->searchEngine = seRecoll;
    } else if (ui->radioSearchBaloo5->isChecked()) {
        gSet->m_settings->searchEngine = seBaloo5;
    } else {
        gSet->m_settings->searchEngine = seNone;
    }

    gSet->m_settings->showTabCloseButtons = ui->checkTabCloseBtn->isChecked();

    gSet->m_ui->actionLogNetRequests->setChecked(ui->checkLogNetworkRequests->isChecked());
    gSet->m_settings->debugDumpHtml = ui->checkDumpHtml->isChecked();

    gSet->m_settings->overrideUserAgent = ui->checkUserAgent->isChecked();
    if (gSet->m_settings->overrideUserAgent)
        gSet->m_settings->userAgent = ui->editUserAgent->lineEdit()->text();

    dictPaths.clear();
    dictPaths.reserve(ui->listDictPaths->count());
    for (int i=0;i<ui->listDictPaths->count();i++)
        dictPaths.append(ui->listDictPaths->item(i)->text());

    gSet->m_settings->pdfExtractImages = ui->checkPdfExtractImages->isChecked();
    gSet->m_settings->pdfImageMaxSize = ui->spinPdfImageMaxSize->value();
    gSet->m_settings->pdfImageQuality = ui->spinPdfImageQuality->value();

    gSet->m_settings->pixivFetchImages = ui->checkPixivFetchImages->isChecked();

    gSet->m_settings->dontUseNativeFileDialog = ui->checkDontUseNativeFileDialogs->isChecked();
    gSet->webProfile()->setHttpCacheMaximumSize(ui->spinCacheSize->value()*oneMB);
    gSet->m_settings->ignoreSSLErrors = ui->checkIgnoreSSLErrors->isChecked();
    gSet->webProfile()->settings()->setAttribute(QWebEngineSettings::AutoLoadIconsForPage,
                                                 ui->checkTabFavicon->isChecked());
    gSet->m_settings->proxyHost = ui->editProxyHost->text();
    gSet->m_settings->proxyPort = static_cast<quint16>(ui->spinProxyPort->value());
    gSet->m_settings->proxyLogin = ui->editProxyLogin->text();
    gSet->m_settings->proxyPassword = ui->editProxyPassword->text();
    gSet->m_settings->proxyUse = ui->checkUseProxy->isChecked();
    gSet->m_settings->proxyUseTranslator = ui->checkUseProxyTranslator->isChecked();
    switch (ui->listProxyType->currentIndex()) {
        case 0: gSet->m_settings->proxyType = QNetworkProxy::HttpCachingProxy; break;
        case 1: gSet->m_settings->proxyType = QNetworkProxy::HttpProxy; break;
        case 2: gSet->m_settings->proxyType = QNetworkProxy::Socks5Proxy; break;
        default: gSet->m_settings->proxyType = QNetworkProxy::HttpCachingProxy; break;
    }
}

void CSettingsDlg::resizeEvent(QResizeEvent *event)
{
    const int transDirectionsColumnWidthFrac = 30;

    ui->listTransDirections->setColumnWidth(0, transDirectionsColumnWidthFrac*event->size().width()/100);
    ui->listTransDirections->setColumnWidth(1, transDirectionsColumnWidthFrac*event->size().width()/100);

    QDialog::resizeEvent(event);
}

void CSettingsDlg::populateTabList()
{
    const QList<int> iconSizes = {8,16,22,32,48,64,128,256};
    const int iconSizeFrac = 165;
    const int padding = 32;

    ui->listTabs->clear();

    QListWidgetItem* itm;

    itm = new QListWidgetItem(
              QIcon::fromTheme(QStringLiteral("internet-web-browser")),tr("Browser"));
    ui->listTabs->addItem(itm);
    itm = new QListWidgetItem(
              QIcon::fromTheme(QStringLiteral("document-preview")),tr("Search"));
    ui->listTabs->addItem(itm);
    itm = new QListWidgetItem(
              QIcon::fromTheme(QStringLiteral("document-edit-decrypt-verify")),tr("Translator"));
    ui->listTabs->addItem(itm);
    itm = new QListWidgetItem(
              QIcon::fromTheme(QStringLiteral("format-text-color")),tr("Fonts"));
    ui->listTabs->addItem(itm);
    itm = new QListWidgetItem(
              QIcon::fromTheme(QStringLiteral("system-run")),tr("Programs"));
    ui->listTabs->addItem(itm);
    itm = new QListWidgetItem(
              QIcon::fromTheme(QStringLiteral("document-edit-decrypt")),tr("Translation pairs"));
    ui->listTabs->addItem(itm);
    itm = new QListWidgetItem(
              QIcon(QStringLiteral(":/img/nepomuk")),tr("Query history"));
    ui->listTabs->addItem(itm);
    itm = new QListWidgetItem(
              QIcon::fromTheme(QStringLiteral("view-history")),tr("History"));
    ui->listTabs->addItem(itm);
    itm = new QListWidgetItem(
              QIcon::fromTheme(QStringLiteral("preferences-web-browser-adblock")),tr("AdBlock"));
    ui->listTabs->addItem(itm);
    itm = new QListWidgetItem(
              QIcon::fromTheme(QStringLiteral("dialog-cancel")),tr("NoScript"));
    ui->listTabs->addItem(itm);
    itm = new QListWidgetItem(
              QIcon::fromTheme(QStringLiteral("preferences-web-browser-cookies")),tr("Cookies"));
    ui->listTabs->addItem(itm);
    itm = new QListWidgetItem(
              QIcon::fromTheme(QStringLiteral("folder-script")),tr("User scripts"));
    ui->listTabs->addItem(itm);
    itm = new QListWidgetItem(
              QIcon::fromTheme(QStringLiteral("server-database")),tr("Network & logging"));
    ui->listTabs->addItem(itm);

    int maxTextWidth = 0;
    QFontMetrics fm(ui->listTabs->item(0)->font());
    for(int i=0;i<ui->listTabs->count();i++) {
        if (fm.width(ui->listTabs->item(i)->text())>maxTextWidth)
            maxTextWidth=fm.width(ui->listTabs->item(i)->text());
    }

    int maxIconSize = iconSizes.last();
    for (int i=1;i<iconSizes.count();i++) {
        if (iconSizes.at(i)>(iconSizeFrac*fm.height()/100)) {
            maxIconSize=iconSizes.at(i-1);
            break;
        }
    }

    ui->listTabs->setIconSize(QSize(maxIconSize,maxIconSize));
    ui->listTabs->setMaximumWidth(maxTextWidth+maxIconSize+padding);
}

void CSettingsDlg::updateCookiesTable()
{
    const static QStringList horizontalLabels( { tr("Domain"), tr("Name"), tr("Path"),
                                                 tr("Expiration"), tr("Value") } );
    QTableWidget *table = ui->tableCookies;

    table->setSortingEnabled(false);
    table->clear();
    table->setColumnCount(horizontalLabels.count());
    table->setRowCount(cookiesList.count());
    table->setHorizontalHeaderLabels(horizontalLabels);
    for (int i=0;i<cookiesList.count();i++) {

        QString s = cookiesList.at(i).domain();
        auto item = new QTableWidgetItem(s);
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
        QTreeWidgetItem* tli = nullptr;
        QString cat = adblockList.at(i).listID();
        if (cats.contains(cat)) {
            tli = cats.value(cat);
        } else {
            tli = new QTreeWidgetItem(ui->treeAdblock);
            tli->setText(0,cat);
            cats[cat] = tli;
        }

        auto item = new QTreeWidgetItem(tli);
        item->setText(0,adblockList.at(i).filter());
        item->setData(0,Qt::UserRole+1,i);
    }
    ui->lblAdblockTotalRules->setText(
                tr("Total rules: %1.").arg(adblockList.count()));
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
    for (QListWidgetItem* i : dl) {
        ui->listQr->removeItemWidget(i);
        delete i;
    }
}

void CSettingsDlg::clearHistory()
{
    if (gSet) {
        gSet->d_func()->mainHistory.clear();
        Q_EMIT gSet->updateAllHistoryLists();
    }
    ui->listHistory->clear();
}

void CSettingsDlg::goHistory()
{
    QListWidgetItem* it = ui->listHistory->currentItem();
    if (it==nullptr || gSet->activeWindow()==nullptr) return;
    QUuid idx(it->data(Qt::UserRole).toString());
    gSet->d_func()->activeWindow->goHistory(idx);
}

void CSettingsDlg::addAd()
{
    bool ok;
    QString s = QInputDialog::getText(this,tr("Add AdBlock rule"),tr("Filter template"),
                                      QLineEdit::Normal,QString(),&ok);
    if (!ok) return;

    adblockList << CAdBlockRule(s,QString());
    updateAdblockList();
}

void CSettingsDlg::delAd()
{
    QList<int> r;
    const QList<QTreeWidgetItem *> il = ui->treeAdblock->selectedItems();
    r.reserve(il.count());
    for (const QTreeWidgetItem* i : il)
        r << i->data(0,Qt::UserRole+1).toInt();

    QVector<CAdBlockRule> tmp = adblockList;

    for (const int idx : qAsConst(r))
        adblockList.removeOne(tmp.at(idx));

    tmp.clear();

    updateAdblockList();
}

void CSettingsDlg::delAdAll()
{
    adblockList.clear();

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
        if (!rule.isCSSRule()) {
            adblockList << rule;
        } else {
            cssRule++;
        }
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
    const QList<QTreeWidgetItem *> il = ui->treeAdblock->selectedItems();
    r.reserve(il.count());
    for (const QTreeWidgetItem* i : il)
        r << i->data(0,Qt::UserRole+1).toInt();

    QString fname = getSaveFileNameD(this,tr("Save AdBlock patterns to file"),
                                     gSet->settings()->savedAuxSaveDir,
                                     tr("Text file (*.txt)"));

    if (fname.isEmpty() || fname.isNull()) return;
    gSet->setSavedAuxSaveDir(QFileInfo(fname).absolutePath());

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
    QWebEngineCookieStore* wsc = gSet->webProfile()->cookieStore();
    if (wsc)
        wsc->deleteAllCookies();

    getCookiesFromStore();
}

void CSettingsDlg::getCookiesFromStore()
{
    auto cj = qobject_cast<CNetworkCookieJar *>(gSet->d_func()->auxNetManager->cookieJar());
    if (cj) {
        cookiesList = cj->getAllCookies();
        updateCookiesTable();
    }
}

void CSettingsDlg::delCookies()
{
    QList<int> r = getSelectedRows(ui->tableCookies);
    if (gSet->webProfile()->cookieStore()) {
        for (int idx : qAsConst(r))
            gSet->webProfile()->cookieStore()->deleteCookie(cookiesList.at(idx));

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

    QString fname = getSaveFileNameD(this,tr("Save cookies to file"),gSet->settings()->savedAuxSaveDir,
                                     tr("Text file, Netscape format (*.txt)"));

    if (fname.isEmpty() || fname.isNull()) return;
    gSet->setSavedAuxSaveDir(QFileInfo(fname).absolutePath());

    QFile f(fname);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this,tr("JPReader"),tr("Unable to create file."));
        return;
    }
    QTextStream fs(&f);

    for (int i=0;i<r.count();i++) {
        int idx = r.at(i);
        fs << cookiesList.at(idx).domain()
           << '\t'
           << bool2str2(cookiesList.at(idx).domain().startsWith('.'))
           << '\t'
           << cookiesList.at(idx).path()
           << '\t'
           << bool2str2(cookiesList.at(idx).isSecure())
           << '\t'
           << cookiesList.at(idx).expirationDate().toSecsSinceEpoch()
           << '\t'
           << cookiesList.at(idx).name()
           << '\t'
           << cookiesList.at(idx).value()
           << endl;
    }
    fs.flush();
    f.close();
}

void CSettingsDlg::cleanAtlCerts()
{
    gSet->m_settings->atlCerts.clear();
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
    CStringHash data;
    data[QStringLiteral("Url template")]=QString();
    data[QStringLiteral("Menu title")]=QString();

    static const QString hlp = tr("In the url template you can use following substitutions\n"
                                  "  %s - search text\n"
                                  "  %ps - percent-encoded search text");

    CMultiInputDialog *dlg = new CMultiInputDialog(this,tr("Add new search engine"),data,hlp);
    if (dlg->exec()==QDialog::Accepted) {
        data = dlg->getInputData();

        QListWidgetItem* li = new QListWidgetItem(QStringLiteral("%1 [ %2 ] %3").
                                                  arg(data[QStringLiteral("Menu title")],
                                                  data[QStringLiteral("Url template")],
                data[QStringLiteral("Menu title")]==gSet->m_settings->defaultSearchEngine ?
                    tr("(default)") : QString()));
        li->setData(Qt::UserRole,data[QStringLiteral("Menu title")]);
        li->setData(Qt::UserRole+1,data[QStringLiteral("Url template")]);
        ui->listSearch->addItem(li);
    }
    dlg->deleteLater();
}

void CSettingsDlg::delSearchEngine()
{
    QList<QListWidgetItem *> dl = ui->listSearch->selectedItems();
    for (QListWidgetItem* i : dl) {
        ui->listSearch->removeItemWidget(i);
        delete i;
    }
}

void CSettingsDlg::updateAtlCertLabel()
{
    if (gSet) {
        ui->atlCertsLabel->setText(QStringLiteral("Trusted:\n%1 certificates")
                                   .arg(gSet->m_settings->atlCerts.count()));
    }
}

void CSettingsDlg::setDefaultSearch()
{
    const QList<QListWidgetItem *> dl = ui->listSearch->selectedItems();
    if (dl.isEmpty()) return;

    gSet->m_settings->defaultSearchEngine = dl.first()->data(Qt::UserRole).toString();

    setSearchEngines(getSearchEngines());
}

void CSettingsDlg::addUserScript()
{
    static QSize addScriptDlgSize;
    QDialog *dlg = new QDialog(this);
    Ui::UserScriptEditorDlg dui;
    dui.setupUi(dlg);
    if (!addScriptDlgSize.isEmpty())
        dlg->resize(addScriptDlgSize);

    if (dlg->exec()==QDialog::Accepted) {
        QListWidgetItem *itm = new QListWidgetItem(dui.editTitle->text());
        itm->setData(Qt::UserRole,dui.editSource->toPlainText());
        ui->listUserScripts->addItem(itm);
    }
    addScriptDlgSize = dlg->size();
    dlg->setParent(nullptr);
    dlg->deleteLater();
}

void CSettingsDlg::editUserScript()
{
    if (ui->listUserScripts->selectedItems().isEmpty()) return;
    QListWidgetItem *itm = ui->listUserScripts->selectedItems().first();

    static QSize editScriptDlgSize;
    QDialog *dlg = new QDialog(this);
    Ui::UserScriptEditorDlg dui;
    dui.setupUi(dlg);
    if (!editScriptDlgSize.isEmpty())
        dlg->resize(editScriptDlgSize);

    dui.editTitle->setText(itm->text());
    dui.editSource->setPlainText(itm->data(Qt::UserRole).toString());
    if (dlg->exec()==QDialog::Accepted) {
        itm->setText(dui.editTitle->text());
        itm->setData(Qt::UserRole,dui.editSource->toPlainText());
    }
    editScriptDlgSize = dlg->size();
    dlg->setParent(nullptr);
    dlg->deleteLater();
}

void CSettingsDlg::addNoScriptHost()
{
    bool ok;
    QString s = QInputDialog::getText(this,tr("Add to NoScript whitelist"),tr("Hostname"),
                                      QLineEdit::Normal,QString(),&ok);
    if (!ok) return;

    auto li = new QListWidgetItem(s);
    ui->listNoScriptWhitelist->addItem(li);
}

void CSettingsDlg::delNoScriptHost()
{
    QList<QListWidgetItem *> dl = ui->listNoScriptWhitelist->selectedItems();
    for (QListWidgetItem* i : dl) {
        ui->listNoScriptWhitelist->removeItemWidget(i);
        delete i;
    }
}

void CSettingsDlg::deleteUserScript()
{
    QList<QListWidgetItem *> dl = ui->listUserScripts->selectedItems();
    for (QListWidgetItem* i : dl) {
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

    CUserScript us(QStringLiteral("loader"),src);

    QListWidgetItem *itm = new QListWidgetItem(us.getTitle());
    itm->setData(Qt::UserRole,src);
    ui->listUserScripts->addItem(itm);
}

void CSettingsDlg::setUserScripts(const CStringHash& scripts)
{
    ui->listUserScripts->clear();

    for (auto it = scripts.constBegin(), end = scripts.constEnd(); it != end; ++it) {
        auto itm = new QListWidgetItem(it.key());
        itm->setData(Qt::UserRole,it.value());
        ui->listUserScripts->addItem(itm);
    }
}

CStringHash CSettingsDlg::getUserScripts()
{
    CStringHash res;
    for (int i=0;i<ui->listUserScripts->count();i++) {
        QListWidgetItem *itm = ui->listUserScripts->item(i);
        res[itm->text()]=itm->data(Qt::UserRole).toString();
    }
    return res;
}

CLangPairVector CSettingsDlg::getLangPairList()
{
    return transModel->getLangPairList();
}

void CSettingsDlg::setNoScriptWhitelist(const CStringSet &hosts)
{
    ui->listNoScriptWhitelist->clear();

    for (auto it = hosts.constBegin(), end = hosts.constEnd(); it != end; ++it) {
        auto itm = new QListWidgetItem(*it);
        ui->listNoScriptWhitelist->addItem(itm);
    }
}

CStringSet CSettingsDlg::getNoScriptWhitelist()
{
    CStringSet res;
    for (int i=0;i<ui->listNoScriptWhitelist->count();i++) {
        QListWidgetItem *itm = ui->listNoScriptWhitelist->item(i);
        res.insert(itm->text());
    }
    return res;
}

void CSettingsDlg::adblockFocusSearchedRule(QList<QTreeWidgetItem *> & items)
{
    if (adblockSearchIdx>=0 && adblockSearchIdx<items.count())
        ui->treeAdblock->setCurrentItem(items.at(adblockSearchIdx));
}

void CSettingsDlg::updateFontColorPreview(const QColor &c)
{
    overridedFontColor = c;
    ui->frameFontColorOverride->setStyleSheet(QStringLiteral("QFrame { background: %1; }")
                                              .arg(overridedFontColor.name(QColor::HexRgb)));
}

QColor CSettingsDlg::getOverridedFontColor()
{
    return overridedFontColor;
}

void CSettingsDlg::setQueryHistory(const QStringList& history)
{
    ui->listQr->clear();
    for (int i=0;i<history.count();i++)
        ui->listQr->addItem(history.at(i));
}

void CSettingsDlg::setAdblock(const QVector<CAdBlockRule> &adblock)
{
    adblockList = adblock;
    updateAdblockList();
}

void CSettingsDlg::setMainHistory(const CUrlHolderVector& history)
{
    for (const CUrlHolder &t : history) {
        QListWidgetItem* li = new QListWidgetItem(QStringLiteral("%1 [ %2 ]")
                                                  .arg(t.title, t.url.toString()));
        li->setData(Qt::UserRole,t.uuid.toString());
        ui->listHistory->addItem(li);
    }
}

void CSettingsDlg::setSearchEngines(const CStringHash& engines)
{
    ui->listSearch->clear();
    for (auto it = engines.constBegin(), end = engines.constEnd(); it != end; ++it) {
        QListWidgetItem* li = new QListWidgetItem(QStringLiteral("%1 [ %2 ] %3").
                                                  arg(it.key(),
                                                      it.value(),
                                                      it.key()==gSet->m_settings->defaultSearchEngine ? tr("(default)") : QString()));
        li->setData(Qt::UserRole,it.key());
        li->setData(Qt::UserRole+1,it.value());
        ui->listSearch->addItem(li);
    }
}

QList<int> CSettingsDlg::getSelectedRows(QTableWidget *table) const
{
    QList<int> res;
    res.clear();
    const QList<QTableWidgetItem *> il = table->selectedItems();
    for (const QTableWidgetItem *i : il) {
        if (!res.contains(i->data(Qt::UserRole+1).toInt()))
            res << i->data(Qt::UserRole+1).toInt();
    }
    return res;
}

QStringList CSettingsDlg::getQueryHistory()
{
    QStringList sl;
    sl.reserve(ui->listQr->count());
    for (int i=0; i<ui->listQr->count();i++)
        sl << ui->listQr->item(i)->text();
    return sl;
}

QVector<CAdBlockRule> CSettingsDlg::getAdblock()
{
    return adblockList;
}

CStringHash CSettingsDlg::getSearchEngines()
{
    CStringHash engines;
    engines.clear();
    for (int i=0; i<ui->listSearch->count(); i++) {
        engines[ui->listSearch->item(i)->data(Qt::UserRole).toString()] =
                ui->listSearch->item(i)->data(Qt::UserRole+1).toString();
    }
    return engines;
}

CLangPairDelegate::CLangPairDelegate(QObject *parent) :
    QStyledItemDelegate (parent)
{

}

QWidget *CLangPairDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                                         const QModelIndex &index) const
{
    Q_UNUSED(option)
    Q_UNUSED(index)
    auto editor = new QComboBox(parent);
    editor->setFrame(false);
    const QStringList sl = gSet->getLanguageCodes();
    for (const QString& bcp : qAsConst(sl)) {
        editor->addItem(gSet->getLanguageName(bcp),QVariant::fromValue(bcp));
    }
    return editor;
}

void CLangPairDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    QString bcp = index.model()->data(index, Qt::EditRole).toString();

    auto cb = qobject_cast<QComboBox *>(editor);

    int idx = cb->findData(QVariant::fromValue(bcp));
    if (idx>=0)
        cb->setCurrentIndex(idx);
}

void CLangPairDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    auto cb = qobject_cast<QComboBox *>(editor);

    model->setData(index, cb->currentData().toString(), Qt::EditRole);
}

void CLangPairDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                                             const QModelIndex &index) const
{
    Q_UNUSED(index)
    editor->setGeometry(option.rect);
}

CLangPairModel::CLangPairModel(QObject *parent, const CLangPairVector &list, QTableView *table) :
    QAbstractTableModel (parent)
{
    m_list = list;
    m_table = table;
}

CLangPairVector CLangPairModel::getLangPairList()
{
    return m_list;
}

int CLangPairModel::rowCount(const QModelIndex & parent) const
{
    Q_UNUSED(parent)
    return m_list.count();
}

int CLangPairModel::columnCount(const QModelIndex & parent) const
{
    Q_UNUSED(parent)
    return 2;
}

QVariant CLangPairModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch (section) {
            case 0: return QVariant(QStringLiteral("Source language"));
            case 1: return QVariant(QStringLiteral("Destination language"));
            default: return QVariant();
        }
    }
    return QVariant();
}

QVariant CLangPairModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row()<0 || index.row()>=m_list.count()) return QVariant();

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case 0: return QVariant(gSet->getLanguageName(m_list.at(index.row()).langFrom.bcp47Name()));
            case 1: return QVariant(gSet->getLanguageName(m_list.at(index.row()).langTo.bcp47Name()));
            default: return QVariant();
        }
    }
    if (role == Qt::EditRole) {
        switch (index.column()) {
            case 0: return QVariant(m_list.at(index.row()).langFrom.bcp47Name());
            case 1: return QVariant(m_list.at(index.row()).langTo.bcp47Name());
            default: return QVariant();
        }
    }
    return QVariant();
}

Qt::ItemFlags CLangPairModel::flags(const QModelIndex &index) const
{
    Q_UNUSED(index)
    return (Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
}

bool CLangPairModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (index.row()<0 || index.row()>=m_list.count()) return false;
    if (role == Qt::EditRole) {
        switch (index.column()) {
            case 0: m_list[index.row()].langFrom = QLocale(value.toString()); break;
            case 1: m_list[index.row()].langTo = QLocale(value.toString()); break;
        }
        return true;
    }
    return false;
}

bool CLangPairModel::insertRows(int row, int count, const QModelIndex &parent)
{
    beginInsertRows(parent, row, row+count-1);
    for (int i=0;i<count;i++)
        m_list.insert(row, CLangPair(QStringLiteral("ja"),QStringLiteral("en")));
    endInsertRows();
    return true;
}

bool CLangPairModel::removeRows(int row, int count, const QModelIndex &parent)
{
    beginRemoveRows(parent, row, row+count-1);
    for (int i=0;i<count;i++)
        m_list.removeAt(row);
    endRemoveRows();
    return true;
}

void CLangPairModel::addItem()
{
    int row = 0;
    if (m_table->currentIndex().isValid())
        row = m_table->currentIndex().row();
    insertRows(row,1);
}

void CLangPairModel::deleteItem()
{
    if (m_table->currentIndex().isValid())
        removeRows(m_table->currentIndex().row(),1);
}
