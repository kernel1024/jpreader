#include <QMessageBox>
#include <QTextCodec>
#include <QUrl>
#include <QDesktopServices>
#include <QThread>
#include <QScopedPointer>
#include <QRandomGenerator>
#include "searchtab.h"
#include "ui_searchtab.h"
#include "globalcontrol.h"
#include "snviewer.h"
#include "genericfuncs.h"
#include "translators/abstracttranslator.h"
#include "ui_hashviewer.h"

CSearchTab::CSearchTab(CMainWindow *parent) :
    CSpecTabContainer(parent),
    ui(new Ui::SearchTab)
{
    const int searchCompleterMaxVisibleItemsFrac = 70;

    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose,true);

    lastQuery.clear();
    setTabTitle(tr("Search"));
    selectFile();

    ui->listResults->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->listResults->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->listResults,&QTableView::customContextMenuRequested,
            this,&CSearchTab::snippetMenu);

    ui->translateFrame->hide();

    model = new CSearchModel(this,ui->listResults);
    sort = new CSearchProxyFilterModel(this);
    sort->setSourceModel(model);
    sort->setSortRole(Qt::UserRole + CStructures::cpSortRole);
    ui->listResults->setModel(sort);
    ui->listResults->sortByColumn(1,Qt::DescendingOrder);

    ui->searchBar->hide();

    connect(model, &CSearchModel::itemContentsUpdated, sort, &QSortFilterProxyModel::invalidate);

    QHeaderView *hh = ui->listResults->horizontalHeader();
    if (hh) {
        hh->setToolTip(tr("Right click for advanced commands on results list"));
        hh->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(hh, &QHeaderView::customContextMenuRequested,
                this, &CSearchTab::headerMenu);
    }

    connect(ui->buttonSearch, &QPushButton::clicked, this, &CSearchTab::doNewSearch);
    connect(ui->buttonOpen, &QPushButton::clicked, this, &CSearchTab::showSnippet);
    connect(ui->buttonDir, &QPushButton::clicked, this, &CSearchTab::selectDir);
    connect(ui->buttonTranslateTitles, &QPushButton::clicked, this, &CSearchTab::translateTitles);
    connect(ui->listResults->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &CSearchTab::applySnippet);
    connect(ui->listResults, &QTableView::activated, this, &CSearchTab::execSnippet);
    connect(ui->editSearch->lineEdit(), &QLineEdit::returnPressed, ui->buttonSearch, &QPushButton::click);

    engine.reset(new CIndexerSearch());
    auto *thread = new QThread();
    engine->moveToThread(thread);
    connect(engine.data(),&CIndexerSearch::destroyed,thread,&QThread::quit);
    connect(thread,&QThread::finished,thread,&QThread::deleteLater);
    connect(engine.data(), &CIndexerSearch::searchFinished,
            this, &CSearchTab::searchFinished, Qt::QueuedConnection);
    connect(this, &CSearchTab::startSearch,
            engine.data(), &CIndexerSearch::doSearch, Qt::QueuedConnection);
    connect(engine.data(), &CIndexerSearch::gotResult,
            this, &CSearchTab::gotSearchResult, Qt::QueuedConnection);
    thread->setObjectName(QSL("SearchTabEngine"));
    thread->start();

    titleTran.reset(new CTitlesTranslator());
    thread = new QThread();
    titleTran->moveToThread(thread);
    connect(titleTran.data(),&CTitlesTranslator::destroyed,thread,&QThread::quit);
    connect(thread,&QThread::finished,thread,&QThread::deleteLater);
    connect(titleTran.data(), &CTitlesTranslator::gotTranslation,
            this, &CSearchTab::gotTitleTranslation,Qt::QueuedConnection);
    connect(titleTran.data(), &CTitlesTranslator::updateProgress,
            this, &CSearchTab::updateProgress,Qt::QueuedConnection);
    connect(ui->translateStopBtn, &QPushButton::clicked,
            titleTran.data(), &CTitlesTranslator::stop,Qt::QueuedConnection);
    connect(this, &CSearchTab::translateTitlesSrc,
            titleTran.data(), &CTitlesTranslator::translateTitles,Qt::QueuedConnection);
    thread->setObjectName(QSL("SearchTitleTran"));
    thread->start();

    ui->buttonSearch->setIcon(QIcon::fromTheme(QSL("document-preview")));
    ui->buttonOpen->setIcon(QIcon::fromTheme(QSL("document-open")));
    ui->buttonDir->setIcon(QIcon::fromTheme(QSL("folder-sync")));

    int mv = (searchCompleterMaxVisibleItemsFrac*parentWnd()->height()) /
             (ui->editSearch->fontMetrics().height()*100);
    ui->editSearch->setMaxVisibleItems(mv);
    updateQueryHistory();

    bindToTab(parentWnd()->tabMain);

    if (engine->getCurrentIndexerService()==CStructures::seRecoll) {
        ui->labelMode->setText(QSL("Recoll search"));
    } else if (engine->getCurrentIndexerService()==CStructures::seBaloo5) {
        ui->labelMode->setText(QSL("Baloo KF5 search"));
    } else {
        ui->labelMode->setText(QSL("Local file search"));
    }

    if (!engine->isValidConfig()) {
        QMessageBox::warning(parentWnd(),QGuiApplication::applicationDisplayName(),
                             tr("Configuration error. \n"
                                "You have enabled some search engine in settings, \n"
                                "but jpreader compiled without support for this engine.\n"
                                "Fallback to local file search.\n"
                                "Please, check program settings and reopen new search tab."));
    }
}

CSearchTab::~CSearchTab()
{
    delete ui;
}

void CSearchTab::updateQueryHistory()
{
    QString t = ui->editSearch->currentText();
    ui->editSearch->clear();
    ui->editSearch->addItems(gSet->searchHistory());
    ui->editSearch->setEditText(t);
}

void CSearchTab::selectDir()
{
    QString dir = CGenericFuncs::getExistingDirectoryD(parentWnd(),tr("Search in"),
                                                       gSet->settings()->savedAuxDir);
    if (!dir.isEmpty()) ui->editDir->setText(dir);
}

void CSearchTab::gotSearchResult(const CStringHash& item)
{
    model->addItem(item);
}

void CSearchTab::searchFinished(const CStringHash &stats, const QString& query)
{
    Q_UNUSED(query)

    const int maxColumnWidth = 400;

    ui->buttonSearch->setEnabled(true);
    ui->searchBar->hide();
    ui->snippetBrowser->clear();
    ui->snippetBrowser->setToolTip(QString());

    if (model->rowCount() == 0) {
        QMessageBox::information(window(), QGuiApplication::applicationDisplayName(),
                                 tr("Nothing found"));
        parentWnd()->setSearchStatus(tr("Ready"));
        return;
    }

    titleTran->stop();

    ui->listResults->resizeColumnsToContents();
    for (int i=0;i<model->columnCount(QModelIndex());i++) {
        if (ui->listResults->columnWidth(i)>maxColumnWidth)
            ui->listResults->setColumnWidth(i,maxColumnWidth);
    }

    QString elapsed = QString(stats[QSL("jp:elapsedtime")])
                      .remove(QRegularExpression(QSL("[s]")));
    QString statusmsg(tr("Found %1 results at %2 seconds").
            arg(stats[QSL("jp:totalhits")],elapsed));
    ui->labelStatus->setText(statusmsg);
    parentWnd()->setSearchStatus(tr("Ready"));
    parentWnd()->updateTitle();
    parentWnd()->updateTabs();

    setTabTitle(tr("S:[%1]").arg(lastQuery));
}

void CSearchTab::translateTitles()
{
    if (model->rowCount()==0) return;

    QStringList titles;
    titles.reserve(model->rowCount());
    for (int i=0;i<model->rowCount();i++)
        titles << model->getSnippet(i).value(QSL("title"));

    Q_EMIT translateTitlesSrc(titles);
}

void CSearchTab::gotTitleTranslation(const QStringList &res)
{
    if (!res.isEmpty() && res.last().contains(QSL("ERROR"))) {
        QMessageBox::warning(this,QGuiApplication::applicationDisplayName(),
                             tr("Title translation failed."));
        return;
    }

    if (res.isEmpty() || model->rowCount()==0 || res.count()!=model->rowCount()) return;

    for (int i=0;i<res.count();i++) {
        CStringHash snip = model->getSnippet(i);
        if (!snip.contains(QSL("title:saved")))
            snip[QSL("title:saved")]=snip[QSL("title")];
        snip[QSL("title")]=res.at(i);
        model->setSnippet(i,snip);
    }
}

void CSearchTab::updateProgress(int pos)
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
    auto *hh = qobject_cast<QHeaderView *>(sender());
    if (hh==nullptr) return;

    int column = hh->logicalIndexAt(pos);

    QMenu cm;

    if (column == 2) { // Directory filter
        QStringList sl = model->getDistinctValues(QSL("jp:dir"));
        if (!sl.isEmpty()) {
            QMenu *cmf = cm.addMenu(tr("Filter"));
            cmf->setStyleSheet(QSL("QMenu { menu-scrollable: 1; }"));
            QAction *ac = cmf->addAction(tr("Clear filter"),this,&CSearchTab::applyFilter);
            ac->setData(QString());
            cmf->addSeparator();
            for (int i=0;i<sl.count();i++) {
                ac = cmf->addAction(sl.at(i),this,&CSearchTab::applyFilter);
                ac->setData(sl.at(i));
            }
            cm.addSeparator();
        }
    }
    cm.addAction(tr("Translate titles"),this,&CSearchTab::translateTitles);

    cm.exec(hh->mapToGlobal(pos));
}

void CSearchTab::snippetMenu(const QPoint &pos)
{
    if (model->rowCount()==0) return;
    const CStringHash sh = model->getSnippet(sort->mapToSource(ui->listResults->indexAt(pos)));
    if (sh.isEmpty()) return;

    const QFileInfo fi(sh[QSL("jp:fullfilename")]);

    QMenu cm;

    if (fi.isFile() && fi.exists()) {
        cm.addAction(QIcon::fromTheme(QSL("fork")),tr("Open with default DE action"),[fi](){
            QDesktopServices::openUrl(QUrl::fromLocalFile(fi.filePath()));
        });
        cm.addAction(QIcon::fromTheme(QSL("document-open-folder")),tr("Open containing directory"),[fi](){
            QDesktopServices::openUrl(QUrl::fromLocalFile(fi.path()));
        });
        cm.addSeparator();
    }
    cm.addAction(QIcon::fromTheme(QSL("document-properties")),tr("Show indexer data..."),[this,sh](){
        QDialog dlg(this);
        Ui::HashViewerDialog ui;
        ui.setupUi(&dlg);
        dlg.setWindowTitle(tr("Indexer data"));
        ui.label->setText(tr("<b>Title:</b> %1")
                           .arg(CGenericFuncs::elideString(sh[QSL("title")],CDefaults::maxTitleElideLength)));
        ui.table->clear();
        ui.table->setColumnCount(2);
        ui.table->setRowCount(sh.size());
        ui.table->setHorizontalHeaderLabels(QStringList() << tr("Property") << tr("Value"));

        int row = 0;
        for (auto it = sh.constKeyValueBegin(), end = sh.constKeyValueEnd(); it != end; ++it, ++row) {
            ui.table->setItem(row,0,new QTableWidgetItem((*it).first));
            ui.table->setItem(row,1,new QTableWidgetItem((*it).second));
        }
        ui.table->resizeColumnsToContents();

        dlg.exec();
    });

    cm.exec(ui->listResults->mapToGlobal(pos));
}

void CSearchTab::applyFilter()
{
    auto *ac = qobject_cast<QAction *>(sender());
    if (ac==nullptr) return;

    sort->setFilter(ac->data().toString());
}

void CSearchTab::applySnippet(const QItemSelection &selected, const QItemSelection &deselected)
{
    Q_UNUSED(deselected)

    if (selected.isEmpty()) return;
    applySnippetIdx(selected.indexes().first());
}

void CSearchTab::doSearch()
{
    if (engine->isWorking()) {
        QMessageBox::information(window(),QGuiApplication::applicationDisplayName(),
                                 tr("Indexed search engine busy. Try later."));
        return;
    }

    parentWnd()->setSearchStatus(tr("Search in progress..."));
    ui->buttonSearch->setEnabled(false);
    ui->searchBar->show();

    lastQuery = ui->editSearch->currentText();

    setTabTitle(tr("Searching:[%1]").arg(lastQuery));

    int idx = ui->editSearch->findText(lastQuery,Qt::MatchFixedString);
    if (idx!=-1)
        ui->editSearch->removeItem(idx);
    ui->editSearch->insertItem(0,lastQuery);

    ui->editSearch->setCurrentIndex(0);

    QDir fsdir = QDir(QSL("/"));
    if (!ui->editDir->text().isEmpty())
        fsdir = QDir(ui->editDir->text());
    Q_EMIT startSearch(lastQuery,fsdir);
}

void CSearchTab::searchTerm(const QString &term, bool startSearch)
{
    if (engine->isWorking()) {
        QMessageBox::warning(this,QGuiApplication::applicationDisplayName(),
                             tr("Indexed search engine busy, try later."));
        return;
    }
    ui->editSearch->setEditText(term);

    if (startSearch)
        doNewSearch();
}

void CSearchTab::doNewSearch()
{
    if (engine->isWorking()) {
        QMessageBox::information(window(),QGuiApplication::applicationDisplayName(),
                                 tr("Indexed search engine busy. Try later."));
        return;
    }

    model->deleteAllItems();
    ui->snippetBrowser->clear();
    ui->snippetBrowser->setToolTip(QString());
    selectFile();
    doSearch();
}

void CSearchTab::applySnippetIdx(const QModelIndex &index)
{
    if (!index.isValid()) return;
    if (model->rowCount()==0) {
        selectFile();
        ui->snippetBrowser->clear();
        ui->snippetBrowser->setToolTip(QString());
    } else {
        int row = sort->mapToSource(index).row();
        if ((row < 0) || (row >= model->rowCount())) return;
        CStringHash snip = model->getSnippet(row);
        QString s = snip[QSL("abstract")];
        QString tranState = CGenericFuncs::bool2str(gSet->ui()->actionSnippetAutotranslate->isChecked());
        if (s.isEmpty() || tranState!=snip.value(QSL("abstract:tran"))) {
            snip[QSL("abstract:untran")]=
                    createSpecSnippet(snip[QSL("jp:fullfilename")],true,s);
            snip[QSL("abstract")]=
                    createSpecSnippet(snip[QSL("jp:fullfilename")],false,s);
            s = snip[QSL("abstract")];
            snip[QSL("abstract:tran")] =
                    CGenericFuncs::bool2str(gSet->ui()->actionSnippetAutotranslate->isChecked());
            model->setSnippet(row, snip);
        }
        s=QSL("<font size='+1'>%1</font>").arg(s);
        ui->snippetBrowser->setHtml(s);
        if (gSet->ui()->actionSnippetAutotranslate->isChecked() &&
            snip.value(QSL("abstract"))!=snip.value(QSL("abstract:untran")))
            ui->snippetBrowser->setToolTip(snip.value(QSL("abstract:untran")));

        selectFile(snip[QSL("url")], snip[QSL("jp:fullfilename")]);
    }
}

QString CSearchTab::createSpecSnippet(const QString& aFilename, bool forceUntranslated,
                                      const QString& auxText)
{
    QString s;
    if(lastQuery.isEmpty()) return s;

    QStringList queryTerms = splitQuery(lastQuery);
    if (queryTerms.count()==0) {
        s=tr("Search query is empty.");
        return s;
    }

    if (!auxText.isEmpty())
        s = CGenericFuncs::highlightSnippet(auxText,queryTerms);

    QStringList queryTermsTran = queryTerms;
    QScopedPointer<CAbstractTranslator,QScopedPointerDeleteLater> tran(
                CAbstractTranslator::translatorFactory(this, CLangPair(gSet->ui()->getActiveLangPair())));
    if (gSet->ui()->actionSnippetAutotranslate->isChecked() && !forceUntranslated) {
        if (!tran || !tran->initTran()) {
            qCritical() << tr("Unable to initialize translation engine.");
            QMessageBox::warning(this,QGuiApplication::applicationDisplayName(),
                                 tr("Unable to initialize translation engine."));
        } else {
            if (!auxText.isEmpty()) {
                s = tran->tranString(auxText);
            } else {
                for (int i=0;i<queryTerms.count();i++) {
                    queryTermsTran[i] = tran->tranString(queryTerms[i]);
                }
            }
        }
    }

    if (!auxText.isEmpty())
        return s;

    QFileInfo fi(aFilename);
    if (!fi.exists()) {
        s=tr("File not found in directory.");
        return s;
    }

    QFile f(aFilename);
    if (!f.open(QIODevice::ReadOnly)) {
        s=tr("File not found");
        return s;
    }

    QByteArray fc;
    if (f.size()>CDefaults::maxSearchFileSize) {
        fc = f.read(CDefaults::maxSearchFileSize);
    } else {
        fc = f.readAll();
    }
    f.close();

    QTextCodec *cd = CGenericFuncs::detectEncoding(fc);

    QString fileContents = cd->toUnicode(fc.constData());

    fileContents.remove(QRegularExpression(QSL("<[^>]*>")));
    fileContents.remove('\n');
    fileContents.remove('\r');
    QHash<int,QStringList> snippets;

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
            bool makeTran = (!tran.isNull()) &&
                            gSet->ui()->actionSnippetAutotranslate->isChecked() &&
                            tran->isReady() &&
                            !forceUntranslated;
            if (makeTran)
                fspart = tran->tranString(fspart);
            QString snpColor = CSettings::snippetColors.at(i % CSettings::snippetColors.count()).name();
            if (makeTran) {
                fspart.replace(ITermT,QSL("<font color='%1'>%2</font>")
                                              .arg(snpColor,ITermT),Qt::CaseInsensitive);
            } else {
                fspart.replace(ITerm,QSL("<font color='%1'>%2</font>")
                               .arg(snpColor,ITerm),Qt::CaseInsensitive);
            }
            // add occurence to collection
            snippets[i] << fspart;
        }
    }
    if (tran) {
        if (tran->isReady())
            tran->doneTran();
    }

    // *** Weighted sorting ***
    // calculate term weights
    double fsumWeight = 0.0;
    double fminWeight = 1.0;
    int sumCount = 0;
    QVector<double> fweights;
    fweights.reserve(queryTerms.count());
    for (int i=0;i<queryTerms.count();i++) {
        double wght = 1.0/static_cast<double>(snippets[i].count());
        if (wght < fminWeight) fminWeight = wght;
        fsumWeight += wght;
        fweights << wght;
        sumCount += snippets[i].count();
    }
    // normalization
    int isumWeight = 0;
    QVector<int> iweights;
    iweights.reserve(queryTerms.count());
    for (int i=0;i<queryTerms.count();i++) {
        int wght = static_cast<int>(fweights[i]/fminWeight);
        iweights << wght;
        isumWeight += wght;
    }

    QStringList rdSnippet;
    int maxcnt = sumCount;
    if (maxSnippets<sumCount) maxcnt = maxSnippets;
    for (int i=0;i<maxcnt && isumWeight>0;i++) {
        int rnd = QRandomGenerator::global()->bounded(INT_MAX) % isumWeight;
        int itm = 0;
        for (int j=0;j<queryTerms.count();j++) {
            if (rnd<iweights[j]) {
                itm = j;
                break;
            }
            rnd -= iweights[j];
        }
        if (snippets[itm].count()>0) {
            rdSnippet << snippets[itm].takeFirst();
        } else { // sublist is empty, take other smallest element from collection
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

QStringList CSearchTab::splitQuery(const QString &aQuery) {
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
            } else {
                qmark=true;
            }
        }
        if ((i>0) && (!qmark) &&
                (sltx[i]==' ') && (sltx[i-1]!=' ')) {
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

    new CSnippetViewer(parentWnd(), selectedUri, sl);
}

void CSearchTab::selectFile(const QString& uri, const QString& dispFilename)
{
    if (!uri.isEmpty()) {
        ui->buttonOpen->setEnabled(true);
        selectedUri = uri;
        ui->filenameEdit->setText(dispFilename);
    } else {
        ui->buttonOpen->setEnabled(false);
        selectedUri.clear();
        ui->filenameEdit->clear();
    }
}

void CSearchTab::execSnippet(const QModelIndex &index)
{
    applySnippetIdx(index);
    showSnippet();
}
