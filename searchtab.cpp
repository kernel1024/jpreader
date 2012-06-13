#include "searchtab.h"
#include "ui_searchtab.h"
#include "globalcontrol.h"
#include "snviewer.h"
#include "genericfuncs.h"

static int sortGMode = -3;

CSearchTab::CSearchTab(QWidget *parent, CMainWindow* parentWnd) :
    QWidget(parent),
    ui(new Ui::SearchTab)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose,true);

    mainWnd = parentWnd;
    sortMode = -3;
    lastQuery = "";
    selectFile();

    ui->listResults->setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(ui->buttonSearch, SIGNAL(clicked()), this, SLOT(doNewSearch()));
    connect(ui->comboFilter, SIGNAL(currentIndexChanged(int)), this, SLOT(applyFilter(int)));
    connect(ui->listResults, SIGNAL(currentCellChanged(int, int, int, int)), this, SLOT(applySnippet(int, int, int, int)));
    connect(ui->buttonOpen, SIGNAL(clicked()), this, SLOT(showSnippet()));
    connect(ui->listResults, SIGNAL(cellDoubleClicked(int, int)), this, SLOT(execSnippet(int, int)));
    connect(ui->editSearch->lineEdit(), SIGNAL(returnPressed()), ui->buttonSearch, SLOT(click()));
    connect(ui->listResults->horizontalHeader(),SIGNAL(sectionClicked(int)),this,SLOT(columnClicked(int)));
    connect(ui->listResults->verticalHeader(),SIGNAL(sectionClicked(int)),this,SLOT(rowIdxClicked(int)));
    connect(ui->buttonDir, SIGNAL(clicked()), this, SLOT(selectDir()));
    connect(&nepomuk,SIGNAL(searchFinished()),this,SLOT(searchFinished()));

    ui->buttonSearch->setIcon(QIcon::fromTheme("document-preview"));
    ui->buttonOpen->setIcon(QIcon::fromTheme("document-open"));
    ui->buttonDir->setIcon(QIcon::fromTheme("folder-sync"));

    int mv = round((70* mainWnd->height()/100)/(ui->editSearch->fontMetrics().height()));
    ui->editSearch->setMaxVisibleItems(mv);
    updateQueryHistory();

    bindToTab(parentWnd->tabMain);
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
    QString dir = getExistingDirectoryD(mainWnd,tr("Search in"),gSet->savedAuxDir);
    if (!dir.isEmpty()) ui->editDir->setText(dir);
}

bool dirsGreaterThan(const DirStruct& f1, const DirStruct& f2)
{
    if (f1.count==f2.count)
        return (f1.dirName>f2.dirName);
    else
        return (f1.count > f2.count);
}

void CSearchTab::searchFinished()
{
    ui->buttonSearch->setEnabled(true);
    ui->snippetBrowser->clear();

    if (nepomuk.result.snippets.count() == 0) {
        QMessageBox::information(window(), tr("JPReader"), tr("Nothing found"));
        mainWnd->stSearchStatus.setText(tr("Ready"));
        return;
    }

    lastQuery = nepomuk.query;
    result = nepomuk.result;

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
    mainWnd->stSearchStatus.setText(tr("Ready"));
    mainWnd->updateTitle();
    mainWnd->updateTabs();

    int idx = mainWnd->tabMain->indexOf(this);
    if (idx>=0 && idx<mainWnd->tabMain->count())
        mainWnd->tabMain->setTabText(idx,tr("B:[%1]").arg(lastQuery));
}

void CSearchTab::doSearch()
{
    if (nepomuk.working) {
        QMessageBox::information(window(),tr("JPReader"),tr("Nepomuk engine busy. Try later."));
        return;
    }

    mainWnd->stSearchStatus.setText(tr("Search in progress..."));
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
    nepomuk.doSearch(searchTerm,fsdir);
}

void CSearchTab::searchTerm(const QString &term)
{
    if (nepomuk.working) {
        QMessageBox::warning(this,tr("JPReader"),tr("Nepomuk engine busy, try later."));
        return;
    }
    ui->editSearch->setEditText(term);
    doNewSearch();
}

void CSearchTab::doNewSearch()
{
    ui->listResults->clear();
    ui->listResults->setRowCount(0);
    ui->listResults->setColumnCount(0);
    ui->snippetBrowser->clear();
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

            QString s = result.snippets[i]["FileTitle"];
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
        ui->listResults->horizontalHeader()->setSortIndicator(abs(sortMode)-1,Qt::AscendingOrder);
    else
        ui->listResults->horizontalHeader()->setSortIndicator(abs(sortMode)-1,Qt::DescendingOrder);
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
    } else {
        if ((row < 0) || (row >= result.snippets.count())) return ;
        int sidx = ui->listResults->item(row, 0)->data(Qt::UserRole).toInt();
        QString s = result.snippets[sidx]["Snip"];
        if (s.isEmpty()) {
            s = createSpecSnippet(result.snippets[sidx]["FullFilename"]);
            result.snippets[sidx]["Snip"]=s;
        }
        s="<font size='+1'>"+s+"</font>";
        ui->snippetBrowser->setHtml(s);

        selectFile(result.snippets[sidx]["Uri"], result.snippets[sidx]["DisplayFilename"]);
    }

}

QString CSearchTab::createSpecSnippet(QString aFilename) {
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

    const int snipWidth = 12;
    const int maxSnippets = 800 / snipWidth;

    for (int i=0;i<queryTerms.count();i++) {
        QString ITerm = queryTerms[i];
        ITerm = ITerm.trimmed();
        while (snippets[i].length()<maxSnippets && fileContents.contains(ITerm,Qt::CaseInsensitive)) {
            // extract term occurence from text
            int fsta = fileContents.indexOf(ITerm,0,Qt::CaseInsensitive)-snipWidth;
            if (fsta<0) fsta=0;
            int fsto = fileContents.indexOf(ITerm,0,Qt::CaseInsensitive)+ITerm.length()+snipWidth;
            if (fsto>=fileContents.length()) fsto=fileContents.length()-1;
            QString fspart = fileContents.mid(fsta,fsto-fsta);
            fileContents.remove(fsta,fsto-fsta);
            QString snpColor = gSet->snippetColors[i % gSet->snippetColors.count()].name();
            fspart.replace(ITerm,"<font color='"+snpColor+"'>"+ITerm+"</font>",Qt::CaseInsensitive);
            // add occurence to collection
            snippets[i] << fspart;
        }
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

    new CSnippetViewer(mainWnd, selectedUri, sl);
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

void CSearchTab::closeTab(bool nowait)
{
    if (!nowait) {
        if (gSet->blockTabCloseActive) return;
        gSet->blockTabClose();
    }
    if (tabWidget!=NULL) {
        if (mainWnd->lastTabIdx>=0) tabWidget->setCurrentIndex(mainWnd->lastTabIdx);
        tabWidget->removeTab(tabWidget->indexOf(this));
    }
    deleteLater();
    mainWnd->updateTitle();
    mainWnd->updateTabs();
    mainWnd->checkTabs();
}

void CSearchTab::bindToTab(QSpecTabWidget* tabs)
{
    tabWidget = tabs;
    if (tabWidget==NULL) return;
    QPushButton* b = new QPushButton(QIcon::fromTheme("dialog-close"),"");
    b->setFlat(true);
    int sz = tabWidget->tabBar()->fontMetrics().height();
    b->resize(QSize(sz,sz));
    connect(b,SIGNAL(clicked()),this,SLOT(closeTab()));
    int i = tabWidget->addTab(this,tr("Search"));
    tabWidget->tabBar()->setTabButton(i,QTabBar::LeftSide,b);
    tabWidget->setCurrentWidget(this);
    mainWnd->updateTitle();
    mainWnd->updateTabs();
}

void CSearchTab::keyPressEvent(QKeyEvent *event)
{
    if (event->key()==Qt::Key_F3)
        execSnippet(ui->listResults->currentRow(),0);
    else
        QWidget::keyPressEvent(event);
}
