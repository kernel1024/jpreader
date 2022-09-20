#include <algorithm>
#include <execution>

#include <QInputDialog>
#include <QMessageBox>
#include <QColorDialog>
#include <QScrollBar>
#include <QWebEngineProfile>
#include <QWebEngineCookieStore>
#include <QWebEngineSettings>
#include <QLocale>
#include <QPointer>

#include "settingstab.h"
#include "ui_settingstab.h"
#include "mainwindow.h"
#include "global/control.h"
#include "global/control_p.h"
#include "global/contentfiltering.h"
#include "global/network.h"
#include "global/history.h"
#include "global/browserfuncs.h"
#include "global/ui.h"
#include "genericfuncs.h"
#include "multiinputdialog.h"
#include "browser-utils/userscript.h"
#include "ui_userscriptdlg.h"

CSettingsTab::CSettingsTab(QWidget *parent) :
    CSpecTabContainer(parent),
    ui(new Ui::SettingsTab)
{
    ui->setupUi(this);

    setTabTitle(tr("Settings"));

    transModel = new CLangPairModel(this, ui->listTransDirections);
    ui->listTransDirections->setModel(transModel);
    ui->listTransDirections->setItemDelegate(new CLangPairDelegate());

    connect(ui->buttonBrowser, &QPushButton::clicked, this, &CSettingsTab::selectBrowser);
    connect(ui->buttonEditor, &QPushButton::clicked, this, &CSettingsTab::selectEditor);
    connect(ui->buttonDelQr, &QPushButton::clicked, this, &CSettingsTab::delQrs);
    connect(ui->buttonCleanHistory, &QPushButton::clicked, this, &CSettingsTab::clearHistory);
    connect(ui->buttonGoHistory, &QPushButton::clicked, this, &CSettingsTab::goHistory);
    connect(ui->buttonAdAdd, &QPushButton::clicked, this, &CSettingsTab::addAd);
    connect(ui->buttonAdDel, &QPushButton::clicked, this, &CSettingsTab::delAd);
    connect(ui->buttonAdDelAll, &QPushButton::clicked, this, &CSettingsTab::delAdAll);
    connect(ui->buttonAdImport, &QPushButton::clicked, this, &CSettingsTab::importAd);
    connect(ui->buttonAdExport, &QPushButton::clicked, this, &CSettingsTab::exportAd);
    connect(ui->buttonFontColorOverride, &QPushButton::clicked, this, &CSettingsTab::fontColorDlg);
    connect(ui->buttonAddDictPath, &QPushButton::clicked, this, &CSettingsTab::addDictPath);
    connect(ui->buttonDelDictPath, &QPushButton::clicked, this, &CSettingsTab::delDictPath);
    connect(ui->buttonLoadedDicts, &QPushButton::clicked, this, &CSettingsTab::showLoadedDicts);
    connect(ui->buttonClearCookies, &QPushButton::clicked, this, &CSettingsTab::clearCookies);
    connect(ui->buttonDelCookies, &QPushButton::clicked, this, &CSettingsTab::delCookies);
    connect(ui->buttonExportCookies, &QPushButton::clicked, this, &CSettingsTab::exportCookies);
    connect(ui->buttonAdSearchBwd, &QPushButton::clicked, this, &CSettingsTab::adblockSearchBwd);
    connect(ui->buttonAdSearchFwd, &QPushButton::clicked, this, &CSettingsTab::adblockSearchFwd);
    connect(ui->buttonAddSearch, &QPushButton::clicked, this, &CSettingsTab::addSearchEngine);
    connect(ui->buttonDelSearch, &QPushButton::clicked, this, &CSettingsTab::delSearchEngine);
    connect(ui->buttonAtlClean, &QPushButton::clicked, this, &CSettingsTab::cleanAtlCerts);
    connect(ui->buttonDefaultSearch, &QPushButton::clicked, this, &CSettingsTab::setDefaultSearch);
    connect(ui->buttonUserScriptAdd, &QPushButton::clicked, this, &CSettingsTab::addUserScript);
    connect(ui->buttonUserScriptEdit, &QPushButton::clicked, this, &CSettingsTab::editUserScript);
    connect(ui->buttonUserScriptDelete, &QPushButton::clicked, this, &CSettingsTab::deleteUserScript);
    connect(ui->buttonUserScriptImport, &QPushButton::clicked, this, &CSettingsTab::importUserScript);
    connect(ui->buttonAddTrDir, &QPushButton::clicked, transModel, &CLangPairModel::addItem);
    connect(ui->buttonDelTrDir, &QPushButton::clicked, transModel, &CLangPairModel::deleteItem);
    connect(ui->buttonAddNoScript, &QPushButton::clicked, this, &CSettingsTab::addNoScriptHost);
    connect(ui->buttonDelNoScript, &QPushButton::clicked, this, &CSettingsTab::delNoScriptHost);
    connect(ui->buttonGcpJsonFile, &QPushButton::clicked, this, &CSettingsTab::selectGcpJsonFile);
    connect(ui->buttonXapianAddDir, &QPushButton::clicked, this, &CSettingsTab::addXapianIndexDir);
    connect(ui->buttonXapianDelDir, &QPushButton::clicked, this, &CSettingsTab::delXapianIndexDir);
    connect(ui->buttonMangaBkColor, &QPushButton::clicked, this, &CSettingsTab::mangaBackgroundColorDlg);

    connect(ui->editAdSearch, &QLineEdit::textChanged, this, &CSettingsTab::adblockSearch);

    connect(gSet->contentFilter(), &CGlobalContentFiltering::adblockRulesUpdated,
            this, &CSettingsTab::updateAdblockList, Qt::QueuedConnection);

    ui->atlSSLProto->addItem(QSL("Secure"),static_cast<int>(QSsl::SecureProtocols));
    ui->atlSSLProto->addItem(QSL("TLS 1.3+"),static_cast<int>(QSsl::TlsV1_3OrLater));
    ui->atlSSLProto->addItem(QSL("TLS 1.3"),static_cast<int>(QSsl::TlsV1_3));
    ui->atlSSLProto->addItem(QSL("TLS 1.2"),static_cast<int>(QSsl::TlsV1_2));
    ui->atlSSLProto->addItem(QSL("TLS 1.1"),static_cast<int>(QSsl::TlsV1_1));
    ui->atlSSLProto->addItem(QSL("TLS 1.0"),static_cast<int>(QSsl::TlsV1_0));
    ui->atlSSLProto->addItem(QSL("Any"),static_cast<int>(QSsl::AnyProtocol));
    updateAtlCertLabel();

    ui->comboAliMode->addItem(tr("General"),static_cast<int>(CStructures::aliTranslatorGeneral));
    ui->comboAliMode->addItem(tr("Title (e-commerce)"),static_cast<int>(CStructures::aliTranslatorECTitle));
    ui->comboAliMode->addItem(tr("Description (e-commerce)"),static_cast<int>(CStructures::aliTranslatorECDescription));
    ui->comboAliMode->addItem(tr("Communication (e-commerce)"),static_cast<int>(CStructures::aliTranslatorECCommunication));
    ui->comboAliMode->addItem(tr("Medical (e-commerce)"),static_cast<int>(CStructures::aliTranslatorECMedical));
    ui->comboAliMode->addItem(tr("Social (e-commerce)"),static_cast<int>(CStructures::aliTranslatorECSocial));

    ui->comboDeeplAPIMode->addItem(tr("DeepL Free"),static_cast<int>(CStructures::deeplAPIModeFree));
    ui->comboDeeplAPIMode->addItem(tr("DeepL Pro"),static_cast<int>(CStructures::deeplAPIModePro));
    ui->comboDeeplAPISplitSentences->addItem(tr("Default"),static_cast<int>(CStructures::deeplAPISplitDefault));
    ui->comboDeeplAPISplitSentences->addItem(tr("None"),static_cast<int>(CStructures::deeplAPISplitNone));
    ui->comboDeeplAPISplitSentences->addItem(tr("Full"),static_cast<int>(CStructures::deeplAPISplitFull));
    ui->comboDeeplAPISplitSentences->addItem(tr("No newlines"),static_cast<int>(CStructures::deeplAPISplitNoNewlines));
    ui->comboDeeplAPIFormality->addItem(tr("Default"),static_cast<int>(CStructures::deeplAPIFormalityDefault));
    ui->comboDeeplAPIFormality->addItem(tr("Less"),static_cast<int>(CStructures::deeplAPIFormalityLess));
    ui->comboDeeplAPIFormality->addItem(tr("More"),static_cast<int>(CStructures::deeplAPIFormalityMore));

    ui->comboXapianStemmerLang->addItem(tr("None"),QString());
    ui->comboXapianStemmerLang->addItem(tr("Arabic"),QSL("ar"));
    ui->comboXapianStemmerLang->addItem(tr("Armenian"),QSL("hy"));
    ui->comboXapianStemmerLang->addItem(tr("Basque"),QSL("eu"));
    ui->comboXapianStemmerLang->addItem(tr("Catalan"),QSL("ca"));
    ui->comboXapianStemmerLang->addItem(tr("Danish"),QSL("da"));
    ui->comboXapianStemmerLang->addItem(tr("Dutch"),QSL("nl"));
    ui->comboXapianStemmerLang->addItem(tr("English - Martin Porter's 2002 revision of his stemmer"),QSL("en"));
    ui->comboXapianStemmerLang->addItem(tr("Early English - e.g. Shakespeare, Dickens"),QSL("earlyenglish"));
    ui->comboXapianStemmerLang->addItem(tr("English - Lovin's stemmer"),QSL("lovins"));
    ui->comboXapianStemmerLang->addItem(tr("English - Porter's stemmer as described in his 1980 paper"),QSL("porter"));
    ui->comboXapianStemmerLang->addItem(tr("Finnish"),QSL("fi"));
    ui->comboXapianStemmerLang->addItem(tr("French"),QSL("fr"));
    ui->comboXapianStemmerLang->addItem(tr("German"),QSL("de"));
    ui->comboXapianStemmerLang->addItem(tr("German - Normalises umlauts and szlig"),QSL("german2"));
    ui->comboXapianStemmerLang->addItem(tr("Hungarian"),QSL("hu"));
    ui->comboXapianStemmerLang->addItem(tr("Indonesian"),QSL("id"));
    ui->comboXapianStemmerLang->addItem(tr("Irish"),QSL("ga"));
    ui->comboXapianStemmerLang->addItem(tr("Italian"),QSL("it"));
    ui->comboXapianStemmerLang->addItem(tr("Dutch different stemmer"),QSL("kraaij_pohlmann"));
    ui->comboXapianStemmerLang->addItem(tr("Lithuanian"),QSL("lt"));
    ui->comboXapianStemmerLang->addItem(tr("Nepali"),QSL("ne"));
    ui->comboXapianStemmerLang->addItem(tr("Norwegian"),QSL("no"));
    ui->comboXapianStemmerLang->addItem(tr("Portuguese"),QSL("pt"));
    ui->comboXapianStemmerLang->addItem(tr("Romanian"),QSL("ro"));
    ui->comboXapianStemmerLang->addItem(tr("Russian"),QSL("ru"));
    ui->comboXapianStemmerLang->addItem(tr("Spanish"),QSL("es"));
    ui->comboXapianStemmerLang->addItem(tr("Swedish"),QSL("sv"));
    ui->comboXapianStemmerLang->addItem(tr("Tamil"),QSL("ta"));
    ui->comboXapianStemmerLang->addItem(tr("Turkish"),QSL("tr"));

    setupSettingsObservers();

    ui->scrollArea->verticalScrollBar()->setValue(ui->scrollArea->verticalScrollBar()->minimum());

#ifndef WITH_XAPIAN
    ui->comboXapianStemmerLang->setEnabled(false);
    ui->spinXapianStartDelay->setEnabled(false);
    ui->listXapianIndexDirs->setEnabled(false);
    ui->radioSearchXapian->setEnabled(false);
#endif
}

CSettingsTab::~CSettingsTab()
{
    delete ui;
}

CSettingsTab *CSettingsTab::instance()
{
    static QPointer<CSettingsTab> inst;
    static QMutex lock;
    QMutexLocker locker(&lock);

    if (gSet==nullptr || gSet->activeWindow()==nullptr)
        return nullptr;

    CMainWindow* wnd = gSet->activeWindow();
    if (inst.isNull()) {
        inst = new CSettingsTab(wnd);
        inst->bindToTab(wnd->tabMain);
        connect(gSet->settings(),&CSettings::adblockRulesUpdated,
                inst,&CSettingsTab::updateAdblockList,Qt::QueuedConnection);
        inst->loadFromGlobal();
    }

    return inst;
}

void CSettingsTab::loadFromGlobal()
{
    if (gSet==nullptr) return;

    m_loadingInterlock = true;

    if (!gSet->m_settings->atlHostHistory.contains(gSet->m_settings->atlHost))
        gSet->m_settings->atlHostHistory.append(gSet->m_settings->atlHost);

    ui->spinMaxLimit->setValue(gSet->m_settings->maxSearchLimit);
    ui->spinMaxHistory->setValue(gSet->m_settings->maxHistory);
    ui->spinMaxRecent->setValue(gSet->m_settings->maxRecent);
    ui->editEditor->setText(gSet->m_settings->sysEditor);
    ui->editBrowser->setText(gSet->m_settings->sysBrowser);
    ui->spinMaxRecycled->setValue(gSet->m_settings->maxRecycled);
    ui->spinMaxWhiteListItems->setValue(gSet->m_settings->maxAdblockWhiteList);
    ui->checkJS->setChecked(gSet->m_browser->webProfile()->settings()->
                            testAttribute(QWebEngineSettings::JavascriptEnabled));
    ui->checkAutoloadImages->setChecked(gSet->m_browser->webProfile()->settings()->
                                        testAttribute(QWebEngineSettings::AutoLoadImages));
    ui->checkEnablePlugins->setChecked(gSet->m_browser->webProfile()->settings()->
                                       testAttribute(QWebEngineSettings::PluginsEnabled));

    switch (gSet->m_settings->translatorEngine) {
        case CStructures::teAtlas: ui->radioAtlas->setChecked(true); break;
        case CStructures::teBingAPI: ui->radioBingAPI->setChecked(true); break;
        case CStructures::teYandexAPI: ui->radioYandexAPI->setChecked(true); break;
        case CStructures::teGoogleGTX: ui->radioGoogleGTX->setChecked(true); break;
        case CStructures::teAmazonAWS: ui->radioAmazonAWS->setChecked(true); break;
        case CStructures::teYandexCloud: ui->radioYandexCloud->setChecked(true); break;
        case CStructures::teGoogleCloud: ui->radioGoogleCloud->setChecked(true); break;
        case CStructures::teAliCloud: ui->radioAliCloud->setChecked(true); break;
        case CStructures::teDeeplFree: ui->radioDeeplFree->setChecked(true); break;
        case CStructures::tePromtOneFree: ui->radioPromtOneFree->setChecked(true); break;
        case CStructures::tePromtNmtAPI: ui->radioPromtNmtAPI->setChecked(true); break;
        case CStructures::teDeeplAPI: ui->radioDeeplAPI->setChecked(true); break;
    }

    ui->atlHost->clear();
    ui->atlHost->addItems(gSet->m_settings->atlHostHistory);
    ui->atlHost->setEditText(gSet->m_settings->atlHost);
    ui->atlPort->setValue(gSet->m_settings->atlPort);
    ui->atlToken->setText(gSet->m_settings->atlToken);
    int idx = ui->atlSSLProto->findData(gSet->m_settings->atlProto);
    if (idx<0 || idx>=ui->atlSSLProto->count())
        idx = 0;
    ui->atlSSLProto->setCurrentIndex(idx);
    updateAtlCertLabel();
    ui->tranRetryCnt->setValue(gSet->m_settings->translatorRetryCount);
    ui->domWorkerRetryTimeoutSec->setValue(gSet->m_settings->domWorkerReplyTimeoutSec);

    ui->editBingKey->setText(gSet->m_settings->bingKey);
    ui->editYandexKey->setText(gSet->m_settings->yandexKey);
    ui->editAWSRegion->setText(gSet->m_settings->awsRegion);
    ui->editAWSAccessKey->setText(gSet->m_settings->awsAccessKey);
    ui->editAWSSecretKey->setText(gSet->m_settings->awsSecretKey);
    ui->editYandexCloudApiKey->setText(gSet->m_settings->yandexCloudApiKey);
    ui->editYandexCloudFolderID->setText(gSet->m_settings->yandexCloudFolderID);
    ui->editGcpJsonKey->setText(gSet->m_settings->gcpJsonKeyFile);
    ui->editAliAccessKeyID->setText(gSet->m_settings->aliAccessKeyID);
    ui->editAliAccessKeySecret->setText(gSet->m_settings->aliAccessKeySecret);
    ui->editPromtNmtAPIKey->setText(gSet->m_settings->promtNmtAPIKey);
    ui->editPromtNmtServer->setText(gSet->m_settings->promtNmtServer);
    ui->editDeeplAPIKey->setText(gSet->m_settings->deeplAPIKey);

    idx = ui->comboAliMode->findData(static_cast<int>(gSet->m_settings->aliCloudTranslatorMode));
    if (idx<0 || idx>=ui->comboAliMode->count())
        idx = 0;
    ui->comboAliMode->setCurrentIndex(idx);

    idx = ui->comboDeeplAPIMode->findData(static_cast<int>(gSet->m_settings->deeplAPIMode));
    if (idx<0 || idx>=ui->comboDeeplAPIMode->count())
        idx = 0;
    ui->comboDeeplAPIMode->setCurrentIndex(idx);
    idx = ui->comboDeeplAPISplitSentences->findData(static_cast<int>(gSet->m_settings->deeplAPISplitSentences));
    if (idx<0 || idx>=ui->comboDeeplAPISplitSentences->count())
        idx = 0;
    ui->comboDeeplAPISplitSentences->setCurrentIndex(idx);
    idx = ui->comboDeeplAPIFormality->findData(static_cast<int>(gSet->m_settings->deeplAPIFormality));
    if (idx<0 || idx>=ui->comboDeeplAPIFormality->count())
        idx = 0;
    ui->comboDeeplAPIFormality->setCurrentIndex(idx);

    ui->checkEmptyRestore->setChecked(gSet->m_settings->emptyRestore);
    ui->checkJSLogConsole->setChecked(gSet->m_settings->jsLogConsole);
    ui->checkDownloaderCleanCompleted->setChecked(gSet->m_settings->downloaderCleanCompleted);

    ui->checkUseAd->setChecked(gSet->m_settings->useAdblock);
    ui->checkUseNoScript->setChecked(gSet->m_settings->useNoScript);

    ui->checkTransFont->setChecked(gSet->m_actions->useOverrideTransFont());
    ui->checkStdFonts->setChecked(gSet->m_settings->overrideStdFonts);
    ui->transFont->setCurrentFont(gSet->m_settings->overrideTransFont);
    QFont f = QApplication::font();
    f.setFamily(gSet->m_settings->fontStandard);
    ui->fontStandard->setCurrentFont(f);
    f.setFamily(gSet->m_settings->fontFixed);
    ui->fontFixed->setCurrentFont(f);
    f.setFamily(gSet->m_settings->fontSerif);
    ui->fontSerif->setCurrentFont(f);
    f.setFamily(gSet->m_settings->fontSansSerif);
    ui->fontSansSerif->setCurrentFont(f);
    ui->transFontSize->setValue(gSet->m_settings->overrideTransFont.pointSize());
    ui->transFont->setEnabled(gSet->m_actions->useOverrideTransFont());
    ui->transFontSize->setEnabled(gSet->m_actions->useOverrideTransFont());
    ui->fontStandard->setEnabled(gSet->m_settings->overrideStdFonts);
    ui->fontFixed->setEnabled(gSet->m_settings->overrideStdFonts);
    ui->fontSerif->setEnabled(gSet->m_settings->overrideStdFonts);
    ui->fontSansSerif->setEnabled(gSet->m_settings->overrideStdFonts);
    ui->checkFontSizeOverride->setChecked(gSet->m_settings->overrideFontSizes);
    ui->spinFontSizeMinimum->setValue(gSet->m_settings->fontSizeMinimal);
    ui->spinFontSizeDefault->setValue(gSet->m_settings->fontSizeDefault);
    ui->spinFontSizeFixed->setValue(gSet->m_settings->fontSizeFixed);
    ui->spinFontSizeMinimum->setEnabled(gSet->m_settings->overrideFontSizes);
    ui->spinFontSizeDefault->setEnabled(gSet->m_settings->overrideFontSizes);
    ui->spinFontSizeFixed->setEnabled(gSet->m_settings->overrideFontSizes);
    ui->checkFontColorOverride->setChecked(gSet->m_actions->forceFontColor());

    ui->gctxHotkey->setKeySequence(gSet->m_settings->gctxSequence);
    ui->autofillHotkey->setKeySequence(gSet->m_settings->autofillSequence);
    ui->checkCreateCoredumps->setChecked(gSet->m_settings->createCoredumps);
#ifndef WITH_XAPIAN
    ui->radioSearchXapian->setEnabled(false);
#endif
#ifndef WITH_RECOLL
    ui->radioSearchRecoll->setEnabled(false);
#endif
#ifndef WITH_BALOO5
    ui->radioSearchBaloo5->setEnabled(false);
#endif
    if ((gSet->m_settings->searchEngine==CStructures::seRecoll) && (ui->radioSearchRecoll->isEnabled())) {
        ui->radioSearchRecoll->setChecked(true);
    } else if ((gSet->m_settings->searchEngine==CStructures::seBaloo5) && (ui->radioSearchBaloo5->isEnabled())) {
        ui->radioSearchBaloo5->setChecked(true);
    } else if ((gSet->m_settings->searchEngine==CStructures::seXapian) && (ui->radioSearchXapian->isEnabled())) {
        ui->radioSearchXapian->setChecked(true);
    } else {
        ui->radioSearchNone->setChecked(true);
    }

    ui->checkTabCloseBtn->setChecked(gSet->m_settings->showTabCloseButtons);

    ui->checkLogNetworkRequests->setChecked(gSet->m_actions->actionLogNetRequests->isChecked());
    ui->checkDumpHtml->setChecked(gSet->m_settings->debugDumpHtml);

    ui->checkUserAgent->setChecked(gSet->m_settings->overrideUserAgent);
    ui->editUserAgent->setEnabled(gSet->m_settings->overrideUserAgent);
    ui->editUserAgent->clear();
    ui->editUserAgent->addItems(gSet->m_settings->userAgentHistory);
    ui->editUserAgent->lineEdit()->setText(gSet->m_settings->userAgent);

    ui->listDictPaths->clear();
    ui->listDictPaths->addItems(gSet->m_settings->dictPaths);

    ui->checkDontUseNativeFileDialogs->setChecked(gSet->m_settings->dontUseNativeFileDialog);
    ui->spinCacheSize->setValue(gSet->m_browser->webProfile()->httpCacheMaximumSize()/CDefaults::oneMB);
    ui->checkIgnoreSSLErrors->setChecked(gSet->m_settings->ignoreSSLErrors);
    ui->checkTabFavicon->setChecked(gSet->m_browser->webProfile()->settings()->
                                    testAttribute(QWebEngineSettings::AutoLoadIconsForPage));

    ui->checkPdfExtractImages->setChecked(gSet->m_settings->pdfExtractImages);
    ui->spinPdfImageQuality->setValue(gSet->m_settings->pdfImageQuality);
    ui->spinPdfImageMaxSize->setValue(gSet->m_settings->pdfImageMaxSize);

    ui->checkPixivFetchImages->setChecked(gSet->m_settings->pixivFetchImages);

    ui->checkTranslatorCacheEnabled->setChecked(gSet->m_settings->translatorCacheEnabled);
    ui->spinTranslatorCacheSize->setValue(gSet->m_settings->translatorCacheSize);

    // flip proxy use check, for updating controls enabling logic
    ui->checkUseProxy->setChecked(true);
    ui->checkUseProxy->setChecked(false);

    int stemIdx = 0;
    if (!gSet->m_settings->xapianStemmerLang.isEmpty()) {
        stemIdx = ui->comboXapianStemmerLang->findData(gSet->m_settings->xapianStemmerLang);
        if (stemIdx<0)
            stemIdx = 0;
    }
    ui->comboXapianStemmerLang->setCurrentIndex(stemIdx);
    ui->spinXapianStartDelay->setValue(gSet->m_settings->getXapianTimerInterval());
    ui->listXapianIndexDirs->clear();
    ui->listXapianIndexDirs->addItems(gSet->m_settings->xapianIndexDirList);

    ui->spinMangaBlur->setValue(gSet->m_settings->mangaResizeBlur);
    ui->spinMangaCacheWidth->setValue(gSet->m_settings->mangaCacheWidth);
    ui->spinMangaMagnify->setValue(gSet->m_settings->mangaMagnifySize);
    ui->spinMangaScrollDelta->setValue(gSet->m_settings->mangaScrollDelta);
    ui->spinMangaScrollFactor->setValue(gSet->m_settings->mangaScrollFactor);
    ui->labelMangaDetectedDelta->setText(tr("Detected delta per one scroll event: %1 deg")
                                         .arg(gSet->m_ui->getMangaDetectedScrollDelta()));
    ui->checkMangaFineRendering->setChecked(gSet->m_settings->mangaUseFineRendering);
    ui->comboMangaUpscaleFilter->setCurrentIndex(static_cast<int>(gSet->m_settings->mangaUpscaleFilter));
    ui->comboMangaDownscaleFilter->setCurrentIndex(static_cast<int>(gSet->m_settings->mangaDownscaleFilter));
    ui->comboPixivMangaPageSize->setCurrentIndex(static_cast<int>(gSet->m_settings->pixivMangaPageSize));

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

    updateSearchEngines();
    updateCookiesTable();
    updateFontColorPreview();
    updateMainHistory();
    updateQueryHistory();
    updateAdblockList();
    updateNoScriptWhitelist();
    updateUserScripts();
    updateMangaBackgroundColorPreview();

    m_loadingInterlock = false;
}

void CSettingsTab::resizeEvent(QResizeEvent *event)
{
    const int transDirectionsColumnWidthFrac = 30;

    ui->listTransDirections->setColumnWidth(0, transDirectionsColumnWidthFrac*event->size().width()/100);
    ui->listTransDirections->setColumnWidth(1, transDirectionsColumnWidthFrac*event->size().width()/100);

    QWidget::resizeEvent(event);
}

QStringList CSettingsTab::getListStrings(QListWidget *list) const
{
    QStringList res;
    res.reserve(list->count());
    for (int i=0;i<list->count();i++)
        res.append(list->item(i)->text());
    return res;
}

void CSettingsTab::setupSettingsObservers()
{
    connect(ui->spinMaxLimit,qOverload<int>(&QSpinBox::valueChanged),this,[this](int val){
        if (m_loadingInterlock) return;
        gSet->m_settings->maxSearchLimit=val;
    });
    connect(ui->spinMaxHistory,qOverload<int>(&QSpinBox::valueChanged),this,[this](int val){
        if (m_loadingInterlock) return;
        gSet->m_settings->maxHistory=val;
    });
    connect(ui->editEditor,&QLineEdit::textChanged,this,[this](const QString& val){
        if (m_loadingInterlock) return;
        gSet->m_settings->sysEditor=val;
    });
    connect(ui->editBrowser,&QLineEdit::textChanged,this,[this](const QString& val){
        if (m_loadingInterlock) return;
        gSet->m_settings->sysBrowser=val;
    });
    connect(ui->spinMaxRecycled,qOverload<int>(&QSpinBox::valueChanged),this,[this](int val){
        if (m_loadingInterlock) return;
        gSet->m_settings->maxRecycled=val;
    });
    connect(ui->spinMaxRecent,qOverload<int>(&QSpinBox::valueChanged),this,[this](int val){
        if (m_loadingInterlock) return;
        gSet->m_settings->maxRecent=val;
    });
    connect(ui->spinMaxWhiteListItems,qOverload<int>(&QSpinBox::valueChanged),this,[this](int val){
        if (m_loadingInterlock) return;
        gSet->m_settings->maxAdblockWhiteList=val;
    });

    connect(ui->checkJS,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->m_browser->webProfile()->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled,val);
        gSet->m_actions->actionJSUsage->setChecked(val);
    });
    connect(ui->checkAutoloadImages,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->m_browser->webProfile()->settings()->setAttribute(QWebEngineSettings::AutoLoadImages,val);
    });
    connect(ui->checkEnablePlugins,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->m_browser->webProfile()->settings()->setAttribute(QWebEngineSettings::PluginsEnabled,val);
    });

    connect(ui->radioAtlas,&QRadioButton::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        if (val)
            gSet->m_net->setTranslationEngine(CStructures::teAtlas);
    });
    connect(ui->radioBingAPI,&QRadioButton::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        if (val)
            gSet->m_net->setTranslationEngine(CStructures::teBingAPI);
    });
    connect(ui->radioYandexAPI,&QRadioButton::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        if (val)
            gSet->m_net->setTranslationEngine(CStructures::teYandexAPI);
    });
    connect(ui->radioGoogleGTX,&QRadioButton::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        if (val)
            gSet->m_net->setTranslationEngine(CStructures::teGoogleGTX);
    });
    connect(ui->radioAmazonAWS,&QRadioButton::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        if (val)
            gSet->m_net->setTranslationEngine(CStructures::teAmazonAWS);
    });
    connect(ui->radioYandexCloud,&QRadioButton::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        if (val)
            gSet->m_net->setTranslationEngine(CStructures::teYandexCloud);
    });
    connect(ui->radioGoogleCloud,&QRadioButton::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        if (val)
            gSet->m_net->setTranslationEngine(CStructures::teGoogleCloud);
    });
    connect(ui->radioAliCloud,&QRadioButton::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        if (val)
            gSet->m_net->setTranslationEngine(CStructures::teAliCloud);
    });
    connect(ui->radioDeeplFree,&QRadioButton::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        if (val)
            gSet->m_net->setTranslationEngine(CStructures::teDeeplFree);
    });
    connect(ui->radioPromtOneFree,&QRadioButton::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        if (val)
            gSet->m_net->setTranslationEngine(CStructures::tePromtOneFree);
    });
    connect(ui->radioPromtNmtAPI,&QRadioButton::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        if (val)
            gSet->m_net->setTranslationEngine(CStructures::tePromtNmtAPI);
    });

    connect(ui->atlHost->lineEdit(),&QLineEdit::editingFinished,this,[this](){
        if (m_loadingInterlock) return;
        const QString val = ui->atlHost->lineEdit()->text();
        gSet->m_settings->atlHost=val;
        if (gSet->m_settings->atlHostHistory.contains(val)) {
            gSet->m_settings->atlHostHistory.move(gSet->m_settings->atlHostHistory.indexOf(val),0);
        } else {
            gSet->m_settings->atlHostHistory.prepend(val);
        }
    });
    connect(ui->atlPort,qOverload<int>(&QSpinBox::valueChanged),this,[this](int val){
        if (m_loadingInterlock) return;
        gSet->m_settings->atlPort=static_cast<quint16>(val);
    });
    connect(ui->tranRetryCnt,qOverload<int>(&QSpinBox::valueChanged),this,[this](int val){
        if (m_loadingInterlock) return;
        gSet->m_settings->translatorRetryCount=val;
    });
    connect(ui->domWorkerRetryTimeoutSec,qOverload<int>(&QSpinBox::valueChanged),this,[this](int val){
        if (m_loadingInterlock) return;
        gSet->m_settings->domWorkerReplyTimeoutSec=val;
    });
    connect(ui->atlToken,&QLineEdit::textChanged,this,[this](const QString& val){
        if (m_loadingInterlock) return;
        gSet->m_settings->atlToken=val;
    });
    connect(ui->atlSSLProto,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int val){
        Q_UNUSED(val)
        if (m_loadingInterlock) return;
        gSet->m_settings->atlProto=static_cast<QSsl::SslProtocol>(ui->atlSSLProto->currentData().toInt());
    });

    connect(ui->editBingKey,&QLineEdit::textChanged,this,[this](const QString& val){
        if (m_loadingInterlock) return;
        gSet->m_settings->bingKey=val;
    });
    connect(ui->editYandexKey,&QLineEdit::textChanged,this,[this](const QString& val){
        if (m_loadingInterlock) return;
        gSet->m_settings->yandexKey=val;
    });
    connect(ui->editAWSRegion,&QLineEdit::textChanged,this,[this](const QString& val){
        if (m_loadingInterlock) return;
        gSet->m_settings->awsRegion=val;
    });
    connect(ui->editAWSAccessKey,&QLineEdit::textChanged,this,[this](const QString& val){
        if (m_loadingInterlock) return;
        gSet->m_settings->awsAccessKey=val;
    });
    connect(ui->editAWSSecretKey,&QLineEdit::textChanged,this,[this](const QString& val){
        if (m_loadingInterlock) return;
        gSet->m_settings->awsSecretKey=val;
    });
    connect(ui->editYandexCloudApiKey,&QLineEdit::textChanged,this,[this](const QString& val){
        if (m_loadingInterlock) return;
        gSet->m_settings->yandexCloudApiKey=val;
    });
    connect(ui->editYandexCloudFolderID,&QLineEdit::textChanged,this,[this](const QString& val){
        if (m_loadingInterlock) return;
        gSet->m_settings->yandexCloudFolderID=val;
    });
    connect(ui->editGcpJsonKey,&QLineEdit::textChanged,this,[this](const QString& val){
        if (m_loadingInterlock) return;
        gSet->m_settings->gcpJsonKeyFile=val;
    });
    connect(ui->editAliAccessKeyID,&QLineEdit::textChanged,this,[this](const QString& val){
        if (m_loadingInterlock) return;
        gSet->m_settings->aliAccessKeyID=val;
    });
    connect(ui->editAliAccessKeySecret,&QLineEdit::textChanged,this,[this](const QString& val){
        if (m_loadingInterlock) return;
        gSet->m_settings->aliAccessKeySecret=val;
    });
    connect(ui->comboAliMode,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int val){
        Q_UNUSED(val)
        if (m_loadingInterlock) return;
        gSet->m_settings->aliCloudTranslatorMode = static_cast<CStructures::AliCloudTranslatorMode>
                                                   (ui->comboAliMode->currentData().toInt());
    });
    connect(ui->editPromtNmtAPIKey,&QLineEdit::textChanged,this,[this](const QString& val){
        if (m_loadingInterlock) return;
        gSet->m_settings->promtNmtAPIKey=val;
    });
    connect(ui->editPromtNmtServer,&QLineEdit::textChanged,this,[this](const QString& val){
        if (m_loadingInterlock) return;
        gSet->m_settings->promtNmtServer=val;
    });

    connect(ui->editDeeplAPIKey,&QLineEdit::textChanged,this,[this](const QString& val){
        if (m_loadingInterlock) return;
        gSet->m_settings->deeplAPIKey=val;
    });
    connect(ui->comboDeeplAPIMode,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int val){
        Q_UNUSED(val)
        if (m_loadingInterlock) return;
        gSet->m_settings->deeplAPIMode = static_cast<CStructures::DeeplAPIMode>
                                         (ui->comboDeeplAPIMode->currentData().toInt());
    });
    connect(ui->comboDeeplAPISplitSentences,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int val){
        Q_UNUSED(val)
        if (m_loadingInterlock) return;
        gSet->m_settings->deeplAPISplitSentences = static_cast<CStructures::DeeplAPISplitSentences>
                                                   (ui->comboDeeplAPISplitSentences->currentData().toInt());
    });
    connect(ui->comboDeeplAPIFormality,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int val){
        Q_UNUSED(val)
        if (m_loadingInterlock) return;
        gSet->m_settings->deeplAPIFormality = static_cast<CStructures::DeeplAPIFormality>
                                              (ui->comboDeeplAPIFormality->currentData().toInt());
    });

    connect(ui->checkEmptyRestore,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->m_settings->emptyRestore=val;
    });
    connect(ui->checkJSLogConsole,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->m_settings->jsLogConsole=val;
    });
    connect(ui->checkDownloaderCleanCompleted,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->m_settings->downloaderCleanCompleted=val;
    });

    connect(ui->checkTransFont,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->m_actions->actionOverrideTransFont->setChecked(val);
    });
    connect(ui->checkStdFonts,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->m_settings->overrideStdFonts=val;
    });
    connect(ui->transFont,&QFontComboBox::currentFontChanged,this,[this](const QFont& val){
        if (m_loadingInterlock) return;
        gSet->m_settings->overrideTransFont=val;
    });
    connect(ui->transFontSize,qOverload<int>(&QSpinBox::valueChanged),this,[this](int val){
        if (m_loadingInterlock) return;
        gSet->m_settings->overrideTransFont.setPointSize(val);
    });
    connect(ui->fontStandard,&QFontComboBox::currentFontChanged,this,[this](const QFont& val){
        if (m_loadingInterlock) return;
        gSet->m_settings->fontStandard=val.family();
    });
    connect(ui->fontFixed,&QFontComboBox::currentFontChanged,this,[this](const QFont& val){
        if (m_loadingInterlock) return;
        gSet->m_settings->fontFixed=val.family();
    });
    connect(ui->fontSerif,&QFontComboBox::currentFontChanged,this,[this](const QFont& val){
        if (m_loadingInterlock) return;
        gSet->m_settings->fontSerif=val.family();
    });
    connect(ui->fontSansSerif,&QFontComboBox::currentFontChanged,this,[this](const QFont& val){
        if (m_loadingInterlock) return;
        gSet->m_settings->fontSansSerif=val.family();
    });
    connect(ui->checkFontSizeOverride,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->m_settings->overrideFontSizes=val;
    });
    connect(ui->spinFontSizeMinimum,qOverload<int>(&QSpinBox::valueChanged),this,[this](int val){
        if (m_loadingInterlock) return;
        gSet->m_settings->fontSizeMinimal=val;
    });
    connect(ui->spinFontSizeDefault,qOverload<int>(&QSpinBox::valueChanged),this,[this](int val){
        if (m_loadingInterlock) return;
        gSet->m_settings->fontSizeDefault=val;
    });
    connect(ui->spinFontSizeFixed,qOverload<int>(&QSpinBox::valueChanged),this,[this](int val){
        if (m_loadingInterlock) return;
        gSet->m_settings->fontSizeFixed=val;
    });


    connect(ui->checkFontColorOverride,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->m_actions->actionOverrideTransFontColor->setChecked(val);
    });

    connect(ui->checkUseAd,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->m_settings->useAdblock=val;
    });
    connect(ui->checkUseNoScript,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->m_settings->useNoScript=val;
    });

    connect(ui->gctxHotkey,&QKeySequenceEdit::keySequenceChanged,this,[this](const QKeySequence& val){
        if (m_loadingInterlock) return;
        gSet->m_settings->gctxSequence = val;
        gSet->m_actions->rebindGctxHotkey();
    });
    connect(ui->autofillHotkey,&QKeySequenceEdit::keySequenceChanged,this,[this](const QKeySequence& val){
        if (m_loadingInterlock) return;
        gSet->m_settings->autofillSequence = val;
    });

    connect(ui->radioSearchRecoll,&QRadioButton::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        if (val)
            gSet->m_settings->searchEngine = CStructures::seRecoll;
    });
    connect(ui->radioSearchBaloo5,&QRadioButton::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        if (val)
            gSet->m_settings->searchEngine = CStructures::seBaloo5;
    });
    connect(ui->radioSearchXapian,&QRadioButton::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        if (val)
            gSet->m_settings->searchEngine = CStructures::seXapian;
    });
    connect(ui->radioSearchNone,&QRadioButton::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        if (val)
            gSet->m_settings->searchEngine = CStructures::seNone;
    });

    connect(ui->checkTabCloseBtn,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->m_settings->showTabCloseButtons = val;
    });

    connect(ui->checkCreateCoredumps,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->m_settings->createCoredumps=val;
    });
    connect(ui->checkLogNetworkRequests,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->m_actions->actionLogNetRequests->setChecked(val);
    });
    connect(ui->checkDumpHtml,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->m_settings->debugDumpHtml = val;
    });

    connect(ui->checkUserAgent,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->m_settings->overrideUserAgent = val;
        if (gSet->m_settings->overrideUserAgent && !gSet->m_settings->userAgent.isEmpty()) {
            gSet->m_browser->webProfile()->setHttpUserAgent(gSet->m_settings->userAgent);
        } else {
            gSet->m_browser->webProfile()->setHttpUserAgent(QWebEngineProfile::defaultProfile()->httpUserAgent());
        }
    });
    connect(ui->editUserAgent->lineEdit(),&QLineEdit::editingFinished,this,[this](){
        if (m_loadingInterlock) return;
        const QString val = ui->editUserAgent->lineEdit()->text();
        gSet->m_settings->userAgent = val;
        if (gSet->m_settings->overrideUserAgent && !gSet->m_settings->userAgent.isEmpty()) {
            gSet->m_browser->webProfile()->setHttpUserAgent(gSet->m_settings->userAgent);
        } else {
            gSet->m_browser->webProfile()->setHttpUserAgent(QWebEngineProfile::defaultProfile()->httpUserAgent());
        }
    });

    connect(ui->checkPdfExtractImages,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->m_settings->pdfExtractImages = val;
    });
    connect(ui->spinPdfImageMaxSize,qOverload<int>(&QSpinBox::valueChanged),this,[this](int val){
        if (m_loadingInterlock) return;
        gSet->m_settings->pdfImageMaxSize = val;
    });
    connect(ui->spinPdfImageQuality,qOverload<int>(&QSpinBox::valueChanged),this,[this](int val){
        if (m_loadingInterlock) return;
        gSet->m_settings->pdfImageQuality = val;
    });

    connect(ui->checkPixivFetchImages,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->m_settings->pixivFetchImages = val;
    });

    connect(ui->checkTranslatorCacheEnabled,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->m_settings->translatorCacheEnabled = val;
    });
    connect(ui->spinTranslatorCacheSize,qOverload<int>(&QSpinBox::valueChanged),this,[this](int val){
        if (m_loadingInterlock) return;
        gSet->m_settings->translatorCacheSize = val;
    });

    connect(ui->checkDontUseNativeFileDialogs,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->m_settings->dontUseNativeFileDialog = val;
    });
    connect(ui->spinCacheSize,qOverload<int>(&QSpinBox::valueChanged),this,[this](int val){
        if (m_loadingInterlock) return;
        gSet->m_browser->webProfile()->setHttpCacheMaximumSize(val*CDefaults::oneMB);
    });
    connect(ui->checkIgnoreSSLErrors,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->m_settings->ignoreSSLErrors = val;
    });
    connect(ui->checkTabFavicon,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->m_browser->webProfile()->settings()->setAttribute(QWebEngineSettings::AutoLoadIconsForPage,val);
    });


    connect(ui->spinXapianStartDelay,qOverload<int>(&QSpinBox::valueChanged),this,[this](int val){
        if (m_loadingInterlock) return;
        gSet->m_settings->setupXapianTimerInterval(gSet,val);
    });
    connect(ui->comboXapianStemmerLang,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int val){
        Q_UNUSED(val)
        if (m_loadingInterlock) return;
        gSet->m_settings->xapianStemmerLang = ui->comboXapianStemmerLang->currentData().toString();
    });

    connect(ui->editProxyHost,&QLineEdit::textChanged,this,[this](const QString& val){
        if (m_loadingInterlock) return;
        gSet->m_settings->proxyHost = val;
        gSet->m_net->updateProxyWithMenuUpdate(gSet->m_settings->proxyUse,true);
    });
    connect(ui->spinProxyPort,qOverload<int>(&QSpinBox::valueChanged),this,[this](int val){
        if (m_loadingInterlock) return;
        gSet->m_settings->proxyPort = static_cast<quint16>(val);
        gSet->m_net->updateProxyWithMenuUpdate(gSet->m_settings->proxyUse,true);
    });
    connect(ui->editProxyLogin,&QLineEdit::textChanged,this,[this](const QString& val){
        if (m_loadingInterlock) return;
        gSet->m_settings->proxyLogin = val;
        gSet->m_net->updateProxyWithMenuUpdate(gSet->m_settings->proxyUse,true);
    });
    connect(ui->editProxyPassword,&QLineEdit::textChanged,this,[this](const QString& val){
        if (m_loadingInterlock) return;
        gSet->m_settings->proxyPassword = val;
        gSet->m_net->updateProxyWithMenuUpdate(gSet->m_settings->proxyUse,true);
    });
    connect(ui->checkUseProxy,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->m_settings->proxyUse = val;
        gSet->m_net->updateProxyWithMenuUpdate(gSet->m_settings->proxyUse,true);
    });
    connect(ui->checkUseProxyTranslator,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->m_settings->proxyUseTranslator = val;
        gSet->m_net->updateProxyWithMenuUpdate(gSet->m_settings->proxyUse,true);
    });
    connect(ui->listProxyType,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int val){
        if (m_loadingInterlock) return;
        switch (val) {
            case 0: gSet->m_settings->proxyType = QNetworkProxy::HttpCachingProxy; break;
            case 1: gSet->m_settings->proxyType = QNetworkProxy::HttpProxy; break;
            case 2: gSet->m_settings->proxyType = QNetworkProxy::Socks5Proxy; break;
            default: gSet->m_settings->proxyType = QNetworkProxy::HttpCachingProxy; break;
        }
        gSet->m_net->updateProxyWithMenuUpdate(gSet->m_settings->proxyUse,true);
    });

    connect(ui->spinMangaBlur,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this](double val){
        if (m_loadingInterlock) return;
        gSet->m_settings->mangaResizeBlur=val;
        Q_EMIT gSet->m_settings->mangaViewerSettingsUpdated();
    });
    connect(ui->spinMangaCacheWidth,qOverload<int>(&QSpinBox::valueChanged),this,[this](int val){
        if (m_loadingInterlock) return;
        gSet->m_settings->mangaCacheWidth=val;
        Q_EMIT gSet->m_settings->mangaViewerSettingsUpdated();
    });
    connect(ui->spinMangaMagnify,qOverload<int>(&QSpinBox::valueChanged),this,[this](int val){
        if (m_loadingInterlock) return;
        gSet->m_settings->mangaMagnifySize=val;
        Q_EMIT gSet->m_settings->mangaViewerSettingsUpdated();
    });
    connect(ui->spinMangaScrollDelta,qOverload<int>(&QSpinBox::valueChanged),this,[this](int val){
        if (m_loadingInterlock) return;
        gSet->m_settings->mangaScrollDelta=val;
        Q_EMIT gSet->m_settings->mangaViewerSettingsUpdated();
    });
    connect(ui->spinMangaScrollFactor,qOverload<int>(&QSpinBox::valueChanged),this,[this](int val){
        if (m_loadingInterlock) return;
        gSet->m_settings->mangaScrollFactor=val;
        Q_EMIT gSet->m_settings->mangaViewerSettingsUpdated();
    });
    connect(ui->checkMangaFineRendering,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->m_settings->mangaUseFineRendering = val;
        Q_EMIT gSet->m_settings->mangaViewerSettingsUpdated();
    });
    connect(ui->comboMangaUpscaleFilter,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int val){
        if (m_loadingInterlock) return;
        gSet->m_settings->mangaUpscaleFilter = static_cast<Blitz::ScaleFilterType>(val);
        Q_EMIT gSet->m_settings->mangaViewerSettingsUpdated();
    });
    connect(ui->comboMangaDownscaleFilter,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int val){
        if (m_loadingInterlock) return;
        gSet->m_settings->mangaDownscaleFilter = static_cast<Blitz::ScaleFilterType>(val);
        Q_EMIT gSet->m_settings->mangaViewerSettingsUpdated();
    });
    connect(ui->comboPixivMangaPageSize,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int val){
        if (m_loadingInterlock) return;
        gSet->m_settings->pixivMangaPageSize = static_cast<CStructures::PixivMangaPageSize>(val);
    });
}

void CSettingsTab::selectBrowser()
{
    QString s = CGenericFuncs::getOpenFileNameD(this,tr("Select browser"),ui->editBrowser->text());
    if (!s.isEmpty()) ui->editBrowser->setText(s);
}

void CSettingsTab::selectEditor()
{
    QString s = CGenericFuncs::getOpenFileNameD(this,tr("Select editor"),ui->editEditor->text());
    if (!s.isEmpty()) ui->editEditor->setText(s);
}

void CSettingsTab::selectGcpJsonFile()
{
    QString s = CGenericFuncs::getOpenFileNameD(this,tr("Select GCP service JSON key"),ui->editGcpJsonKey->text(),
                                                QStringList( { QSL("JSON GCP service access key (*.json)") }));
    if (!s.isEmpty()) ui->editGcpJsonKey->setText(s);
}

void CSettingsTab::addXapianIndexDir()
{
    const QString s = CGenericFuncs::getExistingDirectoryD(this,tr("Select directory"));
    if (!s.isEmpty()) {
        ui->listXapianIndexDirs->addItem(s);
        gSet->m_settings->updateXapianIndexDirs(getListStrings(ui->listXapianIndexDirs));
    }
}

void CSettingsTab::delXapianIndexDir()
{
    int idx = ui->listXapianIndexDirs->currentRow();
    if (idx<0 || idx>=ui->listXapianIndexDirs->count()) return;
    QListWidgetItem *a = ui->listXapianIndexDirs->takeItem(idx);
    delete a;
    gSet->m_settings->updateXapianIndexDirs(getListStrings(ui->listXapianIndexDirs));
}

void CSettingsTab::mangaBackgroundColorDlg()
{
    QColor c = QColorDialog::getColor(gSet->m_settings->mangaBackgroundColor,this);
    if (!c.isValid()) return;
    gSet->m_settings->setMangaBackgroundColor(c);
    updateMangaBackgroundColorPreview();
}

void CSettingsTab::fontColorDlg()
{
    QColor c = QColorDialog::getColor(gSet->m_settings->forcedFontColor,this);
    if (!c.isValid()) return;
    gSet->m_settings->forcedFontColor = c;
    updateFontColorPreview();
}

QList<int> CSettingsTab::getSelectedRows(QTableWidget *table) const
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


// *************** Dictionaries ************************

void CSettingsTab::addDictPath()
{
    const QString s = CGenericFuncs::getExistingDirectoryD(this,tr("Select directory"));
    if (!s.isEmpty()) {
        ui->listDictPaths->addItem(s);
        saveDictPaths();
    }
}

void CSettingsTab::delDictPath()
{
    int idx = ui->listDictPaths->currentRow();
    if (idx<0 || idx>=ui->listDictPaths->count()) return;
    QListWidgetItem *a = ui->listDictPaths->takeItem(idx);
    delete a;
    saveDictPaths();
}

void CSettingsTab::saveDictPaths()
{
    const QStringList dirs = getListStrings(ui->listDictPaths);

    if (CGenericFuncs::compareStringLists(gSet->m_settings->dictPaths,dirs)!=0) {
        gSet->m_settings->dictPaths = dirs;
        gSet->d_func()->dictManager->loadDictionaries(
                    gSet->m_settings->dictPaths);
    }
}

void CSettingsTab::showLoadedDicts()
{
    QString msg = tr("No dictionaries loaded.");

    const QStringList loadedDicts = gSet->d_func()->dictManager->getLoadedDictionaries();
    if (!loadedDicts.isEmpty())
        msg = tr("Loaded %1 dictionaries:\n").arg(loadedDicts.count())+loadedDicts.join(u'\n');
    QMessageBox::information(this,QGuiApplication::applicationDisplayName(),msg);
}


// ************************ Browsing history ***********************

void CSettingsTab::clearHistory()
{
    if (gSet) {
        gSet->d_func()->mainHistory.clear();
        Q_EMIT gSet->m_history->updateAllHistoryLists();
    }
    updateMainHistory();
}

void CSettingsTab::goHistory()
{
    QListWidgetItem* it = ui->listHistory->currentItem();
    if (it==nullptr || gSet->activeWindow()==nullptr) return;
    QUuid idx(it->data(Qt::UserRole).toString());
    gSet->d_func()->activeWindow->goHistory(idx);
}

void CSettingsTab::updateMainHistory()
{
    ui->listHistory->clear();
    for (const CUrlHolder &t : qAsConst(gSet->d_func()->mainHistory)) {
        auto *li = new QListWidgetItem(QSL("%1 [ %2 ]")
                                                  .arg(t.title, t.url.toString()));
        li->setData(Qt::UserRole,t.uuid.toString());
        ui->listHistory->addItem(li);
    }
}


// *********************** Cookies **********************

void CSettingsTab::updateCookiesTable()
{
    const static QStringList horizontalLabels( { tr("Domain"), tr("Name"), tr("Path"),
                                                 tr("Expiration"), tr("Value") } );
    QTableWidget *table = ui->tableCookies;

    table->setSortingEnabled(false);
    table->clear();

    auto *cj = qobject_cast<CNetworkCookieJar *>(gSet->d_func()->auxNetManager->cookieJar());
    if (cj==nullptr) return;
    auto cookiesList = cj->getAllCookies();

    table->setColumnCount(horizontalLabels.count());
    table->setRowCount(cookiesList.count());
    table->setHorizontalHeaderLabels(horizontalLabels);
    for (int i=0;i<cookiesList.count();i++) {

        QString s = cookiesList.at(i).domain();
        auto *item = new QTableWidgetItem(s);
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

        QLocale locale;
        s = locale.toString(cookiesList.at(i).expirationDate(),QLocale::ShortFormat);
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

void CSettingsTab::clearCookies()
{
    QWebEngineCookieStore* wsc = gSet->m_browser->webProfile()->cookieStore();
    if (wsc)
        wsc->deleteAllCookies();

    updateCookiesTable();
}

void CSettingsTab::delCookies()
{
    auto *cj = qobject_cast<CNetworkCookieJar *>(gSet->d_func()->auxNetManager->cookieJar());
    if (cj==nullptr) return;
    auto cookiesList = cj->getAllCookies();

    QList<int> r = getSelectedRows(ui->tableCookies);
    if (gSet->m_browser->webProfile()->cookieStore()) {
        for (int idx : qAsConst(r))
            gSet->m_browser->webProfile()->cookieStore()->deleteCookie(cookiesList.at(idx));

        updateCookiesTable();
    }
}

void CSettingsTab::exportCookies()
{
    QList<int> r = getSelectedRows(ui->tableCookies);
    if (r.isEmpty()) {
        QMessageBox::information(this,QGuiApplication::applicationDisplayName(),
                                 tr("Please select cookies for export."));
        return;
    }

    QString fname = CGenericFuncs::getSaveFileNameD(this,tr("Save cookies to file"),gSet->settings()->savedAuxSaveDir,
                                                    QStringList( { tr("Text file, Netscape format (*.txt)") } ) );

    if (fname.isEmpty() || fname.isNull()) return;
    gSet->m_ui->setSavedAuxSaveDir(QFileInfo(fname).absolutePath());

    if (!gSet->m_net->exportCookies(fname,QUrl(),r)) {
        QMessageBox::critical(this,QGuiApplication::applicationDisplayName(),
                              tr("Unable to create file."));
    }
}


// *********************** AdBlock ************************************

void CSettingsTab::updateAdblockList()
{
    const auto &adblockList = gSet->d_func()->adblock;

    ui->treeAdblock->setHeaderLabels(QStringList() << tr("AdBlock patterns"));
    QHash<QString,QTreeWidgetItem*> cats;
    ui->treeAdblock->clear();
    for (size_t i=0; i<adblockList.size(); i++) {
        QTreeWidgetItem* tli = nullptr;
        QString cat = adblockList.at(i).listID();
        if (cats.contains(cat)) {
            tli = cats.value(cat);
        } else {
            tli = new QTreeWidgetItem(ui->treeAdblock);
            tli->setText(0,cat);
            cats[cat] = tli;
        }

        auto *item = new QTreeWidgetItem(tli);
        item->setText(0,adblockList.at(i).filter());
        item->setData(0,Qt::UserRole+1,static_cast<qulonglong>(i));
    }
    ui->lblAdblockTotalRules->setText(
                tr("Total rules: %1.").arg(adblockList.size()));
}

void CSettingsTab::adblockFocusSearchedRule(QList<QTreeWidgetItem *> & items)
{
    if (adblockSearchIdx>=0 && adblockSearchIdx<items.count())
        ui->treeAdblock->setCurrentItem(items.at(adblockSearchIdx));
}

void CSettingsTab::adblockSearch(const QString &text)
{
    QString s = text.trimmed();
    if (s.isEmpty()) return;
    QList<QTreeWidgetItem *> it = ui->treeAdblock->findItems(s,Qt::MatchContains | Qt::MatchRecursive);
    if (it.isEmpty()) return;
    adblockSearchIdx = 0;
    adblockFocusSearchedRule(it);
}

void CSettingsTab::adblockSearchBwd()
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

void CSettingsTab::adblockSearchFwd()
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

void CSettingsTab::addAd()
{
    bool ok = false;
    QString s = QInputDialog::getText(this,tr("Add AdBlock rule"),tr("Filter template"),
                                      QLineEdit::Normal,QString(),&ok);
    if (!ok) return;

    gSet->contentFilter()->adblockAppend(s);
}

void CSettingsTab::delAd()
{
    CAdBlockVector r;
    const QList<QTreeWidgetItem *> il = ui->treeAdblock->selectedItems();
    r.reserve(il.count());
    for (const QTreeWidgetItem* i : il)
        r.push_back(gSet->d_func()->adblock.at(i->data(0,Qt::UserRole+1).toULongLong()));

    gSet->contentFilter()->adblockDelete(r);
}

void CSettingsTab::delAdAll()
{
    gSet->contentFilter()->adblockClear();
}

void CSettingsTab::importAd()
{
    QString fname = CGenericFuncs::getOpenFileNameD(this,tr("Import rules from text file"),QDir::homePath());
    if (fname.isEmpty()) return;

    QFileInfo fi(fname);
    const QString baseFileName = fi.fileName();

    QFile f(fname);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this,QGuiApplication::applicationDisplayName(),
                             tr("Unable to open file"));
        return;
    }
    QTextStream fs(&f);
    QStringList ruleStrings;
    while (!fs.atEnd())
        ruleStrings.append(fs.readLine());
    f.close();

    int psz = gSet->d_func()->adblock.size();
    QAtomicInteger<int> cssRule = 0;
    std::for_each(std::execution::par,ruleStrings.constBegin(),ruleStrings.constEnd(),
                  [baseFileName,&cssRule](const QString& line){
        CAdBlockRule rule = CAdBlockRule(line,baseFileName);
        if (!rule.isCSSRule()) {
            gSet->contentFilter()->adblockAppend(rule,true);
        } else {
            cssRule++;
        }
    });

    updateAdblockList();
    gSet->d_func()->clearAdblockWhiteList();

    psz = gSet->d_func()->adblock.size() - psz;
    QMessageBox::information(this,QGuiApplication::applicationDisplayName(),
                             tr("%1 rules imported, %2 CSS rules dropped.").arg(psz).arg(cssRule));
}

void CSettingsTab::exportAd()
{
    if (ui->treeAdblock->selectedItems().isEmpty()) {
        QMessageBox::information(this,QGuiApplication::applicationDisplayName(),
                                 tr("Please select patterns for export."));
        return;
    }

    CAdBlockVector r;
    const QList<QTreeWidgetItem *> il = ui->treeAdblock->selectedItems();
    r.reserve(il.count());
    for (const QTreeWidgetItem* i : il)
        r.push_back(gSet->d_func()->adblock.at(i->data(0,Qt::UserRole+1).toULongLong()));

    QString fname = CGenericFuncs::getSaveFileNameD(this,tr("Save AdBlock patterns to file"),
                                                    gSet->settings()->savedAuxSaveDir,
                                                    QStringList( { tr("Text file (*.txt)") } ) );

    if (fname.isEmpty() || fname.isNull()) return;
    gSet->m_ui->setSavedAuxSaveDir(QFileInfo(fname).absolutePath());

    QFile f(fname);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this,QGuiApplication::applicationDisplayName(),
                             tr("Unable to create file"));
        return;
    }
    QTextStream fs(&f);

    for (const auto& rule : qAsConst(r))
        fs << rule.filter() << Qt::endl;

    fs.flush();
    f.close();
}


// ******************* Context Search Engine ******************

void CSettingsTab::updateSearchEngines()
{
    ui->listSearch->clear();
    const auto &engines = gSet->d_func()->ctxSearchEngines;
    for (auto it = engines.constBegin(), end = engines.constEnd(); it != end; ++it) {
        auto *li = new QListWidgetItem(QSL("%1 [ %2 ] %3").
                                                  arg(it.key(),
                                                      it.value(),
                                                      it.key()==gSet->m_settings->defaultSearchEngine ?
                                                          tr("(default)") : QString()));
        li->setData(Qt::UserRole,it.key());
        li->setData(Qt::UserRole+1,it.value());
        ui->listSearch->addItem(li);
    }
}

void CSettingsTab::updateMangaBackgroundColorPreview()
{
    ui->frameMangaBkColor->setStyleSheet(QSL("QFrame { background: %1; }")
                                         .arg(gSet->m_settings->mangaBackgroundColor.name(QColor::HexRgb)));
}

void CSettingsTab::addSearchEngine()
{
    CStringHash data;
    data[QSL("Url template")]=QString();
    data[QSL("Menu title")]=QString();

    static const QString hlp = tr("In the url template you can use following substitutions\n"
                                  "  %s - search text\n"
                                  "  %ps - percent-encoded search text");

    auto *dlg = new CMultiInputDialog(this,tr("Add new search engine"),data,hlp);
    if (dlg->exec()==QDialog::Accepted) {
        data = dlg->getInputData();

        auto *li = new QListWidgetItem(QSL("%1 [ %2 ] %3").
                                                  arg(data[QSL("Menu title")],
                                                  data[QSL("Url template")],
                data[QSL("Menu title")]==gSet->m_settings->defaultSearchEngine ?
                    tr("(default)") : QString()));
        li->setData(Qt::UserRole,data[QSL("Menu title")]);
        li->setData(Qt::UserRole+1,data[QSL("Url template")]);
        ui->listSearch->addItem(li);
    }
    dlg->deleteLater();
    saveSearchEngines();
}

void CSettingsTab::delSearchEngine()
{
    const QList<QListWidgetItem *> dl = ui->listSearch->selectedItems();
    for (QListWidgetItem* i : dl) {
        ui->listSearch->removeItemWidget(i);
        delete i;
    }
    saveSearchEngines();
}

void CSettingsTab::setDefaultSearch()
{
    const QList<QListWidgetItem *> dl = ui->listSearch->selectedItems();
    if (dl.isEmpty()) return;

    gSet->m_settings->defaultSearchEngine = dl.first()->data(Qt::UserRole).toString();

    updateSearchEngines();
}


// ******** User Script *******************************

void CSettingsTab::addUserScript()
{
    static QSize addScriptDlgSize;

    QFile f(QSL(":/data/userscript.js"));
    QString js;
    if (f.open(QIODevice::ReadOnly))
        js = QString::fromUtf8(f.readAll());

    QDialog dlg(this);
    Ui::UserScriptEditorDlg dui;
    dui.setupUi(&dlg);
    if (!addScriptDlgSize.isEmpty())
        dlg.resize(addScriptDlgSize);

    dui.editTitle->setText(tr("Script title"));
    dui.editSource->setPlainText(js);
    if (dlg.exec()==QDialog::Accepted) {
        auto *itm = new QListWidgetItem(dui.editTitle->text());
        itm->setData(Qt::UserRole,dui.editSource->toPlainText());
        ui->listUserScripts->addItem(itm);
    }
    addScriptDlgSize = dlg.size();
    saveUserScripts();
}

void CSettingsTab::editUserScript()
{
    if (ui->listUserScripts->selectedItems().isEmpty()) return;
    QListWidgetItem *itm = ui->listUserScripts->selectedItems().first();

    static QSize editScriptDlgSize;
    QDialog dlg(this);
    Ui::UserScriptEditorDlg dui;
    dui.setupUi(&dlg);
    if (!editScriptDlgSize.isEmpty())
        dlg.resize(editScriptDlgSize);

    dui.editTitle->setText(itm->text());
    dui.editSource->setPlainText(itm->data(Qt::UserRole).toString());
    if (dlg.exec()==QDialog::Accepted) {
        itm->setText(dui.editTitle->text());
        itm->setData(Qt::UserRole,dui.editSource->toPlainText());
    }
    editScriptDlgSize = dlg.size();
    saveUserScripts();
}

void CSettingsTab::deleteUserScript()
{
    const QList<QListWidgetItem *> dl = ui->listUserScripts->selectedItems();
    for (QListWidgetItem* i : dl) {
        ui->listUserScripts->removeItemWidget(i);
        delete i;
    }
    saveUserScripts();
}

void CSettingsTab::importUserScript()
{
    QString fname = CGenericFuncs::getOpenFileNameD(this,tr("Import user script from text file"),QDir::homePath(),
                                                    QStringList( { tr("JavaScript files (*.js)"),
                                                                   tr("All files (*)") } ));
    if (fname.isEmpty()) return;

    QFile f(fname);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this,QGuiApplication::applicationDisplayName(),
                             tr("Unable to open file"));
        return;
    }
    QTextStream fs(&f);
    QString src = fs.readAll();
    f.close();

    CUserScript us(QSL("loader"),src);

    auto *itm = new QListWidgetItem(us.getTitle());
    itm->setData(Qt::UserRole,src);
    ui->listUserScripts->addItem(itm);
    saveUserScripts();
}

void CSettingsTab::updateUserScripts()
{
    const auto &scripts = gSet->m_browser->getUserScripts();
    ui->listUserScripts->clear();

    for (auto it = scripts.constBegin(), end = scripts.constEnd(); it != end; ++it) {
        auto *itm = new QListWidgetItem(it.key());
        itm->setData(Qt::UserRole,it.value());
        ui->listUserScripts->addItem(itm);
    }
    saveUserScripts();
}

void CSettingsTab::saveUserScripts()
{
    CStringHash res;
    for (int i=0;i<ui->listUserScripts->count();i++) {
        QListWidgetItem *itm = ui->listUserScripts->item(i);
        res[itm->text()]=itm->data(Qt::UserRole).toString();
    }
    gSet->m_browser->initUserScripts(res);
}


// ****************** No Script **************************************


void CSettingsTab::saveNoScriptWhitelist()
{
    CStringSet res;
    for (int i=0;i<ui->listNoScriptWhitelist->count();i++) {
        QListWidgetItem *itm = ui->listNoScriptWhitelist->item(i);
        res.insert(itm->text());
    }
    gSet->d_func()->noScriptWhiteList = res;
}

void CSettingsTab::addNoScriptHost()
{
    bool ok = false;
    QString s = QInputDialog::getText(this,tr("Add to NoScript whitelist"),tr("Hostname"),
                                      QLineEdit::Normal,QString(),&ok);
    if (!ok) return;

    auto *li = new QListWidgetItem(s);
    ui->listNoScriptWhitelist->addItem(li);
    saveNoScriptWhitelist();
}

void CSettingsTab::delNoScriptHost()
{
    const QList<QListWidgetItem *> dl = ui->listNoScriptWhitelist->selectedItems();
    for (QListWidgetItem* i : dl) {
        ui->listNoScriptWhitelist->removeItemWidget(i);
        delete i;
    }
    saveNoScriptWhitelist();
}

void CSettingsTab::updateNoScriptWhitelist()
{
    ui->listNoScriptWhitelist->clear();
    const auto &hosts = gSet->d_func()->noScriptWhiteList;
    for (auto it = hosts.constBegin(), end = hosts.constEnd(); it != end; ++it) {
        auto *itm = new QListWidgetItem(*it);
        ui->listNoScriptWhitelist->addItem(itm);
    }
}


// ************** ATLAS SSL Certs ***********************

void CSettingsTab::cleanAtlCerts()
{
    gSet->m_settings->sslTrustedInvalidCerts.clear();
    updateAtlCertLabel();
}

void CSettingsTab::updateAtlCertLabel()
{
    if (gSet) {
        ui->atlCertsLabel->setText(QSL("Trusted:\n%1 certificates")
                                   .arg(gSet->m_settings->sslTrustedInvalidCerts.count()));
    }
}

// *******************************************
void CSettingsTab::saveSearchEngines()
{
    CStringHash engines;
    engines.clear();
    for (int i=0; i<ui->listSearch->count(); i++) {
        engines[ui->listSearch->item(i)->data(Qt::UserRole).toString()] =
                ui->listSearch->item(i)->data(Qt::UserRole+1).toString();
    }
    gSet->d_func()->ctxSearchEngines = engines;
}

void CSettingsTab::updateFontColorPreview()
{
    ui->frameFontColorOverride->setStyleSheet(QSL("QFrame { background: %1; }")
                                              .arg(gSet->m_settings->forcedFontColor.name(QColor::HexRgb)));
}


// ****************** Query history **********************

void CSettingsTab::delQrs()
{
    const QList<QListWidgetItem *> dl = ui->listQueries->selectedItems();
    for (QListWidgetItem* i : dl)
        gSet->d_func()->searchHistory.removeAll(i->text());

    updateQueryHistory();
    Q_EMIT gSet->m_history->updateAllQueryLists();
}

void CSettingsTab::updateQueryHistory()
{
    ui->listQueries->clear();
    for (const auto &i : qAsConst(gSet->d_func()->searchHistory))
        ui->listQueries->addItem(i);
}



// *******************************************************


CLangPairDelegate::CLangPairDelegate(QObject *parent) :
    QStyledItemDelegate (parent)
{

}

QWidget *CLangPairDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                                         const QModelIndex &index) const
{
    Q_UNUSED(option)
    Q_UNUSED(index)
    auto *editor = new QComboBox(parent);
    editor->setFrame(false);
    const QStringList languages = gSet->net()->getLanguageCodes();
    for (const auto &bcp : languages) {
        editor->addItem(gSet->net()->getLanguageName(bcp),QVariant::fromValue(bcp));
    }
    return editor;
}

void CLangPairDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    QString bcp = index.model()->data(index, Qt::EditRole).toString();

    auto *cb = qobject_cast<QComboBox *>(editor);

    int idx = cb->findData(QVariant::fromValue(bcp));
    if (idx>=0)
        cb->setCurrentIndex(idx);
}

void CLangPairDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    auto *cb = qobject_cast<QComboBox *>(editor);

    model->setData(index, cb->currentData().toString(), Qt::EditRole);
}

void CLangPairDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                                             const QModelIndex &index) const
{
    if (index.isValid())
        editor->setGeometry(option.rect);
}

CLangPairModel::CLangPairModel(QObject *parent, QTableView *table) :
    QAbstractTableModel (parent),
    m_table(table)
{
}

int CLangPairModel::rowCount(const QModelIndex & parent) const
{
    if (!checkIndex(parent))
        return 0;
    if (parent.isValid())
        return 0;

    return gSet->m_settings->translatorPairs.count();
}

int CLangPairModel::columnCount(const QModelIndex & parent) const
{
    if (!checkIndex(parent))
        return 0;
    if (parent.isValid())
        return 0;

    const int columnsCount = 2;
    return columnsCount;
}

QVariant CLangPairModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch (section) {
            case 0: return QVariant(QSL("Source language"));
            case 1: return QVariant(QSL("Destination language"));
            default: return QVariant();
        }
    }
    return QVariant();
}

QVariant CLangPairModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index,CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return QVariant();

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case 0: return QVariant(gSet->net()->getLanguageName(gSet->m_settings->translatorPairs
                                                                 .at(index.row()).langFrom.bcp47Name()));
            case 1: return QVariant(gSet->net()->getLanguageName(gSet->m_settings->translatorPairs
                                                                 .at(index.row()).langTo.bcp47Name()));
            default: return QVariant();
        }
    }
    if (role == Qt::EditRole) {
        switch (index.column()) {
            case 0: return QVariant(gSet->m_settings->translatorPairs.at(index.row()).langFrom.bcp47Name());
            case 1: return QVariant(gSet->m_settings->translatorPairs.at(index.row()).langTo.bcp47Name());
            default: return QVariant();
        }
    }
    return QVariant();
}

Qt::ItemFlags CLangPairModel::flags(const QModelIndex &index) const
{
    if (!checkIndex(index,CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return Qt::NoItemFlags;

    return (Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
}

bool CLangPairModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!checkIndex(index,CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return false;

    if (role == Qt::EditRole) {
        switch (index.column()) {
            case 0: gSet->m_settings->translatorPairs[index.row()].langFrom = QLocale(value.toString()); break;
            case 1: gSet->m_settings->translatorPairs[index.row()].langTo = QLocale(value.toString()); break;
        }
        gSet->m_actions->rebuildLanguageActions();
        Q_EMIT dataChanged(index,index);
        return true;
    }
    return false;
}

bool CLangPairModel::insertRows(int row, int count, const QModelIndex &parent)
{
    if (!checkIndex(parent))
        return false;
    if (parent.isValid())
        return false;

    beginInsertRows(parent, row, row+count-1);
    for (int i=0;i<count;i++)
        gSet->m_settings->translatorPairs.insert(row, CLangPair(QSL("ja"),QSL("en")));
    endInsertRows();
    gSet->m_actions->rebuildLanguageActions();
    return true;
}

bool CLangPairModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if (!checkIndex(parent))
        return false;
    if (parent.isValid())
        return false;

    beginRemoveRows(parent, row, row+count-1);
    for (int i=0;i<count;i++)
        gSet->m_settings->translatorPairs.removeAt(row);
    endRemoveRows();
    gSet->m_actions->rebuildLanguageActions();
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
