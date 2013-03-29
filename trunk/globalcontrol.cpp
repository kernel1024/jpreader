#include <QNetworkDiskCache>

#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
#include <QStandardPaths>
#else
#include <QDesktopServices>
#endif

#include "globalcontrol.h"
#include "calcthread.h"
#include "authdlg.h"
#include "lighttranslator.h"
#include "auxtranslator.h"
#include "genericfuncs.h"
#include "auxtranslator_adaptor.h"
#include "qxttooltip.h"


CGlobalControl::CGlobalControl(QtSingleApplication *parent) :
    QObject(parent)
{
    snippetColors << QColor(Qt::red) << QColor(Qt::green) << QColor(Qt::blue) << QColor(Qt::cyan) <<
                     QColor(Qt::magenta) << QColor(Qt::darkRed) << QColor(Qt::darkGreen) <<
                     QColor(Qt::darkBlue) << QColor(Qt::darkCyan) << QColor(Qt::darkMagenta) <<
                     QColor(Qt::darkYellow) << QColor(Qt::gray);

    maxTranThreads=10;
    maxLimit=1000;
    useAdblock=false;
    globalContextTranslate=false;
    blockTabCloseActive=false;
    adblock.clear();
    cleaningState=false;
    forcedCharset=""; // autodetect
    createdFiles.clear();
    recycleBin.clear();
    mainHistory.clear();
    searchHistory.clear();
    scpHostHistory.clear();
    atlHostHistory.clear();

    appIcon.addFile(":/globe16");
    appIcon.addFile(":/globe32");
    appIcon.addFile(":/globe48");
    appIcon.addFile(":/globe128");

    useOverrideFont=false;
    overrideStdFonts=false;
    overrideFont=QApplication::font();
    fontStandard=QApplication::font().family();
    fontFixed="Courier New";
    fontSerif="Times New Roman";
    fontSansSerif="Verdana";

    lastClipboardContents = "";
    lastClipboardContentsUnformatted = "";
    lastClipboardIsHtml = false;

    netAccess.setCookieJar(&cookieJar);

    QString fs = QString();
#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
    fs = QDesktopServices::storageLocation(QDesktopServices::CacheLocation);
#else
    fs = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
#endif

    if (fs.isEmpty()) fs = QDir::homePath() + QDir::separator() + tr(".config");
    if (!fs.endsWith(QDir::separator())) fs += QDir::separator();
    fs += tr("jpreader_cache") + QDir::separator();
    QNetworkDiskCache* cache = new QNetworkDiskCache(this);
    cache->setCacheDirectory(fs);
    netAccess.setCache(cache);

    dlg = NULL;
    activeWindow = NULL;
    lightTranslator = NULL;
    autoTranslate = false;

    actionGlobalTranslator = new QAction(tr("Global context translator"),this);
    actionGlobalTranslator->setCheckable(true);
    actionGlobalTranslator->setChecked(false);

    actionSelectionDictionary = new QAction(tr("Dictionary search"),this);
    actionSelectionDictionary->setCheckable(true);
    actionSelectionDictionary->setChecked(false);


    auxTranslatorDBus = new CAuxTranslator(this);
    new AuxtranslatorAdaptor(auxTranslatorDBus);
    QDBusConnection dbus = QDBusConnection::sessionBus();
    dbus.registerObject("/",auxTranslatorDBus);
    dbus.registerService("org.jpreader.auxtranslator");

    connect(actionGlobalTranslator,SIGNAL(triggered()),this,SLOT(updateTrayIconState()));
    connect(actionGlobalTranslator,SIGNAL(toggled(bool)),this,SLOT(updateTrayIconState(bool)));

    QMenu* tiMenu = new QMenu();
    tiMenu->addAction(QIcon::fromTheme("window-new"),tr("Create new window"),this,SLOT(addMainWindow()));
    tiMenu->addAction(actionGlobalTranslator);
    tiMenu->addSeparator();
    tiMenu->addAction(QIcon::fromTheme("application-exit"),tr("Exit"),this,SLOT(cleanupAndExit()));
    trayIcon.setIcon(appIcon);
    trayIcon.setContextMenu(tiMenu);
    trayIcon.show();
    connect(&trayIcon,SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this,SLOT(trayClicked(QSystemTrayIcon::ActivationReason)));

    connect(parent,SIGNAL(focusChanged(QWidget*,QWidget*)),this,SLOT(focusChanged(QWidget*,QWidget*)));
    connect(parent,SIGNAL(aboutToQuit()),this,SLOT(preShutdown()));
    connect(&netAccess,SIGNAL(authenticationRequired(QNetworkReply*,QAuthenticator*)),
            this,SLOT(authentication(QNetworkReply*,QAuthenticator*)));
    connect(QApplication::clipboard(),SIGNAL(changed(QClipboard::Mode)),
            this,SLOT(clipboardChanged(QClipboard::Mode)));
    connect(parent,SIGNAL(messageReceived(QString)),
            this,SLOT(ipcMessageReceived(QString)));

    gctxTranHotkey = new QxtGlobalShortcut(this);
    gctxTranHotkey->setDisabled();
    connect(gctxTranHotkey,SIGNAL(activated()),actionGlobalTranslator,SLOT(toggle()));

    readSettings();

    settingsSaveTimer.setInterval(60000);
    connect(&settingsSaveTimer,SIGNAL(timeout()),this,SLOT(writeSettings()));
    settingsSaveTimer.start();
}

void CGlobalControl::writeSettings()
{
    if (!settingsSaveMutex.tryLock(1000)) return;
    QSettings settings("kernel1024", "jpreader");
    settings.beginGroup("MainWindow");
    settings.setValue("searchCnt", searchHistory.count());
    for (int i=0;i<searchHistory.count();i++)
        settings.setValue(QString("searchTxt%1").arg(i), searchHistory.at(i));
    settings.setValue("hostingDir",hostingDir);
    settings.setValue("hostingUrl",hostingUrl);
    settings.setValue("maxLimit",maxLimit);
    settings.setValue("browser",sysBrowser);
    settings.setValue("editor",sysEditor);
    settings.setValue("tr_engine",translatorEngine);
    settings.setValue("scp",useScp);
    settings.setValue("scphost",scpHost);
    settings.setValue("scpparams",scpParams);
    settings.setValue("atlasHost",atlHost);
    settings.setValue("atlasPort",atlPort);
    settings.setValue("auxDir",savedAuxDir);
    settings.setValue("javascript",QWebSettings::globalSettings()->
                      testAttribute(QWebSettings::JavascriptEnabled));
    settings.setValue("autoloadimages",QWebSettings::globalSettings()->
                      testAttribute(QWebSettings::AutoLoadImages));
    settings.setValue("cookies",cookieJar.saveCookies());
    settings.setValue("recycledCount",maxRecycled);
    settings.beginWriteArray("bookmarks");
    int i=0;
    foreach (const QString &t, bookmarks.keys()) {
        settings.setArrayIndex(i);
        settings.setValue("title",t);
        settings.setValue("url",bookmarks.value(t));
        i++;
    }
    settings.endArray();

    QByteArray ba;
    QDataStream hss(&ba,QIODevice::WriteOnly);
    hss << mainHistory;
    settings.setValue("history",ba);

    settings.setValue("adblock_count",adblock.count());
    for (int i=0;i<adblock.count();i++)
        settings.setValue(QString("adblock%1").arg(i),adblock.at(i));

    settings.setValue("useAdblock",useAdblock);
    settings.setValue("useOverrideFont",useOverrideFont);
    settings.setValue("overrideFont",overrideFont.family());
    settings.setValue("overrideFontSize",overrideFont.pointSize());
    settings.setValue("overrideStdFonts",overrideStdFonts);
    settings.setValue("standardFont",fontStandard);
    settings.setValue("fixedFont",fontFixed);
    settings.setValue("serifFont",fontSerif);
    settings.setValue("sansSerifFont",fontSansSerif);
    settings.setValue("forceFontColor",forceFontColor);
    settings.setValue("forcedFontColor",forcedFontColor.name());
    settings.setValue("gctxHotkey",gctxTranHotkey->shortcut().toString());
    settings.setValue("atlHost_count",atlHostHistory.count());
    for (int i=0;i<atlHostHistory.count();i++)
        settings.setValue(QString("atlHost%1").arg(i),atlHostHistory.at(i));
    settings.setValue("scpHost_count",scpHostHistory.count());
    for (int i=0;i<scpHostHistory.count();i++)
        settings.setValue(QString("scpHost%1").arg(i),scpHostHistory.at(i));

    settings.endGroup();
    settingsSaveMutex.unlock();
}

void CGlobalControl::readSettings()
{
    QSettings settings("kernel1024", "jpreader");
    settings.beginGroup("MainWindow");
    int cnt = settings.value("searchCnt",0).toInt();
    QStringList qs;
    for (int i=0;i<cnt;i++) {
        QString s=settings.value(QString("searchTxt%1").arg(i),"").toString();
        if (!s.isEmpty()) qs << s;
    }
    searchHistory.clear();
    searchHistory.append(qs);
    updateAllQueryLists();

    hostingDir = settings.value("hostingDir","").toString();
    hostingUrl = settings.value("hostingUrl","about:blank").toString();
    maxLimit = settings.value("maxLimit",1000).toInt();
    sysBrowser = settings.value("browser","konqueror").toString();
    sysEditor = settings.value("editor","kwrite").toString();
    translatorEngine = settings.value("tr_engine",TE_NIFTY).toInt();
    useScp = settings.value("scp",false).toBool();
    scpHost = settings.value("scphost","").toString();
    scpParams = settings.value("scpparams","").toString();
    atlHost = settings.value("atlasHost","localhost").toString();
    atlPort = settings.value("atlasPort",18000).toInt();
    cnt = settings.value("atlHost_count",0).toInt();
    qs.clear();
    atlHostHistory.clear();
    for (int i=0;i<cnt;i++) {
        QString s=settings.value(QString("atlHost%1").arg(i),"").toString();
        if (!s.isEmpty()) qs << s;
    }
    atlHostHistory.append(qs);
    cnt = settings.value("scpHost_count",0).toInt();
    qs.clear();
    scpHostHistory.clear();
    for (int i=0;i<cnt;i++) {
        QString s=settings.value(QString("scpHost%1").arg(i),"").toString();
        if (!s.isEmpty()) qs << s;
    }
    scpHostHistory.append(qs);
    savedAuxDir = settings.value("auxDir",QDir::homePath()).toString();
    maxRecycled = settings.value("recycledCount",20).toInt();
    QWebSettings::globalSettings()->setAttribute(QWebSettings::JavascriptEnabled,
                                                 settings.value("javascript",true).toBool());
    QWebSettings::globalSettings()->setAttribute(QWebSettings::AutoLoadImages,
                                                 settings.value("autoloadimages",true).toBool());
    QByteArray ck = settings.value("cookies",QByteArray()).toByteArray();
    cookieJar.loadCookies(&ck);
    int sz = settings.beginReadArray("bookmarks");
    for (int i=0; i<sz; i++) {
        settings.setArrayIndex(i);
        QString t = settings.value("title").toString();
        if (!t.isEmpty())
            bookmarks[t]=settings.value("url").toUrl();
    }
    settings.endArray();

    QByteArray ba = settings.value("history",QByteArray()).toByteArray();
    if (!ba.isEmpty()) {
        mainHistory.clear();
        QDataStream hss(&ba,QIODevice::ReadOnly);
        try {
            hss >> mainHistory;
        } catch (...) {
            mainHistory.clear();
        }
    }
    cnt = settings.value("adblock_count",0).toInt();
    adblock.clear();
    for (int i=0;i<cnt;i++) {
        QString at = settings.value(QString("adblock%1").arg(i),"").toString();
        if (!at.isEmpty()) adblock << at;
    }

    useAdblock=settings.value("useAdblock",false).toBool();
    useOverrideFont=settings.value("useOverrideFont",false).toBool();
    overrideFont.setFamily(settings.value("overrideFont","Verdana").toString());
    overrideFont.setPointSize(settings.value("overrideFontSize",12).toInt());
    overrideStdFonts=settings.value("overrideStdFonts",false).toBool();
    fontStandard=settings.value("standardFont",QApplication::font().family()).toString();
    fontFixed=settings.value("fixedFont","Courier New").toString();
    fontSerif=settings.value("serifFont","Times New Roman").toString();
    fontSansSerif=settings.value("sansSerifFont","Verdana").toString();
    forceFontColor=settings.value("forceFontColor",false).toBool();
    forcedFontColor=QColor(settings.value("forcedFontColor","#000000").toString());
    QString hk = settings.value("gctxHotkey",QString()).toString();
    if (!hk.isEmpty()) {
        gctxTranHotkey->setShortcut(QKeySequence::fromString(hk));
        if (!gctxTranHotkey->shortcut().isEmpty())
            gctxTranHotkey->setEnabled();
    }

    settings.endGroup();
    if (hostingDir.right(1)!="/") hostingDir=hostingDir+"/";
    if (hostingUrl.right(1)!="/") hostingUrl=hostingUrl+"/";
    updateAllBookmarks();
}

void CGlobalControl::settingsDlg()
{
    if (dlg!=NULL) {
        dlg->activateWindow();
        return;
    }
    dlg = new CSettingsDlg();
    dlg->hostingDir->setText(hostingDir);
    dlg->hostingUrl->setText(hostingUrl);
    dlg->maxLimit->setValue(maxLimit);
    dlg->editor->setText(sysEditor);
    dlg->browser->setText(sysBrowser);
    dlg->maxRecycled->setValue(maxRecycled);
    dlg->useJS->setChecked(QWebSettings::globalSettings()->
                           testAttribute(QWebSettings::JavascriptEnabled));
    dlg->autoloadImages->setChecked(QWebSettings::globalSettings()->
                                    testAttribute(QWebSettings::AutoLoadImages));
    dlg->qrList->clear();
    for (int i=0;i<searchHistory.count();i++)
        dlg->qrList->addItem(searchHistory.at(i));
    foreach (const QString &t, bookmarks.keys()) {
        QListWidgetItem* li = new QListWidgetItem(QString("%1 [ %2 ]").
                                                  arg(t).
                                                  arg(bookmarks.value(t).toString()));
        li->setData(Qt::UserRole,t);
        li->setData(Qt::UserRole+1,bookmarks.value(t));
        dlg->bmList->addItem(li);
    }
    foreach (const UrlHolder &t, mainHistory) {
        QListWidgetItem* li = new QListWidgetItem(QString("%1 [ %2 ]").arg(t.title).
                                                  arg(t.url.toString()));
        li->setData(Qt::UserRole,t.uuid.toString());
        dlg->hsList->addItem(li);
    }
    switch (translatorEngine) {
    case TE_NIFTY: dlg->rbNifty->setChecked(true); break;
    case TE_GOOGLE: dlg->rbGoogle->setChecked(true); break;
    case TE_ATLAS: dlg->rbAtlas->setChecked(true); break;
    default: dlg->rbNifty->setChecked(true); break;
    }
    dlg->scpHost->clear();
    if (!scpHostHistory.contains(scpHost))
        scpHostHistory.append(scpHost);
    dlg->scpHost->addItems(scpHostHistory);
    dlg->scpHost->setEditText(scpHost);
    dlg->scpParams->setText(scpParams);
    dlg->cbSCP->setChecked(useScp);
    dlg->atlHost->clear();
    if (!atlHostHistory.contains(atlHost))
        atlHostHistory.append(atlHost);
    dlg->atlHost->addItems(atlHostHistory);
    dlg->atlHost->setEditText(atlHost);
    dlg->atlPort->setValue(atlPort);
    dlg->adList->clear();
    dlg->adList->addItems(adblock);
    dlg->useAd->setChecked(useAdblock);
    dlg->useOverrideFont->setChecked(useOverrideFont);
    dlg->overrideStdFonts->setChecked(overrideStdFonts);
    dlg->fontOverride->setCurrentFont(overrideFont);
    QFont f = QApplication::font();
    f.setFamily(fontStandard);
    dlg->fontStandard->setCurrentFont(f);
    f.setFamily(fontFixed);
    dlg->fontFixed->setCurrentFont(f);
    f.setFamily(fontSerif);
    dlg->fontSerif->setCurrentFont(f);
    f.setFamily(fontSansSerif);
    dlg->fontSansSerif->setCurrentFont(f);
    dlg->fontOverrideSize->setValue(overrideFont.pointSize());
    dlg->fontOverride->setEnabled(useOverrideFont);
    dlg->fontOverrideSize->setEnabled(useOverrideFont);
    dlg->fontStandard->setEnabled(overrideStdFonts);
    dlg->fontFixed->setEnabled(overrideStdFonts);
    dlg->fontSerif->setEnabled(overrideStdFonts);
    dlg->fontSansSerif->setEnabled(overrideStdFonts);
    dlg->overrideFontColor->setChecked(forceFontColor);
    dlg->updateFontColorPreview(forcedFontColor);
    dlg->gctxHotkey->setKeySequence(gctxTranHotkey->shortcut());

    if (dlg->exec()==QDialog::Accepted) {
        hostingDir=dlg->hostingDir->text();
        hostingUrl=dlg->hostingUrl->text();
        maxLimit=dlg->maxLimit->value();
        sysEditor=dlg->editor->text();
        sysBrowser=dlg->browser->text();
        maxRecycled=dlg->maxRecycled->value();
        QStringList sl;
        for (int i=0; i<dlg->qrList->count();i++)
            sl << dlg->qrList->item(i)->text();
        searchHistory.clear();
        searchHistory.append(sl);
        updateAllQueryLists();
        if (hostingDir.right(1)!="/") hostingDir=hostingDir+"/";
        if (hostingUrl.right(1)!="/") hostingUrl=hostingUrl+"/";
        bookmarks.clear();
        for (int i=0; i<dlg->bmList->count(); i++)
            bookmarks[dlg->bmList->item(i)->data(Qt::UserRole).toString()] =
                    dlg->bmList->item(i)->data(Qt::UserRole+1).toUrl();
        updateAllBookmarks();
        QWebSettings::globalSettings()->setAttribute(QWebSettings::JavascriptEnabled,
                                                     dlg->useJS->isChecked());
        QWebSettings::globalSettings()->setAttribute(QWebSettings::AutoLoadImages,
                                                     dlg->autoloadImages->isChecked());
        if (dlg->rbNifty->isChecked()) translatorEngine=TE_NIFTY;
        else if (dlg->rbGoogle->isChecked()) translatorEngine=TE_GOOGLE;
        else if (dlg->rbAtlas->isChecked()) translatorEngine=TE_ATLAS;
        else translatorEngine=TE_NIFTY;
        useScp=dlg->cbSCP->isChecked();
        scpHost=dlg->scpHost->lineEdit()->text();
        scpParams=dlg->scpParams->text();
        atlHost=dlg->atlHost->lineEdit()->text();
        atlPort=dlg->atlPort->value();
        if (scpHostHistory.contains(scpHost))
            scpHostHistory.move(scpHostHistory.indexOf(scpHost),0);
        else
            scpHostHistory.prepend(scpHost);
        if (atlHostHistory.contains(atlHost))
            atlHostHistory.move(atlHostHistory.indexOf(atlHost),0);
        else
            atlHostHistory.prepend(atlHost);
        useOverrideFont=dlg->useOverrideFont->isChecked();
        overrideStdFonts=dlg->overrideStdFonts->isChecked();
        overrideFont=dlg->fontOverride->currentFont();
        overrideFont.setPointSize(dlg->fontOverrideSize->value());
        fontStandard=dlg->fontStandard->currentFont().family();
        fontFixed=dlg->fontFixed->currentFont().family();
        fontSerif=dlg->fontSerif->currentFont().family();
        fontSansSerif=dlg->fontSansSerif->currentFont().family();
        forceFontColor=dlg->overrideFontColor->isChecked();
        forcedFontColor=dlg->getOverridedFontColor();
        sl.clear();
        for (int i=0;i<dlg->adList->count();i++)
            sl << dlg->adList->item(i)->text();
        sl.sort();
        adblock.clear();
        adblock.append(sl);
        useAdblock=dlg->useAd->isChecked();
        gctxTranHotkey->setShortcut(dlg->gctxHotkey->keySequence());
        if (!gctxTranHotkey->shortcut().isEmpty())
            gctxTranHotkey->setEnabled();
    }
    dlg->setParent(NULL);
    delete dlg;
    dlg=NULL;
}

void CGlobalControl::cleanTmpFiles()
{
    for (int i=0; i<createdFiles.count(); i++) {
        QFile f(createdFiles[i]);
        f.remove();
    }
}

void CGlobalControl::appendTranThread(QThread *tran)
{
    if (tran==NULL) return;
    connect(tran,SIGNAL(finished()),this,SLOT(tranFinished()));
    tranThreads.append(tran);
    int w = 0;
    foreach (QThread* th, tranThreads) {
        if (th->isRunning()) w++;
    }
    if (w<maxTranThreads) tran->start();
}

void CGlobalControl::tranFinished()
{
    CCalcThread* ct = qobject_cast<CCalcThread*>(sender());
    if (ct==NULL) return;
    tranThreads.removeOne(ct);
    ct->deleteLater();

    foreach (QThread* th, tranThreads)
        if (!th->isRunning() && !th->isFinished()) {
            th->start();
            break;
        }
}

void CGlobalControl::closeLockTimer()
{
    blockTabCloseActive=false;
}

void CGlobalControl::blockTabClose()
{
    blockTabCloseActive=true;
    QTimer::singleShot(500,this,SLOT(closeLockTimer()));
}

void CGlobalControl::authentication(QNetworkReply *reply, QAuthenticator *authenticator)
{
    CAuthDlg *dlg = new CAuthDlg(QApplication::activeWindow(),reply->url(),authenticator->realm());
    if (dlg->exec()) {
        authenticator->setUser(dlg->getUser());
        authenticator->setPassword(dlg->getPassword());
    }
    dlg->setParent(NULL);
    delete dlg;
}

void CGlobalControl::clipboardChanged(QClipboard::Mode mode)
{
    QClipboard *cb = QApplication::clipboard();
    if (mode==QClipboard::Clipboard) {
        if (cb->mimeData(QClipboard::Clipboard)->hasHtml()) {
            lastClipboardContents = cb->mimeData(QClipboard::Clipboard)->html();
            lastClipboardContentsUnformatted = cb->mimeData(QClipboard::Clipboard)->text();
            lastClipboardIsHtml = true;
        } else if (cb->mimeData(QClipboard::Clipboard)->hasText()) {
            lastClipboardContents = cb->mimeData(QClipboard::Clipboard)->text();
            lastClipboardContentsUnformatted = cb->mimeData(QClipboard::Clipboard)->text();
            lastClipboardIsHtml = false;
        } else {
            lastClipboardContents = "";
            lastClipboardContentsUnformatted = "";
            lastClipboardIsHtml = false;
        }
    } else if (mode==QClipboard::Selection) {
        if (cb->mimeData(QClipboard::Selection)->hasHtml()) {
            lastClipboardContents = cb->mimeData(QClipboard::Selection)->html();
            lastClipboardContentsUnformatted = cb->mimeData(QClipboard::Selection)->text();
            lastClipboardIsHtml = true;
        } else if (cb->mimeData(QClipboard::Selection)->hasText()) {
            lastClipboardContents = cb->mimeData(QClipboard::Selection)->text();
            lastClipboardContentsUnformatted = cb->mimeData(QClipboard::Selection)->text();
            lastClipboardIsHtml = false;
        } else {
            lastClipboardContents = "";
            lastClipboardContentsUnformatted = "";
            lastClipboardIsHtml = false;
        }
    }

    if (!lastClipboardContentsUnformatted.isEmpty() && actionGlobalTranslator->isChecked())
        startGlobalContextTranslate(lastClipboardContentsUnformatted);
}

void CGlobalControl::startGlobalContextTranslate(const QString &text)
{
    if (text.isEmpty()) return;
    QThread *th = new QThread();
    CAuxTranslator *at = new CAuxTranslator();
    at->setParams(text);
    connect(this,SIGNAL(startAuxTranslation()),
            at,SLOT(startTranslation()),Qt::QueuedConnection);
    connect(at,SIGNAL(gotTranslation(QString)),
            this,SLOT(globalContextTranslateReady(QString)),Qt::QueuedConnection);
    at->moveToThread(th);
    th->start();

    emit startAuxTranslation();
}

void CGlobalControl::globalContextTranslateReady(const QString &text)
{
    QSpecToolTipLabel* t = new QSpecToolTipLabel(wordWrap(text,80));
    QPoint p = QCursor::pos();
    QxtToolTip::show(p,t,NULL);
}

void CGlobalControl::preShutdown()
{
    writeSettings();
    cleanTmpFiles();
    cleanupAndExit(false);
}

void CGlobalControl::appendSearchHistory(QStringList req)
{
    for(int i=0;i<req.count();i++) {
        int idx = searchHistory.indexOf(req.at(i));
        if (idx>=0)
            searchHistory.removeAt(idx);
        searchHistory.insert(0,req.at(i));
    }
}

void CGlobalControl::appendRecycled(QString title, QUrl url)
{
    int idx = recycleBin.indexOf(UrlHolder(title,url));
    if (idx>=0)
        recycleBin.move(idx,0);
    else
        recycleBin.prepend(UrlHolder(title,url));

    if (recycleBin.count()>maxRecycled) recycleBin.removeLast();

    updateAllRecycleBins();
}

void CGlobalControl::appendMainHistory(UrlHolder &item)
{
    if (mainHistory.contains(item))
        mainHistory.removeOne(item);
    mainHistory.prepend(item);
    updateAllHistoryLists();
}

void CGlobalControl::updateAllBookmarks()
{
    foreach (CMainWindow* w, mainWindows) w->updateBookmarks();
}

void CGlobalControl::updateAllCharsetLists()
{
    foreach (CMainWindow* w, mainWindows) w->reloadCharsetList();
}

void CGlobalControl::updateAllHistoryLists()
{
    foreach (CMainWindow* w, mainWindows) w->updateHistoryList();
}

void CGlobalControl::updateAllQueryLists()
{
    foreach (CMainWindow* w, mainWindows) w->updateQueryHistory();
}

void CGlobalControl::updateAllRecycleBins()
{
    foreach (CMainWindow* w, mainWindows) w->updateRecycled();
}

void CGlobalControl::ipcMessageReceived(const QString &msg)
{
    if (msg==QString("newWindow"))
        addMainWindow();
}

CMainWindow* CGlobalControl::addMainWindow(bool withSearch, bool withViewer)
{
    CMainWindow* mainWindow = new CMainWindow(withSearch,withViewer);
    connect(mainWindow,SIGNAL(aboutToClose(CMainWindow*)),this,SLOT(windowDestroyed(CMainWindow*)));

    mainWindows.append(mainWindow);

    mainWindow->show();

    mainWindow->menuTools->addAction(actionGlobalTranslator);
    mainWindow->menuTools->addAction(actionSelectionDictionary);

    return mainWindow;
}

void CGlobalControl::trayClicked(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
        case QSystemTrayIcon::Trigger:
            if (activeWindow!=NULL) {
                CMainWindow* w = activeWindow;
                w->activateWindow();
                w->raise();
            } else
                addMainWindow();
            break;
        case QSystemTrayIcon::DoubleClick:
            addMainWindow();
            break;
        default:
            break;
    }
}

void CGlobalControl::updateTrayIconState(bool)
{
    QIcon icn = appIcon;
    QIcon icnOut;
    foreach (const QSize& sz, icn.availableSizes()) {
        QPixmap px = icn.pixmap(sz);
        if (actionGlobalTranslator->isChecked()) {
            QPainter p(&px);

            QPen pen = p.pen();
            pen.setColor(QColor(Qt::red));
            p.setPen(pen);
            QBrush brush = p.brush();
            brush.setColor(QColor(Qt::red));
            brush.setStyle(Qt::SolidPattern);
            p.setBrush(brush);
            p.drawEllipse(2*sz.width()/3,0,sz.width()/3,sz.height()/3);
        }
        icnOut.addPixmap(px);
    }
    trayIcon.setIcon(icnOut);
}

void CGlobalControl::focusChanged(QWidget *, QWidget *now)
{
    if (now==NULL) return;
    CMainWindow* mw = qobject_cast<CMainWindow*>(now->window());
    if (mw==NULL) return;
    activeWindow=mw;
}

void CGlobalControl::windowDestroyed(CMainWindow *obj)
{
    if (obj==NULL) return;
    mainWindows.removeOne(obj);
    if (activeWindow==obj) {
        if (mainWindows.count()>0) {
            activeWindow=mainWindows.first();
            activeWindow->activateWindow();
        } else
            activeWindow=NULL;
    }
}

void CGlobalControl::cleanupAndExit(bool appQuit)
{
    if (cleaningState) return;
    cleaningState = true;

    trayIcon.hide();
    trayIcon.setIcon(QIcon());

    if (mainWindows.count()>0) {
        foreach (CMainWindow* w, mainWindows) {
            if (w!=NULL)
                w->close();
            QApplication::processEvents();
        }
        QApplication::processEvents();
    }
    gctxTranHotkey->setDisabled();
    QApplication::processEvents();
    gctxTranHotkey->deleteLater();
    QApplication::processEvents();

    foreach (QThread* th, tranThreads) {
        if (th->isRunning()) {
            th->terminate();
            th->wait(5000);
            th->deleteLater();
        }
    }
    tranThreads.clear();
    if (appQuit)
        QApplication::quit();
}

bool CGlobalControl::isUrlBlocked(QUrl url)
{
    if (!useAdblock) return false;
    QString u = url.toString(QUrl::RemoveUserInfo | QUrl::RemovePort |
                             QUrl::RemoveFragment | QUrl::StripTrailingSlash);
    for(int i=0;i<adblock.count();i++) {
        QRegExp fl(adblock.at(i),Qt::CaseInsensitive,QRegExp::Wildcard);
        if (fl.exactMatch(u)) return true;
    }
    return false;
}

void CGlobalControl::readPassword(const QUrl &origin, QString &user, QString &password)
{
    if (!origin.isValid()) return;
    QSettings settings("kernel1024", "jpreader");
    settings.beginGroup("passwords");
    QString key = QString::fromLatin1(origin.toEncoded().toBase64());

    QString u = settings.value(QString("%1-user").arg(key),QString()).toString();
    QByteArray ba = settings.value(QString("%1-pass").arg(key),QByteArray()).toByteArray();
    QString p = "";
    if (!ba.isEmpty()) {
        p = QString::fromUtf8(QByteArray::fromBase64(ba));
    }

    user = "";
    password = "";
    if (!u.isNull() && !p.isNull()) {
        user = u;
        password = p;
    }
    settings.endGroup();
}

void CGlobalControl::savePassword(const QUrl &origin, const QString &user, const QString &password)
{
    if (!origin.isValid()) return;
    QSettings settings("kernel1024", "jpreader");
    settings.beginGroup("passwords");
    QString key = QString::fromLatin1(origin.toEncoded().toBase64());
    settings.setValue(QString("%1-user").arg(key),user);
    settings.setValue(QString("%1-pass").arg(key),password.toUtf8().toBase64());
    settings.endGroup();
}

UrlHolder::UrlHolder()
{
    UrlHolder::title="";
    UrlHolder::url=QUrl();
    UrlHolder::uuid=QUuid::createUuid();
}

UrlHolder::UrlHolder(QString title, QUrl url)
{
    UrlHolder::title=title;
    UrlHolder::url=url;
    UrlHolder::uuid=QUuid::createUuid();
}

UrlHolder& UrlHolder::operator=(const UrlHolder& other)
{
    title=other.title;
    url=other.url;
    uuid=other.uuid;
    return *this;
}

bool UrlHolder::operator==(const UrlHolder &s) const
{
    return (s.url==url);
}

bool UrlHolder::operator!=(const UrlHolder &s) const
{
    return !operator==(s);
}

DirStruct::DirStruct()
{
    DirStruct::dirName="";
    DirStruct::count=-1;
}

DirStruct::DirStruct(QString DirName, int Count)
{
    DirStruct::dirName=DirName;
    DirStruct::count=Count;
}

DirStruct& DirStruct::operator=(const DirStruct& other)
{
    dirName=other.dirName;
    count=other.count;
    return *this;
}

QDataStream &operator<<(QDataStream &out, const UrlHolder &obj) {
    out << obj.title << obj.url << obj.uuid;
    return out;
}

QDataStream &operator>>(QDataStream &in, UrlHolder &obj) {
    in >> obj.title >> obj.url >> obj.uuid;
    return in;
}
