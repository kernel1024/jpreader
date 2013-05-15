#include <QInputDialog>
#include <QMessageBox>
#include <QColorDialog>

#include "settingsdlg.h"
#include "ui_settingsdlg.h"
#include "mainwindow.h"
#include "globalcontrol.h"
#include "genericfuncs.h"

CSettingsDlg::CSettingsDlg(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDlg)
{
    ui->setupUi(this);

    hostingUrl=ui->editHostingUrl;
    hostingDir=ui->editHostingDir;
    maxLimit=ui->spinMaxLimit;
    browser=ui->editBrowser;
    editor=ui->editEditor;
	qrList=ui->listQr;
	bmList=ui->listBookmarks;
    rbNifty=ui->radioNifty;
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
    hsList=ui->listHistory;
    useJS=ui->checkJS;
    adList=ui->listAdblock;
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
    searchNepomuk=ui->radioSearchNepomuk;
    searchRecoll=ui->radioSearchRecoll;
    searchNone=ui->radioSearchNone;

    connect(ui->buttonHostingDir,SIGNAL(clicked()),this,SLOT(selectDir()));
    connect(ui->buttonBrowser,SIGNAL(clicked()),this,SLOT(selectBrowser()));
    connect(ui->buttonEditor,SIGNAL(clicked()),this,SLOT(selectEditor()));
	connect(ui->buttonDelQr,SIGNAL(clicked()),this,SLOT(delQrs()));
	connect(ui->buttonDelBookmark,SIGNAL(clicked()),this,SLOT(delBkm()));
    connect(ui->buttonClearCookies,SIGNAL(clicked()),this,SLOT(clearCookies()));
    connect(ui->buttonCleanHistory,SIGNAL(clicked()),this,SLOT(clearHistory()));
    connect(ui->buttonGoHistory,SIGNAL(clicked()),this,SLOT(goHistory()));
    connect(ui->buttonAdAdd,SIGNAL(clicked()),this,SLOT(addAd()));
    connect(ui->buttonAdDel,SIGNAL(clicked()),this,SLOT(delAd()));
    connect(ui->buttonAdImport,SIGNAL(clicked()),this,SLOT(importAd()));
    connect(ui->buttonAdImportOpera,SIGNAL(clicked()),this,SLOT(importAdOpera()));
    connect(ui->buttonFontColorOverride,SIGNAL(clicked()),this,SLOT(fontColorDlg()));
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
	QList<QListWidgetItem *> dl =qrList->selectedItems();
	foreach (QListWidgetItem* i, dl) {
		qrList->removeItemWidget(i);
		delete i;
	}
}

void CSettingsDlg::delBkm()
{
	QList<QListWidgetItem *> dl =bmList->selectedItems();
	foreach (QListWidgetItem* i, dl) {
		bmList->removeItemWidget(i);
		delete i;
	}
}

void CSettingsDlg::clearCookies()
{
    if (gSet!=NULL)
        gSet->cookieJar.clearCookies();
}

void CSettingsDlg::clearHistory()
{
    if (gSet!=NULL) {
        gSet->mainHistory.clear();
        gSet->updateAllHistoryLists();
    }
	hsList->clear();
}

void CSettingsDlg::goHistory()
{
    QListWidgetItem* it = hsList->currentItem();
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
    adList->addItem(s);
}

void CSettingsDlg::delAd()
{
    QList<QListWidgetItem *> dl =adList->selectedItems();
    foreach (QListWidgetItem* i, dl) {
        adList->removeItemWidget(i);
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
    adList->addItems(lnks);
}

void CSettingsDlg::importAdOpera()
{
    QString fname = getOpenFileNameD(this,tr("Import rules from Opera urlfilter"),QDir::home().filePath(".opera/urlfilter.ini"),"urlfilter.ini");
    if (fname.isEmpty()) return;

    QFile ini(fname);
    if (!ini.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this,tr("JPReader"),tr("Unable to open file"));
        return;
    }
    QTextStream inird(&ini);
    bool rdlinks = false;
    QStringList lnks;
    lnks.clear();
    while (!inird.atEnd()) {
        QString s = inird.readLine();
        if (rdlinks && (s.isEmpty() || s.left(1)=="[")) rdlinks=false;
        if (rdlinks) {
            if (!s.contains("=UUID:")) continue;
            s.remove(QRegExp("=UUID:.*$"));
            lnks << s;
        }
        if (s.contains("[exclude]")) rdlinks=true;
    }
    ini.close();
    adList->addItems(lnks);
}

void CSettingsDlg::fontColorDlg()
{
    QColor c = QColorDialog::getColor(overridedFontColor,this);
    if (!c.isValid()) return;
    updateFontColorPreview(c);
}

void CSettingsDlg::updateFontColorPreview(const QColor &c)
{
    overridedFontColor = c;
    QPalette p = ui->frameFontColorOverride->palette();
    p.setBrush(QPalette::Window,QBrush(overridedFontColor));
    ui->frameFontColorOverride->setPalette(p);
}

QColor CSettingsDlg::getOverridedFontColor()
{
    return overridedFontColor;
}
