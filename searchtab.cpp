#include <QMessageBox>
#include <QTextCodec>
#include <math.h>
#include "searchtab.h"
#include "ui_searchtab.h"
#include "globalcontrol.h"
#include "snviewer.h"
#include "genericfuncs.h"
#include "abstracttranslator.h"

CSearchTab::CSearchTab(CMainWindow *parent) :
    CSpecTabContainer(parent),
    ui(new Ui::SearchTab)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose,true);

    QThread *th = new QThread();
    engine = new CIndexerSearch();
    titleTran = new CTitlesTranslator();

    lastQuery = "";
    tabTitle = tr("Search");
    selectFile();

    ui->listResults->setEditTriggers(QAbstractItemView::NoEditTriggers);

    ui->translateFrame->hide();

    model = new CSearchModel(this,ui->listResults);
    sort = new QSortFilterProxyModel(this);
    sort->setSourceModel(model);
    sort->setSortRole(Qt::UserRole + cpSortRole);
    sort->setFilterRole(Qt::UserRole + cpFilterRole);
    ui->listResults->setModel(sort);

    connect(model, SIGNAL(itemContentsUpdated()), sort, SLOT(invalidate()));

    QHeaderView *hh = ui->listResults->horizontalHeader();
    if (hh!=NULL) {
        hh->setToolTip(tr("Right click for advanced commands on results list"));
        hh->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(hh,SIGNAL(customContextMenuRequested(QPoint)),
                this,SLOT(headerMenu(QPoint)));
    }

    connect(ui->buttonSearch, SIGNAL(clicked()), this, SLOT(doNewSearch()));
    connect(ui->listResults->selectionModel(),SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            this,SLOT(applySnippet(QItemSelection,QItemSelection)));
    connect(ui->buttonOpen, SIGNAL(clicked()), this, SLOT(showSnippet()));
    connect(ui->listResults, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(execSnippet(QModelIndex)));
    connect(ui->editSearch->lineEdit(), SIGNAL(returnPressed()), ui->buttonSearch, SLOT(click()));
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

    ui->buttonSearch->setIcon(QIcon::fromTheme("document-preview"));
    ui->buttonOpen->setIcon(QIcon::fromTheme("document-open"));
    ui->buttonDir->setIcon(QIcon::fromTheme("folder-sync"));

    int mv = round((70* parentWnd->height()/100)/(ui->editSearch->fontMetrics().height()));
    ui->editSearch->setMaxVisibleItems(mv);
    updateQueryHistory();

    bindToTab(parentWnd->tabMain);

    if (engine->getCurrentIndexerService()==SE_RECOLL)
        ui->labelMode->setText("Recoll search");
    else if (engine->getCurrentIndexerService()==SE_BALOO5)
        ui->labelMode->setText("Baloo KF5 search");
    else
        ui->labelMode->setText("Local file search");

    if (!engine->isValidConfig())
        QMessageBox::warning(parentWnd,tr("JPReader"),
                             tr("Configuration error. \n"
                                "You have enabled some search engine in settings, \n"
                                "but jpreader compiled without support for this engine.\n"
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
    QString dir = getExistingDirectoryD(parentWnd,tr("Search in"),gSet->settings.savedAuxDir);
    if (!dir.isEmpty()) ui->editDir->setText(dir);
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

    model->deleteAllItems();
    titleTran->stop();

    lastQuery = aQuery;
    model->addItems(aResult.snippets);
    ui->listResults->sortByColumn(1,Qt::DescendingOrder);
    stats = aResult.stats;

    QString elapsed = QString(stats["Elapsed time"]).remove(QRegExp("[s]"));
    QString statusmsg(tr("Found %1 results at %2 seconds").
            arg(stats["Total hits"]).
            arg(elapsed));
    ui->labelStatus->setText(statusmsg);
    parentWnd->stSearchStatus.setText(tr("Ready"));
    parentWnd->updateTitle();
    parentWnd->updateTabs();

    setDocTitle(tr("S:[%1]").arg(lastQuery));
}

void CSearchTab::translateTitles()
{
    if (model->rowCount()==0) return;

    QStringList titles;

    for (int i=0;i<model->rowCount();i++)
        titles << model->getSnippet(i)["dc:title"];

    emit translateTitlesSrc(titles);
}

void CSearchTab::gotTitleTranslation(const QStringList &res)
{
    if (!res.isEmpty() && res.last().contains("ERROR")) {
        QMessageBox::warning(this,tr("JPReader"),tr("Title translation failed."));
        return;
    }

    if (res.isEmpty() || model->rowCount()==0 || res.count()!=model->rowCount()) return;

    for (int i=0;i<res.count();i++) {
        QStrHash snip = model->getSnippet(i);
        if (!snip.contains("dc:title:saved"))
            snip["dc:title:saved"]=snip["dc:title"];
        snip["dc:title"]=res.at(i);
        model->setSnippet(i,snip);
    }
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

void CSearchTab::headerMenu(const QPoint &pos)
{
    if (model->rowCount()==0) return;
    QHeaderView* hh = qobject_cast<QHeaderView *>(sender());
    if (hh==NULL) return;

    int column = hh->logicalIndexAt(pos);

    QMenu cm(this);

    if (column == 2) { // Directory filter
        QStringList sl = model->getDistinctValues("Dir");
        if (!sl.isEmpty()) {
            QMenu *cmf = cm.addMenu(tr("Filter"));
            QAction *ac = cmf->addAction(tr("Clear filter"),this,SLOT(applyFilter()));
            ac->setData(QString());
            cmf->addSeparator();
            for (int i=0;i<sl.count();i++) {
                ac = cmf->addAction(sl.at(i),this,SLOT(applyFilter()));
                ac->setData(sl.at(i));
            }
            cm.addSeparator();
        }
    }
    cm.addAction(tr("Translate titles"),this,SLOT(translateTitles()));

    cm.exec(hh->mapToGlobal(pos));
}

void CSearchTab::applyFilter()
{
    QAction *ac = qobject_cast<QAction *>(sender());
    if (ac==NULL) return;

    QString dir = ac->data().toString();
    if (dir.isEmpty())
        sort->setFilterRegExp(QRegExp());
    else {
        sort->setFilterKeyColumn(2);
        sort->setFilterFixedString(dir);
    }
}

void CSearchTab::applySnippet(const QItemSelection &selected, const QItemSelection &)
{
    if (selected.isEmpty()) return;
    applySnippet(selected.indexes().first());
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
    setDocTitle(tr("Searching:[%1]").arg(searchTerm));

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

void CSearchTab::searchTerm(const QString &term, bool startSearch)
{
    if (engine->isWorking()) {
        QMessageBox::warning(this,tr("JPReader"),tr("Indexed search engine busy, try later."));
        return;
    }
    ui->editSearch->setEditText(term);

    if (startSearch)
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
    model->deleteAllItems();
    ui->snippetBrowser->clear();
    ui->snippetBrowser->setToolTip(QString());
    selectFile();
    doSearch();
}

void CSearchTab::applySnippet(const QModelIndex &index)
{
    if (stats.isEmpty() || !index.isValid()) return;
    if (model->rowCount()==0) {
        selectFile();
        ui->snippetBrowser->clear();
        ui->snippetBrowser->setToolTip(QString());
    } else {
        int row = sort->mapToSource(index).row();
        if ((row < 0) || (row >= model->rowCount())) return;
        QStrHash snip = model->getSnippet(row);
        QString s = snip["Snip"];
        QString tranState = bool2str(gSet->actionSnippetAutotranslate->isChecked());
        if (s.isEmpty() || tranState!=snip.value("SnipTran")) {
            s = createSpecSnippet(snip["FullFilename"]);
            snip["Snip"]=s;
            snip["SnipUntran"]=createSpecSnippet(snip["FullFilename"],true);
            snip["SnipTran"]=bool2str(gSet->actionSnippetAutotranslate->isChecked());
            model->setSnippet(row, snip);
        }
        s="<font size='+1'>"+s+"</font>";
        ui->snippetBrowser->setHtml(s);
        if (gSet->actionSnippetAutotranslate->isChecked() &&
            snip.value("Snip")!=snip.value("SnipUntran"))
            ui->snippetBrowser->setToolTip(snip.value("SnipUntran"));

        selectFile(snip["Uri"], snip["DisplayFilename"]);
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
            qCritical() << tr("Unable to initialize translation engine.");
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
            QString snpColor = gSet->settings.snippetColors[i % gSet->settings.snippetColors.count()].name();
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

void CSearchTab::execSnippet(const QModelIndex &index)
{
    applySnippet(index);
    showSnippet();
}

void CSearchTab::keyPressEvent(QKeyEvent *event)
{
    if (event->key()==Qt::Key_Return && model->rowCount()>0) {
        execSnippet(ui->listResults->currentIndex());
    } else
        QWidget::keyPressEvent(event);
}
