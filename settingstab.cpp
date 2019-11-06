#include <QInputDialog>
#include <QMessageBox>
#include <QColorDialog>
#include <QScrollBar>
#include <QWebEngineProfile>
#include <QWebEngineCookieStore>
#include <QWebEngineSettings>
#include <goldendictlib/goldendictmgr.hh>

#include "settingstab.h"
#include "ui_settingstab.h"
#include "mainwindow.h"
#include "globalcontrol.h"
#include "globalprivate.h"
#include "genericfuncs.h"
#include "multiinputdialog.h"
#include "userscript.h"
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

    connect(ui->editAdSearch, &QLineEdit::textChanged, this, &CSettingsTab::adblockSearch);

    ui->atlSSLProto->addItem(QSL("Secure"),static_cast<int>(QSsl::SecureProtocols));
    ui->atlSSLProto->addItem(QSL("TLS 1.2"),static_cast<int>(QSsl::TlsV1_2));
    ui->atlSSLProto->addItem(QSL("TLS 1.1"),static_cast<int>(QSsl::TlsV1_1));
    ui->atlSSLProto->addItem(QSL("TLS 1.0"),static_cast<int>(QSsl::TlsV1_0));
    ui->atlSSLProto->addItem(QSL("SSL V3"),static_cast<int>(QSsl::SslV3));
    ui->atlSSLProto->addItem(QSL("SSL V2"),static_cast<int>(QSsl::SslV2));
    ui->atlSSLProto->addItem(QSL("Any"),static_cast<int>(QSsl::AnyProtocol));
    updateAtlCertLabel();

    setupSettingsObservers();

    ui->scrollArea->verticalScrollBar()->setValue(ui->scrollArea->verticalScrollBar()->minimum());
}

CSettingsTab::~CSettingsTab()
{
    delete ui;
}

CSettingsTab *CSettingsTab::instance()
{
    static CSettingsTab* inst = nullptr;
    static QMutex lock;
    QMutexLocker locker(&lock);

    if (gSet==nullptr || gSet->activeWindow()==nullptr)
        return nullptr;

    CMainWindow* wnd = gSet->activeWindow();
    if (inst==nullptr) {
        inst = new CSettingsTab(wnd);
        inst->bindToTab(wnd->tabMain);

        connect(inst,&CSettingsTab::destroyed,gSet,[](){
            inst = nullptr;
        });

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
    ui->checkJS->setChecked(gSet->webProfile()->settings()->
                            testAttribute(QWebEngineSettings::JavascriptEnabled));
    ui->checkAutoloadImages->setChecked(gSet->webProfile()->settings()->
                                        testAttribute(QWebEngineSettings::AutoLoadImages));
    ui->checkEnablePlugins->setChecked(gSet->webProfile()->settings()->
                                       testAttribute(QWebEngineSettings::PluginsEnabled));

    switch (gSet->m_settings->translatorEngine) {
        case CStructures::teAtlas: ui->radioAtlas->setChecked(true); break;
        case CStructures::teBingAPI: ui->radioBingAPI->setChecked(true); break;
        case CStructures::teYandexAPI: ui->radioYandexAPI->setChecked(true); break;
        case CStructures::teGoogleGTX: ui->radioGoogleGTX->setChecked(true); break;
        case CStructures::teAmazonAWS: ui->radioAmazonAWS->setChecked(true); break;
    }

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
    ui->editAWSRegion->setText(gSet->m_settings->awsRegion);
    ui->editAWSAccessKey->setText(gSet->m_settings->awsAccessKey);
    ui->editAWSSecretKey->setText(gSet->m_settings->awsSecretKey);
    ui->checkEmptyRestore->setChecked(gSet->m_settings->emptyRestore);
    ui->checkJSLogConsole->setChecked(gSet->m_settings->jsLogConsole);

    ui->checkUseAd->setChecked(gSet->m_settings->useAdblock);
    ui->checkUseNoScript->setChecked(gSet->m_settings->useNoScript);

    ui->checkTransFont->setChecked(gSet->m_ui->useOverrideTransFont());
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
    ui->transFont->setEnabled(gSet->m_ui->useOverrideTransFont());
    ui->transFontSize->setEnabled(gSet->m_ui->useOverrideTransFont());
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
    ui->checkFontColorOverride->setChecked(gSet->m_ui->forceFontColor());

    ui->gctxHotkey->setKeySequence(gSet->m_ui->gctxTranHotkey.shortcut());
    ui->checkCreateCoredumps->setChecked(gSet->m_settings->createCoredumps);
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

    ui->checkDontUseNativeFileDialogs->setChecked(gSet->m_settings->dontUseNativeFileDialog);
    ui->spinCacheSize->setValue(gSet->webProfile()->httpCacheMaximumSize()/CDefaults::oneMB);
    ui->checkIgnoreSSLErrors->setChecked(gSet->m_settings->ignoreSSLErrors);
    ui->checkTabFavicon->setChecked(gSet->webProfile()->settings()->
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

    m_loadingInterlock = false;
}

void CSettingsTab::resizeEvent(QResizeEvent *event)
{
    const int transDirectionsColumnWidthFrac = 30;

    ui->listTransDirections->setColumnWidth(0, transDirectionsColumnWidthFrac*event->size().width()/100);
    ui->listTransDirections->setColumnWidth(1, transDirectionsColumnWidthFrac*event->size().width()/100);

    QWidget::resizeEvent(event);
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
        gSet->webProfile()->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled,val);
        gSet->m_ui->actionJSUsage->setChecked(val);
    });
    connect(ui->checkAutoloadImages,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->webProfile()->settings()->setAttribute(QWebEngineSettings::AutoLoadImages,val);
    });
    connect(ui->checkEnablePlugins,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->webProfile()->settings()->setAttribute(QWebEngineSettings::PluginsEnabled,val);
    });

    connect(ui->radioAtlas,&QRadioButton::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        if (val)
            gSet->setTranslationEngine(CStructures::teAtlas);
    });
    connect(ui->radioBingAPI,&QRadioButton::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        if (val)
            gSet->setTranslationEngine(CStructures::teBingAPI);
    });
    connect(ui->radioYandexAPI,&QRadioButton::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        if (val)
            gSet->setTranslationEngine(CStructures::teYandexAPI);
    });
    connect(ui->radioGoogleGTX,&QRadioButton::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        if (val)
            gSet->setTranslationEngine(CStructures::teGoogleGTX);
    });
    connect(ui->radioAmazonAWS,&QRadioButton::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        if (val)
            gSet->setTranslationEngine(CStructures::teAmazonAWS);
    });

    connect(ui->atlHost->lineEdit(),&QLineEdit::textChanged,this,[this](const QString& val){
        if (m_loadingInterlock) return;
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
    connect(ui->atlRetryCnt,qOverload<int>(&QSpinBox::valueChanged),this,[this](int val){
        if (m_loadingInterlock) return;
        gSet->m_settings->atlTcpRetryCount=val;
    });
    connect(ui->atlRetryTimeout,qOverload<int>(&QSpinBox::valueChanged),this,[this](int val){
        if (m_loadingInterlock) return;
        gSet->m_settings->atlTcpTimeout=val;
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
    connect(ui->checkEmptyRestore,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->m_settings->emptyRestore=val;
    });
    connect(ui->checkJSLogConsole,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->m_settings->jsLogConsole=val;
    });

    connect(ui->checkTransFont,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->m_ui->actionOverrideTransFont->setChecked(val);
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
        gSet->m_ui->actionOverrideTransFontColor->setChecked(val);
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
        gSet->m_ui->gctxTranHotkey.setShortcut(val);
        if (!gSet->m_ui->gctxTranHotkey.shortcut().isEmpty())
            gSet->m_ui->gctxTranHotkey.setEnabled();
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
        gSet->m_ui->actionLogNetRequests->setChecked(val);
    });
    connect(ui->checkDumpHtml,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->m_settings->debugDumpHtml = val;
    });

    connect(ui->checkUserAgent,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->m_settings->overrideUserAgent = val;
        if (gSet->m_settings->overrideUserAgent && !gSet->m_settings->userAgent.isEmpty()) {
            gSet->webProfile()->setHttpUserAgent(gSet->m_settings->userAgent);
        } else {
            gSet->webProfile()->setHttpUserAgent(QWebEngineProfile::defaultProfile()->httpUserAgent());
        }
    });
    connect(ui->editUserAgent->lineEdit(),&QLineEdit::textChanged,this,[this](const QString& val){
        if (m_loadingInterlock) return;
        gSet->m_settings->userAgent = val;
        if (gSet->m_settings->overrideUserAgent && !gSet->m_settings->userAgent.isEmpty()) {
            gSet->webProfile()->setHttpUserAgent(gSet->m_settings->userAgent);
        } else {
            gSet->webProfile()->setHttpUserAgent(QWebEngineProfile::defaultProfile()->httpUserAgent());
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
        gSet->webProfile()->setHttpCacheMaximumSize(val*CDefaults::oneMB);
    });
    connect(ui->checkIgnoreSSLErrors,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->m_settings->ignoreSSLErrors = val;
    });
    connect(ui->checkTabFavicon,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->webProfile()->settings()->setAttribute(QWebEngineSettings::AutoLoadIconsForPage,val);
    });


    connect(ui->editProxyHost,&QLineEdit::textChanged,this,[this](const QString& val){
        if (m_loadingInterlock) return;
        gSet->m_settings->proxyHost = val;
        gSet->updateProxyWithMenuUpdate(gSet->m_settings->proxyUse,true);
    });
    connect(ui->spinProxyPort,qOverload<int>(&QSpinBox::valueChanged),this,[this](int val){
        if (m_loadingInterlock) return;
        gSet->m_settings->proxyPort = static_cast<quint16>(val);
        gSet->updateProxyWithMenuUpdate(gSet->m_settings->proxyUse,true);
    });
    connect(ui->editProxyLogin,&QLineEdit::textChanged,this,[this](const QString& val){
        if (m_loadingInterlock) return;
        gSet->m_settings->proxyLogin = val;
        gSet->updateProxyWithMenuUpdate(gSet->m_settings->proxyUse,true);
    });
    connect(ui->editProxyPassword,&QLineEdit::textChanged,this,[this](const QString& val){
        if (m_loadingInterlock) return;
        gSet->m_settings->proxyPassword = val;
        gSet->updateProxyWithMenuUpdate(gSet->m_settings->proxyUse,true);
    });
    connect(ui->checkUseProxy,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->m_settings->proxyUse = val;
        gSet->updateProxyWithMenuUpdate(gSet->m_settings->proxyUse,true);
    });
    connect(ui->checkUseProxyTranslator,&QCheckBox::toggled,this,[this](bool val){
        if (m_loadingInterlock) return;
        gSet->m_settings->proxyUseTranslator = val;
        gSet->updateProxyWithMenuUpdate(gSet->m_settings->proxyUse,true);
    });
    connect(ui->listProxyType,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int val){
        if (m_loadingInterlock) return;
        switch (val) {
            case 0: gSet->m_settings->proxyType = QNetworkProxy::HttpCachingProxy; break;
            case 1: gSet->m_settings->proxyType = QNetworkProxy::HttpProxy; break;
            case 2: gSet->m_settings->proxyType = QNetworkProxy::Socks5Proxy; break;
            default: gSet->m_settings->proxyType = QNetworkProxy::HttpCachingProxy; break;
        }
        gSet->updateProxyWithMenuUpdate(gSet->m_settings->proxyUse,true);
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
    QString s = CGenericFuncs::getExistingDirectoryD(this,tr("Select directory"));
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
    QStringList res;
    res.reserve(ui->listDictPaths->count());
    for (int i=0;i<ui->listDictPaths->count();i++)
        res.append(ui->listDictPaths->item(i)->text());

    if (CGenericFuncs::compareStringLists(gSet->m_settings->dictPaths,res)!=0) {
        gSet->m_settings->dictPaths = res;
        gSet->d_func()->dictManager->loadDictionaries(
                    gSet->m_settings->dictPaths,
                    gSet->m_settings->dictIndexDir);
    }
}

void CSettingsTab::showLoadedDicts()
{
    QString msg = tr("No dictionaries loaded.");

    const QStringList loadedDicts = gSet->d_func()->dictManager->getLoadedDictionaries();
    if (!loadedDicts.isEmpty())
        msg = tr("Loaded %1 dictionaries:\n").arg(loadedDicts.count())+loadedDicts.join('\n');
    QMessageBox::information(this,tr("JPReader"),msg);
}


// ************************ Browsing history ***********************

void CSettingsTab::clearHistory()
{
    if (gSet) {
        gSet->d_func()->mainHistory.clear();
        Q_EMIT gSet->updateAllHistoryLists();
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
        auto li = new QListWidgetItem(QSL("%1 [ %2 ]")
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

    auto cj = qobject_cast<CNetworkCookieJar *>(gSet->d_func()->auxNetManager->cookieJar());
    if (cj==nullptr) return;
    auto cookiesList = cj->getAllCookies();

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

void CSettingsTab::clearCookies()
{
    QWebEngineCookieStore* wsc = gSet->webProfile()->cookieStore();
    if (wsc)
        wsc->deleteAllCookies();

    updateCookiesTable();
}

void CSettingsTab::delCookies()
{
    auto cj = qobject_cast<CNetworkCookieJar *>(gSet->d_func()->auxNetManager->cookieJar());
    if (cj==nullptr) return;
    auto cookiesList = cj->getAllCookies();

    QList<int> r = getSelectedRows(ui->tableCookies);
    if (gSet->webProfile()->cookieStore()) {
        for (int idx : qAsConst(r))
            gSet->webProfile()->cookieStore()->deleteCookie(cookiesList.at(idx));

        updateCookiesTable();
    }
}

void CSettingsTab::exportCookies()
{
    auto cj = qobject_cast<CNetworkCookieJar *>(gSet->d_func()->auxNetManager->cookieJar());
    if (cj==nullptr) return;
    auto cookiesList = cj->getAllCookies();

    QList<int> r = getSelectedRows(ui->tableCookies);
    if (r.isEmpty()) {
        QMessageBox::information(this,tr("JPReader"),tr("Please select cookies for export."));
        return;
    }

    QString fname = CGenericFuncs::getSaveFileNameD(this,tr("Save cookies to file"),gSet->settings()->savedAuxSaveDir,
                                                    QStringList( { tr("Text file, Netscape format (*.txt)") } ) );

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
           << CGenericFuncs::bool2str2(cookiesList.at(idx).domain().startsWith('.'))
           << '\t'
           << cookiesList.at(idx).path()
           << '\t'
           << CGenericFuncs::bool2str2(cookiesList.at(idx).isSecure())
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


// *********************** AdBlock ************************************

void CSettingsTab::clearAdblockWhitelist()
{
    gSet->d_func()->adblockWhiteListMutex.lock();
    gSet->d_func()->adblockWhiteList.clear();
    gSet->d_func()->adblockWhiteListMutex.unlock();
}

void CSettingsTab::updateAdblockList()
{
    const auto &adblockList = gSet->d_func()->adblock;

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
    bool ok;
    QString s = QInputDialog::getText(this,tr("Add AdBlock rule"),tr("Filter template"),
                                      QLineEdit::Normal,QString(),&ok);
    if (!ok) return;

    gSet->d_func()->adblock << CAdBlockRule(s,QString());
    updateAdblockList();
    clearAdblockWhitelist();
}

void CSettingsTab::delAd()
{
    CAdBlockVector r;
    const QList<QTreeWidgetItem *> il = ui->treeAdblock->selectedItems();
    r.reserve(il.count());
    for (const QTreeWidgetItem* i : il)
        r << gSet->d_func()->adblock.at(i->data(0,Qt::UserRole+1).toInt());

    for (const auto& rule : qAsConst(r))
        gSet->d_func()->adblock.removeOne(rule);

    updateAdblockList();
    clearAdblockWhitelist();
}

void CSettingsTab::delAdAll()
{
    gSet->d_func()->adblock.clear();
    updateAdblockList();
    clearAdblockWhitelist();
}

void CSettingsTab::importAd()
{
    QString fname = CGenericFuncs::getOpenFileNameD(this,tr("Import rules from text file"),QDir::homePath());
    if (fname.isEmpty()) return;

    QFile f(fname);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this,tr("JPReader"),tr("Unable to open file"));
        return;
    }
    QTextStream fs(&f);

    QFileInfo fi(fname);

    int psz = gSet->d_func()->adblock.count();
    int cssRule = 0;
    while (!fs.atEnd()) {
        CAdBlockRule rule = CAdBlockRule(fs.readLine(),fi.fileName());
        // skip CSS rules for speedup search, anyway we do not support CSS rules now.
        if (!rule.isCSSRule()) {
            gSet->d_func()->adblock << rule;
        } else {
            cssRule++;
        }
    }
    f.close();

    updateAdblockList();
    clearAdblockWhitelist();

    psz = gSet->d_func()->adblock.count() - psz;
    QMessageBox::information(this,tr("JPReader"),tr("%1 rules imported, %2 CSS rules dropped.").arg(psz).arg(cssRule));
}

void CSettingsTab::exportAd()
{
    if (ui->treeAdblock->selectedItems().isEmpty()) {
        QMessageBox::information(this,tr("JPReader"),tr("Please select patterns for export."));
        return;
    }

    CAdBlockVector r;
    const QList<QTreeWidgetItem *> il = ui->treeAdblock->selectedItems();
    r.reserve(il.count());
    for (const QTreeWidgetItem* i : il)
        r << gSet->d_func()->adblock.at(i->data(0,Qt::UserRole+1).toInt());

    QString fname = CGenericFuncs::getSaveFileNameD(this,tr("Save AdBlock patterns to file"),
                                                    gSet->settings()->savedAuxSaveDir,
                                                    QStringList( { tr("Text file (*.txt)") } ) );

    if (fname.isEmpty() || fname.isNull()) return;
    gSet->setSavedAuxSaveDir(QFileInfo(fname).absolutePath());

    QFile f(fname);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this,tr("JPReader"),tr("Unable to create file"));
        return;
    }
    QTextStream fs(&f);

    for (const auto& rule : qAsConst(r))
        fs << rule.filter() << endl;

    fs.flush();
    f.close();
}


// ******************* Context Search Engine ******************

void CSettingsTab::updateSearchEngines()
{
    ui->listSearch->clear();
    const auto &engines = gSet->d_func()->ctxSearchEngines;
    for (auto it = engines.constBegin(), end = engines.constEnd(); it != end; ++it) {
        auto li = new QListWidgetItem(QSL("%1 [ %2 ] %3").
                                                  arg(it.key(),
                                                      it.value(),
                                                      it.key()==gSet->m_settings->defaultSearchEngine ?
                                                          tr("(default)") : QString()));
        li->setData(Qt::UserRole,it.key());
        li->setData(Qt::UserRole+1,it.value());
        ui->listSearch->addItem(li);
    }
}

void CSettingsTab::addSearchEngine()
{
    CStringHash data;
    data[QSL("Url template")]=QString();
    data[QSL("Menu title")]=QString();

    static const QString hlp = tr("In the url template you can use following substitutions\n"
                                  "  %s - search text\n"
                                  "  %ps - percent-encoded search text");

    auto dlg = new CMultiInputDialog(this,tr("Add new search engine"),data,hlp);
    if (dlg->exec()==QDialog::Accepted) {
        data = dlg->getInputData();

        auto li = new QListWidgetItem(QSL("%1 [ %2 ] %3").
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
    QList<QListWidgetItem *> dl = ui->listSearch->selectedItems();
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
    auto dlg = new QDialog(this);
    Ui::UserScriptEditorDlg dui;
    dui.setupUi(dlg);
    if (!addScriptDlgSize.isEmpty())
        dlg->resize(addScriptDlgSize);

    if (dlg->exec()==QDialog::Accepted) {
        auto itm = new QListWidgetItem(dui.editTitle->text());
        itm->setData(Qt::UserRole,dui.editSource->toPlainText());
        ui->listUserScripts->addItem(itm);
    }
    addScriptDlgSize = dlg->size();
    dlg->setParent(nullptr);
    dlg->deleteLater();
    saveUserScripts();
}

void CSettingsTab::editUserScript()
{
    if (ui->listUserScripts->selectedItems().isEmpty()) return;
    QListWidgetItem *itm = ui->listUserScripts->selectedItems().first();

    static QSize editScriptDlgSize;
    auto dlg = new QDialog(this);
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
    saveUserScripts();
}

void CSettingsTab::deleteUserScript()
{
    QList<QListWidgetItem *> dl = ui->listUserScripts->selectedItems();
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
        QMessageBox::warning(this,tr("JPReader"),tr("Unable to open file"));
        return;
    }
    QTextStream fs(&f);
    QString src = fs.readAll();
    f.close();

    CUserScript us(QSL("loader"),src);

    auto itm = new QListWidgetItem(us.getTitle());
    itm->setData(Qt::UserRole,src);
    ui->listUserScripts->addItem(itm);
    saveUserScripts();
}

void CSettingsTab::updateUserScripts()
{
    const auto &scripts = gSet->getUserScripts();
    ui->listUserScripts->clear();

    for (auto it = scripts.constBegin(), end = scripts.constEnd(); it != end; ++it) {
        auto itm = new QListWidgetItem(it.key());
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
    gSet->initUserScripts(res);
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
    bool ok;
    QString s = QInputDialog::getText(this,tr("Add to NoScript whitelist"),tr("Hostname"),
                                      QLineEdit::Normal,QString(),&ok);
    if (!ok) return;

    auto li = new QListWidgetItem(s);
    ui->listNoScriptWhitelist->addItem(li);
    saveNoScriptWhitelist();
}

void CSettingsTab::delNoScriptHost()
{
    QList<QListWidgetItem *> dl = ui->listNoScriptWhitelist->selectedItems();
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
        auto itm = new QListWidgetItem(*it);
        ui->listNoScriptWhitelist->addItem(itm);
    }
}


// ************** ATLAS SSL Certs ***********************

void CSettingsTab::cleanAtlCerts()
{
    gSet->m_settings->atlCerts.clear();
    updateAtlCertLabel();
}

void CSettingsTab::updateAtlCertLabel()
{
    if (gSet) {
        ui->atlCertsLabel->setText(QSL("Trusted:\n%1 certificates")
                                   .arg(gSet->m_settings->atlCerts.count()));
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
    QList<QListWidgetItem *> dl = ui->listQueries->selectedItems();
    for (QListWidgetItem* i : dl)
        gSet->d_func()->searchHistory.removeAll(i->text());

    updateQueryHistory();
    Q_EMIT gSet->updateAllQueryLists();
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
    if (index.isValid())
        editor->setGeometry(option.rect);
}

CLangPairModel::CLangPairModel(QObject *parent, QTableView *table) :
    QAbstractTableModel (parent)
{
    m_table = table;
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
            case 0: return QVariant(gSet->getLanguageName(gSet->m_settings->translatorPairs
                                                          .at(index.row()).langFrom.bcp47Name()));
            case 1: return QVariant(gSet->getLanguageName(gSet->m_settings->translatorPairs
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
        gSet->m_ui->rebuildLanguageActions();
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
    gSet->m_ui->rebuildLanguageActions();
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
    gSet->m_ui->rebuildLanguageActions();
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
