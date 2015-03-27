#include <QMessageBox>
#include <QTextCodec>
#include <math.h>
#include "searchtab.h"
#include "ui_searchtab.h"
#include "globalcontrol.h"
#include "snviewer.h"
#include "genericfuncs.h"
#include "abstracttranslator.h"

static int sortGMode = -3;

CSearchTab::CSearchTab(CMainWindow *parent) :
    QSpecTabContainer(parent),
    ui(new Ui::SearchTab)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose,true);

    QThread *th = new QThread();
    engine = new CIndexerSearch();
    titleTran = new CTitlesTranslator();

    sortMode = -3;
    lastQuery = "";
    tabTitle = tr("Search");
    selectFile();

    ui->listResults->setEditTriggers(QAbstractItemView::NoEditTriggers);

    ui->translateFrame->hide();

    connect(ui->buttonSearch, SIGNAL(clicked()), this, SLOT(doNewSearch()));
    connect(ui->comboFilter, SIGNAL(currentIndexChanged(int)), this, SLOT(applyFilter(int)));
    connect(ui->listResults, SIGNAL(currentCellChanged(int, int, int, int)), this, SLOT(applySnippet(int, int, int, int)));
    connect(ui->buttonOpen, SIGNAL(clicked()), this, SLOT(showSnippet()));
    connect(ui->listResults, SIGNAL(cellDoubleClicked(int, int)), this, SLOT(execSnippet(int, int)));
    connect(ui->editSearch->lineEdit(), SIGNAL(returnPressed()), ui->buttonSearch, SLOT(click()));
    connect(ui->listResults->horizontalHeader(),SIGNAL(sectionClicked(int)),this,SLOT(columnClicked(int)));
    connect(ui->listResults->verticalHeader(),SIGNAL(sectionClicked(int)),this,SLOT(rowIdxClicked(int)));
    connect(ui->buttonDir, SIGNAL(clicked()), this, SLOT(selectDir()));

    connect(engine,SIGNAL(searchFinished(QBResult,QString)),
            this,SLOT(searchFinished(QBResult,QString)),Qt::QueuedConnection);
    connect(this,SIGNAL(startSearch(QString,QDir)),
            engine,SLOT(doSearch(QString,QDir)),Qt::QueuedConnection);

    connect(titleTran,SIGNAL(gotTranslation(QStringList)),
            this,SLOT(gotTitleTranslation(QStringList)),Qt::QueuedConnection);
    connect(titleTran,SIGNAL(updateProgress(int)),
            this,SLOT(updateProgress(int)),Qt::QueuedConnection);
    connect(ui->translateStopBtn,SIGNAL(clicked()),
            titleTran,SLOT(stop()),Qt::QueuedConnection);
    connect(this,SIGNAL(translateTitlesSrc(QStringList)),
            titleTran,SLOT(translateTitles(QStringList)),Qt::QueuedConnection);

    ui->listResults->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->listResults->horizontalHeader(),SIGNAL(customContextMenuRequested(QPoint)),
            this,SLOT(headerContextMenu(QPoint)));

    ui->buttonSearch->setIcon(QIcon::fromTheme("document-preview"));
    ui->buttonOpen->setIcon(QIcon::fromTheme("document-open"));
    ui->buttonDir->setIcon(QIcon::fromTheme("folder-sync"));

    int mv = round((70* parentWnd->height()/100)/(ui->editSearch->fontMetrics().height()));
    ui->editSearch->setMaxVisibleItems(mv);
    updateQueryHistory();

    bindToTab(parentWnd->tabMain);

    if (engine->getCurrentIndexerService()==SE_NEPOMUK)
        ui->labelMode->setText("Nepomuk search");
    else if (engine->getCurrentIndexerService()==SE_RECOLL)
        ui->labelMode->setText("Recoll search");
    else if (engine->getCurrentIndexerService()==SE_BALOO5)
        ui->labelMode->setText("Baloo KF5 search");
    else
        ui->labelMode->setText("Local file search");

    if (!engine->isValidConfig())
        QMessageBox::warning(parentWnd,tr("JPReader warning"),
                             tr("Configuration error. \n"
                                "You have enabled Nepomuk or Recoll search engine in settings, \n"
                                "but jpreader compiled without support for selected engine.\n"
                                "Fallback to local file search.\n"
                                "Please, check program settings and reopen new search tab."));

    engine->moveToThread(th);
    th->start();

    QThread* th2 = new QThread();
    titleTran->moveToThread(th2);
    th2->start();
}

CSearchTab::~CSearchTab()
{
    delete ui;
}

void CSearchTab::updateQueryHistory()
{
    QString t = ui->editSearch->currentText();
    ui->editSearch->clear();
    ui->editSearch->addItems(gSet->searchHistory);
    ui->editSearch->setEditText(t);
}

void CSearchTab::selectDir()
{
    QString dir = getExistingDirectoryD(parentWnd,tr("Search in"),gSet->savedAuxDir);
    if (!dir.isEmpty()) ui->editDir->setText(dir);
}

bool dirsGreaterThan(const DirStruct& f1, const DirStruct& f2)
{
    if (f1.count==f2.count)
        return (f1.dirName>f2.dirName);
    else
        return (f1.count > f2.count);
}

void CSearchTab::searchFinished(const QBResult &aResult, const QString& aQuery)
{
    ui->buttonSearch->setEnabled(true);
    ui->snippetBrowser->clear();
    ui->snippetBrowser->setToolTip(QString());

    if (aResult.snippets.count() == 0) {
        QMessageBox::information(window(), tr("JPReader"), tr("Nothing found"));
        parentWnd->stSearchStatus.setText(tr("Ready"));
        return;
    }

    result.snippets.clear();
    titleTran->stop();

    lastQuery = aQuery;
    result = aResult;

    ui->comboFilter->clear();
    ui->comboFilter->addItem(tr("show all"));
    QHash<QString, int> dirs;
    for (int i = 0;i < result.snippets.count();i++) {
        if (dirs.contains(result.snippets[i]["Dir"]))
            dirs[result.snippets[i]["Dir"]]++;
        else
            dirs[result.snippets[i]["Dir"]] = 1;
    }
    QList<DirStruct> sortedDirs;
    QHashIterator<QString, int> i(dirs);
    while (i.hasNext()) {
        i.next();
        sortedDirs << DirStruct(i.key(),i.value());
    }
    qSort(sortedDirs.begin(),sortedDirs.end(),dirsGreaterThan);
    QListIterator<DirStruct> j(sortedDirs);
    while (j.hasNext()) {
        DirStruct t=j.next();
        ui->comboFilter->addItem(QString("%1 (%2 snippets)").arg(t.dirName).arg(t.count));
    }
    applySort(sortMode);
    ui->comboFilter->setCurrentIndex(0);

    QString elapsed = QString(result.stats["Elapsed time"]).remove(QRegExp("[s]"));
    QString statusmsg(tr("Found %1 results at %2 seconds").
            arg(result.stats["Total hits"]).
            arg(elapsed));
    ui->labelStatus->setText(statusmsg);
    parentWnd->stSearchStatus.setText(tr("Ready"));
    parentWnd->updateTitle();
    parentWnd->updateTabs();

    setDocTitle(tr("S:[%1]").arg(lastQuery));
}

void CSearchTab::headerContextMenu(const QPoint &pos)
{
    if (result.snippets.count()==0) return;

    QMenu cm(this);
    cm.addAction(tr("Translate titles"),this,SLOT(translateTitles()));

    cm.exec(ui->listResults->mapToGlobal(pos));
}

void CSearchTab::translateTitles()
{
    if (result.snippets.count()==0) return;

    QStringList titles;

    for (int i=0;i<result.snippets.count();i++)
        titles << result.snippets[i]["dc:title"];

    emit translateTitlesSrc(titles);
}

void CSearchTab::gotTitleTranslation(const QStringList &res)
{
    if (!res.isEmpty() && res.last().contains("ERROR")) {
        QMessageBox::warning(this,tr("JPReader"),tr("Title translation failed."));
        return;
    }

    if (res.isEmpty() || result.snippets.count()==0 || res.count()!=result.snippets.count()) return;


    for (int i=0;i<res.count();i++) {
        if (!result.snippets[i].keys().contains("dc:title:saved"))
            result.snippets[i]["dc:title:saved"]=result.snippets[i]["dc:title"];
        result.snippets[i]["dc:title"]=res.at(i);
    }
    applyFilter(ui->comboFilter->currentIndex());
}

void CSearchTab::updateProgress(const int pos)
{
    if (pos>=0) {
        ui->translateBar->setValue(pos);
        if (!ui->translateFrame->isVisible())
            ui->translateFrame->show();
    } else
        ui->translateFrame->hide();
}

void CSearchTab::doSearch()
{
    if (engine->isWorking()) {
        QMessageBox::information(window(),tr("JPReader"),tr("Indexed search engine busy. Try later."));
        return;
    }

    parentWnd->stSearchStatus.setText(tr("Search in progress..."));
    ui->buttonSearch->setEnabled(false);
    ui->snippetBrowser->setHtml("<html><body><div align='center'><font size='+2'>Search in progress...</font></div></body></html>");

    QString searchTerm = ui->editSearch->currentText();

    int idx = ui->editSearch->findText(searchTerm,Qt::MatchFixedString);
    if (idx!=-1)
        ui->editSearch->removeItem(idx);
    ui->editSearch->insertItem(0,searchTerm);

    ui->editSearch->setCurrentIndex(0);

    QDir fsdir = QDir("/");
    if (!ui->editDir->text().isEmpty())
        fsdir = QDir(ui->editDir->text());
    emit startSearch(searchTerm,fsdir);
}

void CSearchTab::searchTerm(const QString &term)
{
    if (engine->isWorking()) {
        QMessageBox::warning(this,tr("JPReader"),tr("Indexed search engine busy, try later."));
        return;
    }
    ui->editSearch->setEditText(term);
    doNewSearch();
}

QString CSearchTab::getDocTitle()
{
    return tabTitle;
}

void CSearchTab::setDocTitle(const QString& title)
{
    tabTitle = title;
    int idx = parentWnd->tabMain->indexOf(this);
    if (idx>=0 && idx<parentWnd->tabMain->count()) {
        parentWnd->tabMain->setTabText(idx,tabTitle);
    }
}

void CSearchTab::doNewSearch()
{
    ui->listResults->clear();
    ui->listResults->setRowCount(0);
    ui->listResults->setColumnCount(0);
    ui->snippetBrowser->clear();
    ui->snippetBrowser->setToolTip(QString());
    ui->comboFilter->clear();
    selectFile();
    doSearch();
}

void CSearchTab::rowIdxClicked(int row)
{
    execSnippet(row,0);
}

void CSearchTab::applyFilter(int idx)
{
    if (!(result.presented)) return ;
    if ((idx < 0) || (idx >= ui->comboFilter->count())) return ;
    ui->listResults->clear();
    ui->snippetBrowser->clear();
    ui->snippetBrowser->setToolTip(QString());
    selectFile();

    QString flt = ui->comboFilter->itemText(idx).remove(QRegExp("\\(.*\\)")).trimmed();

    ui->listResults->setColumnCount(5);
    ui->listResults->setRowCount(0);

    int titleLen = 0; int scoreLen = ui->listResults->fontMetrics().width("Rel. XX"); int groupLen = 0;
    int sizeLen = 0; int fnameLen = 0;

    ui->listResults->setHorizontalHeaderLabels(QStringList() << tr("Title") << tr("Rel.") <<
                                           tr("Group") << tr("Size") << tr("Filename"));
    for (int i = 0;i < result.snippets.count();i++) {
        if ((idx == 0) || ((idx > 0) && (result.snippets[i]["Dir"] == flt))) {

            QString s = result.snippets[i]["dc:title"];
            QTableWidgetItem* title = new QTableWidgetItem(s);
            if (ui->listResults->fontMetrics().width(s) > titleLen) titleLen = ui->listResults->fontMetrics().width(s);
            title->setData(Qt::UserRole, QVariant(i));

            s = result.snippets[i]["Dir"];
            QTableWidgetItem* dir = new QTableWidgetItem(s);
            dir->setStatusTip(result.snippets[i]["DisplayFilename"]);
            if (ui->listResults->fontMetrics().width(s) > groupLen) groupLen = ui->listResults->fontMetrics().width(s);

            s = result.snippets[i]["FileSize"];
            QTableWidgetItem* fsize = new QTableWidgetItem(s);
            if (ui->listResults->fontMetrics().width(s) > sizeLen) sizeLen = ui->listResults->fontMetrics().width(s);

            s = result.snippets[i]["Score"];
            s = QString("%1").arg(s.toDouble(), 0, 'f', 1);
            QTableWidgetItem* score = new QTableWidgetItem(s);
            if (ui->listResults->fontMetrics().width(s) > scoreLen) scoreLen = ui->listResults->fontMetrics().width(s);

            s = result.snippets[i]["OnlyFilename"];
            QTableWidgetItem* fname = new QTableWidgetItem(s);
            if (ui->listResults->fontMetrics().width(s) > fnameLen) fnameLen = ui->listResults->fontMetrics().width(s);

            ui->listResults->insertRow(ui->listResults->rowCount());
            int ridx = ui->listResults->rowCount() - 1;
            ui->listResults->setItem(ridx, 0, title);
            ui->listResults->setItem(ridx, 1, score);
            ui->listResults->setItem(ridx, 2, dir);
            ui->listResults->setItem(ridx, 3, fsize);
            ui->listResults->setItem(ridx, 4, fname);
            if (result.snippets[i].contains("dc:title:saved"))
                ui->listResults->item(ridx,0)->setToolTip(result.snippets[i]["dc:title:saved"]);
        }
    }
    //int maxWidth = listResults->width() / 2;
    QFontMetrics fm(ui->listResults->font());

    int titleMaxWidth = fm.width('X') * 50;
    int scoreMaxWidth = fm.width('X') * 7;
    int groupMaxWidth = fm.width('X') * 22;
    int sizeMaxWidth =  fm.width('X') * 10;
    int fnameMaxWidth = fm.width('X') * 25;

    if (titleLen >  titleMaxWidth) titleLen = titleMaxWidth;
    if (scoreLen > scoreMaxWidth) scoreLen = scoreMaxWidth;
    if (groupLen > groupMaxWidth) groupLen = groupMaxWidth;
    if (sizeLen > sizeMaxWidth) sizeLen = sizeMaxWidth;
    if (fnameLen > fnameMaxWidth) fnameLen = fnameMaxWidth;

    ui->listResults->setColumnWidth(0, titleLen + 20);
    ui->listResults->setColumnWidth(1, scoreLen + 20);
    ui->listResults->setColumnWidth(2, groupLen + 20);
    ui->listResults->setColumnWidth(3, sizeLen + 20);
    ui->listResults->setColumnWidth(4, fnameLen + 20);

    if (sortMode>0)
        ui->listResults->horizontalHeader()->setSortIndicator(abs(sortMode)-1,Qt::DescendingOrder);
    else
        ui->listResults->horizontalHeader()->setSortIndicator(abs(sortMode)-1,Qt::AscendingOrder);
    ui->listResults->horizontalHeader()->setSortIndicatorShown(true);
}

bool snippetsLessThan(const QStrHash& f1, const QStrHash& f2)
{
    switch (abs(sortGMode)) {
        case 1: return (f1["FileTitle"] < f2["FileTitle"]);
        case 2: return (f1["Score"].toDouble() < f2["Score"].toDouble());
        case 3: return (f1["FilePath"] < f2["FilePath"]);
        case 4: return (f1["FileSizeNum"].toULongLong() < f2["FileSizeNum"].toULongLong() );
        case 5: return (f1["OnlyFilename"] < f2["OnlyFilename"]);
        default: return (f1["FileTitle"] < f2["FileTitle"]);
    }
}

bool snippetsGreaterThan(const QStrHash& f1, const QStrHash& f2)
{
    switch (abs(sortGMode)) {
        case 1: return (f1["FileTitle"] > f2["FileTitle"]);
        case 2: return (f1["Score"].toDouble() > f2["Score"].toDouble());
        case 3: return (f1["FilePath"] > f2["FilePath"]);
        case 4: return (f1["FileSizeNum"].toULongLong() > f2["FileSizeNum"].toULongLong() );
        case 5: return (f1["OnlyFilename"] > f2["OnlyFilename"]);
        default: return (f1["FileTitle"] > f2["FileTitle"]);
    }
}

void CSearchTab::applySort(int idx)
{
    if (!(result.presented)) return ;
    if (abs(idx)>5) return ;

    sortMode = idx;
    gSet->sortMutex.lock();
    sortGMode = sortMode;
    if (idx>0)
        qSort(result.snippets.begin(), result.snippets.end(), snippetsLessThan);
    else
        qSort(result.snippets.begin(), result.snippets.end(), snippetsGreaterThan);
    gSet->sortMutex.unlock();
    applyFilter(ui->comboFilter->currentIndex());
}

void CSearchTab::applySnippet(int row, int, int, int)
{
    if (!(result.presented)) return ;
    if (result.snippets.count()==0 ||
            ui->listResults->item(row, 0)==NULL) {
        selectFile();
        ui->snippetBrowser->clear();
        ui->snippetBrowser->setToolTip(QString());
    } else {
        if ((row < 0) || (row >= result.snippets.count())) return ;
        int sidx = ui->listResults->item(row, 0)->data(Qt::UserRole).toInt();
        QString s = result.snippets[sidx]["Snip"];
        QString tranState = bool2str(gSet->actionSnippetAutotranslate->isChecked());
        if (s.isEmpty() || tranState!=result.snippets[sidx].value("SnipTran")) {
            s = createSpecSnippet(result.snippets[sidx]["FullFilename"]);
            result.snippets[sidx]["Snip"]=s;
            result.snippets[sidx]["SnipUntran"]=createSpecSnippet(result.snippets[sidx]["FullFilename"],true);
            result.snippets[sidx]["SnipTran"]=bool2str(gSet->actionSnippetAutotranslate->isChecked());
        }
        s="<font size='+1'>"+s+"</font>";
        ui->snippetBrowser->setHtml(s);
        if (gSet->actionSnippetAutotranslate->isChecked() &&
            result.snippets[sidx].value("Snip")!=result.snippets[sidx].value("SnipUntran"))
            ui->snippetBrowser->setToolTip(result.snippets[sidx].value("SnipUntran"));

        selectFile(result.snippets[sidx]["Uri"], result.snippets[sidx]["DisplayFilename"]);
    }

}

QString CSearchTab::createSpecSnippet(QString aFilename, bool forceUntranslated) {
    QString s = "";
    if(lastQuery.isEmpty()) return s;
    QFileInfo fi(aFilename);
    if (!fi.exists()) {
        s=tr("File not found in directory.");
        return s;
    }

    QStringList queryTerms = splitQuery(lastQuery);
    if (queryTerms.count()==0) {
        s=tr("Search query is empty.");
        return s;
    }

    QFile f(aFilename);
    if (!f.open(QIODevice::ReadOnly)) {
        s=tr("File not found");
        return s;
    }
    if (f.size()>25*1024*1024) {
        s=tr("File too big");
        return s;
    }
    QByteArray fc = f.readAll();
    f.close();

    QTextCodec *cd = detectEncoding(fc);

    QString fileContents = cd->toUnicode(fc.constData());
    QTextEdit tagRemover(fileContents);
    fileContents = tagRemover.toPlainText();
    fileContents.remove("\n");
    fileContents.remove("\r");
    QHash<int,QStringList> snippets;

    QStringList queryTermsTran = queryTerms;
    CAbstractTranslator* tran = translatorFactory(this);
    if (gSet->actionSnippetAutotranslate->isChecked() && !forceUntranslated) {
        if (tran==NULL || !tran->initTran()) {
            qDebug() << tr("Unable to initialize translation engine.");
            QMessageBox::warning(this,tr("JPReader"),tr("Unable to initialize translation engine."));
        } else {
            for (int i=0;i<queryTerms.count();i++) {
                queryTermsTran[i] = tran->tranString(queryTerms[i]);
            }
        }
    }

    const int snipWidth = 12;
    const int maxSnippets = 800 / snipWidth;

    for (int i=0;i<queryTerms.count();i++) {
        QString ITerm = queryTerms.value(i).trimmed();
        QString ITermT = queryTermsTran.value(i).trimmed();
        while (snippets[i].length()<maxSnippets && fileContents.contains(ITerm,Qt::CaseInsensitive)) {
            // extract term occurence from text
            int fsta = fileContents.indexOf(ITerm,0,Qt::CaseInsensitive)-snipWidth;
            if (fsta<0) fsta=0;
            int fsto = fileContents.indexOf(ITerm,0,Qt::CaseInsensitive)+ITerm.length()+snipWidth;
            if (fsto>=fileContents.length()) fsto=fileContents.length()-1;
            QString fspart = fileContents.mid(fsta,fsto-fsta);
            fileContents.remove(fsta,fsto-fsta);
            bool makeTran = tran!=NULL &&
                            gSet->actionSnippetAutotranslate->isChecked() &&
                            tran->isReady() &&
                            !forceUntranslated;
            if (makeTran)
                fspart = tran->tranString(fspart);
            QString snpColor = gSet->snippetColors[i % gSet->snippetColors.count()].name();
            if (makeTran)
                fspart.replace(ITermT,"<font color='"+snpColor+"'>"+ITermT+"</font>",Qt::CaseInsensitive);
            else
                fspart.replace(ITerm,"<font color='"+snpColor+"'>"+ITerm+"</font>",Qt::CaseInsensitive);
            // add occurence to collection
            snippets[i] << fspart;
        }
    }
    if (tran!=NULL) {
        if (tran->isReady())
            tran->doneTran();
        tran->deleteLater();
    }

    // *** Weighted sorting ***
    // calculate term weights
    QList<float> fweights;
    float fsumWeight = 0.0;
    float fminWeight = 1.0;
    int sumCount = 0;
    for (int i=0;i<queryTerms.count();i++) {
        float wght = 1.0/(float)(snippets[i].count());
        if (wght < fminWeight) fminWeight = wght;
        fsumWeight += wght;
        fweights << wght;
        sumCount += snippets[i].count();
    }
    // normalization
    QList<int> iweights;
    int isumWeight = 0;
    for (int i=0;i<queryTerms.count();i++) {
        int wght = fweights[i]/fminWeight;
        iweights << wght;
        isumWeight += wght;
    }

    QStringList rdSnippet;
    int maxcnt = sumCount;
    if (maxSnippets<sumCount) maxcnt = maxSnippets;
    for (int i=0;i<maxcnt;i++) {
        int rnd = qrand() % isumWeight;
        int itm = 0;
        for (int j=0;j<queryTerms.count();j++) {
            if (rnd<iweights[j]) {
                itm = j;
                break;
            }
            rnd -= iweights[j];
        }
        if (snippets[itm].count()>0)
            rdSnippet << snippets[itm].takeFirst();
        else { // sublist is empty, take other smallest element from collection
            int maxCnt = INT_MAX;
            int idx = -1;
            for (int j=0;j<queryTerms.count();j++) {
                if (snippets[j].count()>0 && snippets[j].count()<maxCnt) {
                    maxCnt = snippets[j].count();
                    idx = j;
                }
            }
            if (idx>=0)
                rdSnippet << snippets[idx].takeFirst();
        }
    }
    // *** ----------- ***

    QString snippet;
    for (int i=0;i<rdSnippet.count();i++)
        snippet+=rdSnippet[i]+"&nbsp;&nbsp;&nbsp;...&nbsp;&nbsp;&nbsp;";
    return snippet;
}

QStringList CSearchTab::splitQuery(QString aQuery) {
    QString sltx=aQuery;
    QStringList res;
    QString tmp;
    bool qmark=false;
    int i=0;
    while (true) {
        if (sltx[i]=='\"') {
            if (qmark) { // closing mark
                tmp=sltx.mid(sltx.indexOf('\"')+1,i-1-sltx.indexOf('\"'));
                sltx.remove(sltx.indexOf('\"'),i+1-sltx.indexOf('\"'));
                qmark=false;
                res << tmp.trimmed();
                i=0;
            } else
                qmark=true;
        }
        if ((i>0) && (!qmark))
            if ((sltx[i]==' ') && (sltx[i-1]!=' ')) {
                tmp=sltx.mid(0,i);
                sltx.remove(0,i+1);
                res << tmp.trimmed();
                i=0;
            }
        i++;
        if (i>=sltx.length()) {
            if (sltx.trimmed().length()>0)
                res << sltx.trimmed();
            break;
        }
    }
    return res;
}

void CSearchTab::showSnippet()
{
    QStringList sl = splitQuery(ui->editSearch->currentText());

    new CSnippetViewer(parentWnd, selectedUri, sl);
}

void CSearchTab::selectFile(const QString& uri, const QString& dispFilename)
{
    if (!uri.isEmpty()) {
        ui->buttonOpen->setEnabled(true);
        selectedUri = uri;
        ui->filenameEdit->setText(tr("%1").arg(dispFilename));
    } else {
        ui->buttonOpen->setEnabled(false);
        selectedUri = "";
        ui->filenameEdit->setText(tr(""));
    }
}

void CSearchTab::columnClicked(int col)
{
    if (abs(sortMode)==(col+1))
        applySort(-sortMode);
    else
        applySort(col+1);
}

void CSearchTab::execSnippet(int row, int column)
{
    applySnippet(row, column, 0, 0);
    showSnippet();
}

void CSearchTab::keyPressEvent(QKeyEvent *event)
{
    if (event->key()==Qt::Key_F3)
        execSnippet(ui->listResults->currentRow(),0);
    else
        QWidget::keyPressEvent(event);
}
