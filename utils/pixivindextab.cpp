#include <QSortFilterProxyModel>
#include <QJsonArray>
#include <QMenu>
#include <QThread>
#include <QMessageBox>
#include <QPainter>

#include "pixivindextab.h"
#include "global/control.h"
#include "global/network.h"
#include "global/startup.h"
#include "global/ui.h"
#include "utils/genericfuncs.h"
#include "browser/browser.h"
#include "browser/net.h"
#include "browser-utils/downloadmanager.h"
#include "extractors/abstractextractor.h"
#include "extractors/fanboxextractor.h"
#include "extractors/patreonextractor.h"
#include "manga/mangaviewtab.h"
#include "ui_pixivindextab.h"

namespace CDefaults {
const int pixivSortRole = Qt::UserRole + 1;
const int maxRowCountColumnResize = 25;
const int previewWidthMargin = 25;
const int mangaCoverSize = 120;
const double previewProps = 600.0/400.0;
}

CPixivIndexTab::CPixivIndexTab(QWidget *parent, const QJsonArray &list,
                               CPixivIndexExtractor::ExtractorMode extractorMode,
                               CPixivIndexExtractor::IndexMode indexMode,
                               const QString &indexId, const QUrlQuery &sourceQuery,
                               const QString &extractorFilterDesc, const QUrl &coversOrigin) :
    CSpecTabContainer(parent),
    ui(new Ui::CPixivIndexTab),
    m_indexId(indexId),
    m_coversOrigin(coversOrigin),
    m_sourceQuery(sourceQuery),
    m_indexMode(indexMode),
    m_extractorMode(extractorMode)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose,true);
    setTabTitle(tr("Pixiv list"));

    double coverHeight = static_cast<double>(ui->editDescription->maximumHeight());
    double coverWidth = coverHeight / CDefaults::previewProps;
    ui->labelCover->setMinimumWidth(static_cast<int>(coverWidth));

    if (m_extractorMode == CPixivIndexExtractor::emArtworks) {
        m_table = ui->list;
        ui->table->hide();
        ui->list->setModelColumn(1);
        ui->buttonReport->setEnabled(false);
    } else {
        m_table = ui->table;
        ui->list->hide();
    }

    m_table->setContextMenuPolicy(Qt::CustomContextMenu);

    m_model = new CPixivIndexModel(this,this,list);
    m_proxyModel = new QSortFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setSortRole(CDefaults::pixivSortRole);
    m_proxyModel->setFilterKeyColumn(-1);
    m_table->setModel(m_proxyModel);

    connect(m_table,&QAbstractItemView::activated,this,&CPixivIndexTab::itemActivated);
    connect(m_table->selectionModel(),&QItemSelectionModel::currentRowChanged,
            this,&CPixivIndexTab::tableSelectionChanged);
    connect(m_table,&QAbstractItemView::customContextMenuRequested,this,&CPixivIndexTab::tableContextMenu);
    connect(ui->labelHead,&QLabel::linkActivated,this,&CPixivIndexTab::linkClicked);
    connect(ui->buttonReport,&QPushButton::clicked,this,&CPixivIndexTab::htmlReport);
    connect(ui->buttonSave,&QPushButton::clicked,this,&CPixivIndexTab::saveToFile);
    connect(ui->buttonTranslateTitles,&QPushButton::clicked,this,&CPixivIndexTab::startTitlesAndTagsTranslation);
    connect(ui->buttonReverseSort,&QPushButton::clicked,this,&CPixivIndexTab::sortReverseClicked);
    connect(ui->editDescription,&QTextBrowser::anchorClicked,this,[this](const QUrl& url){
        linkClicked(url.toString());
    });
    connect(ui->editFilter,&QLineEdit::textEdited,this,&CPixivIndexTab::filterChanged);
    connect(gSet->settings(), &CSettings::mangaViewerSettingsUpdated,
            this, &CPixivIndexTab::mangaSettingsChanged, Qt::QueuedConnection);

    connect(m_proxyModel,&QSortFilterProxyModel::layoutChanged,this,&CPixivIndexTab::modelSorted);
    connect(ui->comboSort,&QComboBox::currentIndexChanged,this,&CPixivIndexTab::comboSortChanged);
    connect(m_model,&CPixivIndexModel::coverUpdated,this,&CPixivIndexTab::coverUpdated);

    m_titleTran.reset(new CTitlesTranslator());
    auto *thread = new QThread();
    m_titleTran->moveToThread(thread);
    connect(m_titleTran.data(),&CTitlesTranslator::destroyed,thread,&QThread::quit);
    connect(thread,&QThread::finished,thread,&QThread::deleteLater);
    connect(m_titleTran.data(), &CTitlesTranslator::gotTranslation,
            this, &CPixivIndexTab::gotTitlesAndTagsTranslation,Qt::QueuedConnection);
    connect(m_titleTran.data(), &CTitlesTranslator::updateProgress,
            this, &CPixivIndexTab::updateTranslatorProgress,Qt::QueuedConnection);
    connect(ui->buttonStopTranslator, &QPushButton::clicked,
            m_titleTran.data(), &CTitlesTranslator::stop,Qt::QueuedConnection);
    connect(this, &CPixivIndexTab::translateTitlesAndTags,
            m_titleTran.data(), &CTitlesTranslator::translateTitles,Qt::QueuedConnection);
    thread->setObjectName(QSL("PixivTabTran"));
    thread->start();

    ui->frameTranslator->hide();

    updateWidgets(extractorFilterDesc);
    bindToTab(parentWnd()->tabMain);
}

CPixivIndexTab::~CPixivIndexTab()
{
    delete ui;
}

CPixivIndexTab *CPixivIndexTab::fromJson(QWidget *parentWidget, const QJsonObject &data)
{
    const QJsonArray list = data.value(QSL("list")).toArray();
    const auto indexMode = static_cast<CPixivIndexExtractor::IndexMode>(data.value(QSL("indexMode")).toInt(0));
    const auto extractorMode = static_cast<CPixivIndexExtractor::ExtractorMode>(data.value(QSL("extractorMode")).toInt(0));
    const QString indexId = data.value(QSL("indexId")).toString();
    const QUrlQuery sourceQuery(data.value(QSL("sourceQuery")).toString());
    const QString filterDesc = data.value(QSL("filterDesc")).toString();
    const QUrl coversOrigin(data.value(QSL("coversOrigin")).toString());

    return new CPixivIndexTab(parentWidget,list,extractorMode,indexMode,indexId,sourceQuery,filterDesc,coversOrigin);
}

CPixivIndexExtractor::ExtractorMode CPixivIndexTab::extractorMode() const
{
    return m_extractorMode;
}

QPointer<QAbstractItemView> CPixivIndexTab::table() const
{
    return m_table;
}

const QUrl &CPixivIndexTab::coversOrigin() const
{
    return m_coversOrigin;
}

void CPixivIndexTab::updateWidgets(const QString& extractorFilterDesc)
{
    ui->labelHead->clear();
    ui->labelCount->clear();
    ui->labelCover->clear();
    ui->labelExtractorFilter->setText(extractorFilterDesc);
    ui->editDescription->clear();

    ui->labelCover->setVisible(gSet->settings()->pixivFetchCovers != CStructures::pxfmNone);
    m_model->overrideRowCount(CDefaults::maxRowCountColumnResize);
    ui->table->resizeColumnsToContents();
    m_model->overrideRowCount();

    if (m_extractorMode == CPixivIndexExtractor::emArtworks)
        ui->list->setGridSize(m_model->preferredGridSize(CDefaults::mangaCoverSize));

    if (m_model->isEmpty()) {
        ui->labelHead->setText(tr("Nothing found."));

    } else {
        QString selector;
        switch (m_extractorMode) {
            case CPixivIndexExtractor::emNovels: selector = QSL("novels"); break;
            case CPixivIndexExtractor::emArtworks: selector = QSL("artworks"); break;
        }
        QString header;
        switch (m_indexMode) {
            case CPixivIndexExtractor::imWorkIndex: header.clear(); break;
            case CPixivIndexExtractor::imBookmarksIndex: header = tr("bookmarks"); break;
            case CPixivIndexExtractor::imTagSearchIndex: header = tr("search"); break;
        }
        QString author = m_model->authors().value(m_indexId);
        if (author.isEmpty())
            author = tr("author");
        if (m_indexMode == CPixivIndexExtractor::imTagSearchIndex)
            author = m_indexId;

        m_title = tr("Pixiv: %1").arg(author);
        setTabTitle(m_title);

        if (m_indexMode == CPixivIndexExtractor::imTagSearchIndex) {
            QUrl u(QSL("https://www.pixiv.net/tags/%1/%2").arg(m_indexId,selector));
            u.setQuery(m_sourceQuery);
            header = QSL("Pixiv %1 %2 list for <a href=\"%3\">"
                         "%4</a>.").arg(header,selector,u.toString(QUrl::FullyEncoded),author);
        } else {
            header = QSL("Pixiv %1 %2 list for <a href=\"https://www.pixiv.net/users/%3\">"
                         "%4</a>.").arg(header,selector,m_indexId,author);
        }
        ui->labelHead->setText(header);

        ui->comboSort->blockSignals(true);
        ui->comboSort->clear();
        ui->comboSort->addItems(m_model->basicHeaders());
        ui->comboSort->addItems(m_model->getTags());
        ui->comboSort->blockSignals(false);

        updateCountLabel();
    }
}

void CPixivIndexTab::updateCountLabel()
{
    QString res = tr("Loaded %1 items").arg(m_model->count());
    if (!(ui->editFilter->text().isEmpty()))
        res.append(tr(" (%1 filtered)").arg(m_proxyModel->rowCount()));
    res.append(QSL("."));

    ui->labelCount->setText(res);
}

QStringList CPixivIndexTab::jsonToTags(const QJsonArray &tags) const
{
    QStringList res;
    res.reserve(tags.count());
    for (const auto& t : qAsConst(tags))
        res.append(t.toString());
    return res;
}

void CPixivIndexTab::setCoverLabel(const QModelIndex &index)
{
    ui->labelCover->clear();
    m_currentCoverIndex = index;
    coverUpdated(index,m_model->cover(index));
}

void CPixivIndexTab::coverUpdated(const QModelIndex &index, const QImage& cover)
{
    if (index != m_currentCoverIndex) return;

    QImage img = cover;
    if (img.isNull()) return;
    img = img.scaled(1,ui->editDescription->height(),
                         Qt::AspectRatioMode::KeepAspectRatioByExpanding,
                         Qt::SmoothTransformation);

    ui->labelCover->setPixmap(QPixmap::fromImage(img));
}

void CPixivIndexTab::tableSelectionChanged(const QModelIndex &current, const QModelIndex &previous)
{
    Q_UNUSED(previous)

    ui->editDescription->clear();

    if (!current.isValid()) return;

    auto *proxy = qobject_cast<QAbstractProxyModel *>(m_table->model());
    if (proxy) {
        const QModelIndex idx = proxy->mapToSource(current);
        const QJsonObject item = m_model->item(idx);
        const QStringList tags = jsonToTags(item.value(QSL("tags")).toArray());
        const QDateTime dt = QDateTime::fromString(item.value(QSL("createDate")).toString(),
                                                   Qt::ISODate);
        QString desc;
        if (!tags.isEmpty())
            desc = QSL("<b>Tags:</b> %1.<br/>").arg(tags.join(QSL(" / ")));

        if (extractorMode() == CPixivIndexExtractor::emArtworks) {
            desc.append(tr("<b>Author:</b> <a href=\"https://www.pixiv.net/users/%1\">%2</a><br/>")
                        .arg(item.value(QSL("userId")).toString(),
                             item.value(QSL("userName")).toString()));

            desc.append(tr("<b>Size:</b> %1x%2 px, <b>created at:</b> %3.<br/>")
                        .arg(item.value(QSL("width")).toInt())
                        .arg(item.value(QSL("height")).toInt())
                        .arg(dt.toString(QSL("yyyy/MM/dd hh:mm"))));
        }

        desc.append(QSL("<b>Description:</b> %1.<br/>")
                    .arg(item.value(QSL("description")).toString()));

        ui->editDescription->setHtml(desc);

        if (ui->labelCover->isVisible())
            setCoverLabel(idx);
    }
}

void CPixivIndexTab::itemActivated(const QModelIndex &index)
{
    if (!index.isValid()) return;

    auto *proxy = qobject_cast<QAbstractProxyModel *>(m_table->model());
    if (proxy) {
        const QString id = m_model->item(proxy->mapToSource(index))
                           .value(QSL("id")).toString();
        if (!id.isEmpty()) {
            if (m_extractorMode == CPixivIndexExtractor::emNovels) {
                new CBrowserTab(gSet->activeWindow(),QUrl(
                                    QSL("https://www.pixiv.net/novel/show.php?id=%1").arg(id)));
            } else {
                new CBrowserTab(gSet->activeWindow(),QUrl(
                                    QSL("https://www.pixiv.net/artworks/%1").arg(id)));
            }
        }
    }
}

void CPixivIndexTab::tableContextMenu(const QPoint &pos)
{

    QModelIndex idx = m_table->indexAt(pos);
    if (!idx.isValid()) return;

    auto *proxy = qobject_cast<QAbstractProxyModel *>(m_table->model());
    if (proxy == nullptr) return;
    idx = proxy->mapToSource(idx);

    const bool artworksMode = (extractorMode() == CPixivIndexExtractor::emArtworks);
    const QString text = m_model->text(idx);
    const QJsonObject item = m_model->item(idx);
    const QString id = item.value(QSL("id")).toString();
    const QString user = item.value(QSL("userId")).toString();
    const QString userName = item.value(QSL("userName")).toString();
    const QString series = item.value(QSL("seriesId")).toString();
    const QString title = item.value(QSL("title")).toString();
    const QString tag = m_model->tag(idx);

    QUrl novelUrl;
    QUrl artworkUrl;
    if (!id.isEmpty()) {
        if (artworksMode) {
            artworkUrl = QUrl(QSL("https://www.pixiv.net/artworks/%1").arg(id));
        } else {
            novelUrl = QUrl(QSL("https://www.pixiv.net/novel/show.php?id=%1").arg(id));
        }
    }
    QUrl userUrl;
    if (!user.isEmpty())
        userUrl = QUrl(QSL("https://www.pixiv.net/users/%1").arg(user));
    QUrl seriesUrl;
    if (!series.isEmpty()) {
        if (artworksMode && !user.isEmpty()) { // Just in case, do not make separate series queries
            seriesUrl = QUrl(QSL("https://www.pixiv.net/user/%1/series/%2").arg(user,series));
        } else {
            seriesUrl = QUrl(QSL("https://www.pixiv.net/novel/series/%1").arg(series));
        }
    }
    QUrl tagUrl;
    if (!tag.isEmpty()) {
        if (artworksMode) {
            tagUrl = QUrl(QSL("https://www.pixiv.net/tags/%1/artworks").arg(tag));
        } else {
            tagUrl = QUrl(QSL("https://www.pixiv.net/tags/%1/novels").arg(tag));
        }
    }

    QMenu cm;
    if (!novelUrl.isEmpty()) {
        cm.addAction(tr("Open novel in new browser tab"),gSet,[novelUrl](){
            new CBrowserTab(gSet->activeWindow(),novelUrl);
        });
    }
    if (!artworkUrl.isEmpty()) {
        cm.addAction(tr("Open artwork in new browser tab"),gSet,[artworkUrl](){
            new CBrowserTab(gSet->activeWindow(),artworkUrl);
        });
    }
    if (!seriesUrl.isEmpty()) {
        cm.addAction(tr("Open series list in new browser tab"),gSet,[seriesUrl](){
            new CBrowserTab(gSet->activeWindow(),seriesUrl);
        });
    }

    auto extractorsList = CAbstractExtractor::addMenuActions(novelUrl,QUrl(),title,&cm,this,true);
    for (auto *eac : qAsConst(extractorsList))
        connect(eac,&QAction::triggered,this,&CPixivIndexTab::processExtractorAction);
    if (!extractorsList.isEmpty()) {
        if (!cm.isEmpty())
            cm.addSeparator();
        cm.addActions(extractorsList);
    }

    extractorsList = CAbstractExtractor::addMenuActions(artworkUrl,QUrl(),title,&cm,this,true);
    for (auto *eac : qAsConst(extractorsList))
        connect(eac,&QAction::triggered,this,&CPixivIndexTab::processExtractorAction);
    if (!extractorsList.isEmpty()) {
        if (!cm.isEmpty())
            cm.addSeparator();
        cm.addActions(extractorsList);
    }

    extractorsList = CAbstractExtractor::addMenuActions(userUrl,QUrl(),title,&cm,this,true);
    for (auto *eac : qAsConst(extractorsList))
        connect(eac,&QAction::triggered,this,&CPixivIndexTab::processExtractorAction);
    if (!extractorsList.isEmpty()) {
        if (!cm.isEmpty()) {
            auto *ac = cm.addSeparator();
            if (!userName.isEmpty())
                ac->setText(tr("Author: %1").arg(userName));
        }
        cm.addActions(extractorsList);
    }

    extractorsList = CAbstractExtractor::addMenuActions(tagUrl,QUrl(),title,&cm,this,true);
    for (auto *eac : qAsConst(extractorsList))
        connect(eac,&QAction::triggered,this,&CPixivIndexTab::processExtractorAction);
    if (!extractorsList.isEmpty()) {
        if (!cm.isEmpty()) {
            auto *ac = cm.addSeparator();
            if (!tag.isEmpty())
                ac->setText(tr("Tag: %1").arg(tag));
        }
        cm.addActions(extractorsList);
    }

    if (!text.isEmpty()) {
        if (!cm.isEmpty())
            cm.addSeparator();
        cm.addAction(QIcon::fromTheme(QSL("edit-copy")),tr("Copy \"%1\" to clipboard").arg(text),this,[text](){
            QClipboard *cb = QApplication::clipboard();
            cb->setText(text);
        });
    }

    if (cm.isEmpty()) return;

    cm.exec(m_table->mapToGlobal(pos));
}

void CPixivIndexTab::htmlReport()
{
    QString html;
    CStringHash authors;

    auto *proxy = qobject_cast<QAbstractProxyModel *>(m_table->model());
    if (proxy == nullptr) return;

    if (m_model->isEmpty()) {
        html = tr("Nothing found.");

    } else {
        for (int row=0; row<proxy->rowCount(); row++) {
            const QJsonObject w = m_model->item(proxy->mapToSource(proxy->index(row,0)));
            const QStringList tags = jsonToTags(w.value(QSL("tags")).toArray());

            html.append(makeNovelInfoBlock(&authors,
                                           w.value(QSL("id")).toString(),
                                           w.value(QSL("url")).toString(),
                                           w.value(QSL("title")).toString(),
                                           w.value(QSL("textCount")).toInt(),
                                           w.value(QSL("userName")).toString(),
                                           w.value(QSL("userId")).toString(),
                                           tags,
                                           w.value(QSL("description")).toString(),
                                           QDateTime::fromString(w.value(QSL("createDate")).toString(),
                                                                 Qt::ISODate),
                                           w.value(QSL("seriesTitle")).toString(),
                                           w.value(QSL("seriesId")).toString(),
                                           w.value(QSL("bookmarkCount")).toInt()));
        }
        html.append(tr("Found %1 novels.").arg(proxy->rowCount()));
    }

    QString header;
    switch (m_indexMode) {
        case CPixivIndexExtractor::imWorkIndex: header = tr("works"); break;
        case CPixivIndexExtractor::imBookmarksIndex: header = tr("bookmarks"); break;
        case CPixivIndexExtractor::imTagSearchIndex: header = tr("search"); break;
    }
    QString author = authors.value(m_indexId);
    if (author.isEmpty())
        author = tr("author");
    if (m_indexMode == CPixivIndexExtractor::imTagSearchIndex)
        author = m_indexId;

    if (m_indexMode == CPixivIndexExtractor::imTagSearchIndex) {
        QUrl u(QSL("https://www.pixiv.net/tags/%1/novels").arg(m_indexId));
        u.setQuery(m_sourceQuery);
        header = QSL("Pixiv %1 list for <a href=\"%2\">"
                     "%3</a>.").arg(header,u.toString(QUrl::FullyEncoded),author);
    } else {
        header = QSL("<h3>Pixiv %1 list for <a href=\"https://www.pixiv.net/users/%2\">"
                     "%3</a>.</h3>").arg(header,m_indexId,author);
    }
    html.prepend(header);

    html = CGenericFuncs::makeSimpleHtml(m_title,html);

    new CBrowserTab(gSet->activeWindow(),QUrl(),QStringList(),html);
}

void CPixivIndexTab::saveToFile()
{
    QString fileName = CGenericFuncs::getSaveFileNameD(this,tr("Save list to file"),gSet->settings()->savedAuxSaveDir,
                                                    QStringList( { tr("Json Pixiv List (*.jspix)") } ));
    if (fileName.isEmpty()) return;
    gSet->ui()->setSavedAuxSaveDir(QFileInfo(fileName).absolutePath());

    QJsonObject root;
    root.insert(QSL("list"),m_model->toJsonArray());
    root.insert(QSL("indexMode"),static_cast<int>(m_indexMode));
    root.insert(QSL("extractorMode"),static_cast<int>(m_extractorMode));
    root.insert(QSL("indexId"),m_indexId);
    root.insert(QSL("sourceQuery"),m_sourceQuery.toString());
    root.insert(QSL("filterDesc"),ui->labelExtractorFilter->text());
    root.insert(QSL("coversOrigin"),m_coversOrigin.toString());
    QJsonDocument doc(root);

    QFile f(fileName);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this,QGuiApplication::applicationDisplayName(),
                              tr("Unable to create file %1").arg(fileName));
        return;
    }
    f.write(doc.toJson(QJsonDocument::Indented));
    f.close();
}

QString CPixivIndexTab::makeNovelInfoBlock(CStringHash *authors,
                                           const QString& workId, const QString& workImgUrl,
                                           const QString& title, int length,
                                           const QString& author, const QString& authorId,
                                           const QStringList& tags, const QString& description,
                                           const QDateTime& creationDate, const QString& seriesTitle,
                                           const QString& seriesId, int bookmarkCount)
{
    QLocale locale;
    QString res;

    res.append(QSL("<div style='border-top:1px black solid;padding-top:10px;overflow:auto;'>"));
    res.append(QSL("<div style='width:120px;float:left;margin-right:20px;'>"));
    res.append(QSL("<a href=\"https://www.pixiv.net/novel/show.php?"
                                 "id=%1\"><img src=\"%2\" style='width:120px;'/></a>")
                  .arg(workId,workImgUrl));
    res.append(QSL("</div><div style='float:none;'>"));

    res.append(QSL("<b>Title:</b> <a href=\"https://www.pixiv.net/novel/show.php?"
                                 "id=%1\">%2</a>.<br/>")
                  .arg(workId,title));

    if (!seriesTitle.isEmpty() && !seriesId.isEmpty()) {
        res.append(QSL("<b>Series:</b> <a href=\"https://www.pixiv.net/novel/series/%1\">"
                          "%2</a>.<br/>")
                      .arg(seriesId,seriesTitle));
    }

    res.append(QSL("<b>Bookmarked:</b> %1.<br/>")
                  .arg(bookmarkCount));

    res.append(QSL("<b>Size:</b> %1 characters.<br/>")
                  .arg(length));

    if (creationDate.isValid()) {
        res.append(QSL("<b>Date:</b> %1.<br/>")
                      .arg(locale.toString(creationDate,QLocale::LongFormat)));
    }

    res.append(QSL("<b>Author:</b> <a href=\"https://www.pixiv.net/users/%1\">"
                                 "%2</a>.<br/>")
                  .arg(authorId,author));

    if (!tags.isEmpty()) {
        res.append(QSL("<b>Tags:</b> %1.<br/>")
                      .arg(tags.join(QSL(" / "))));
    }

    res.append(QSL("<b>Description:</b> %1.<br/>")
                  .arg(description));

    res.append(QSL("</div></div>\n"));

    (*authors)[authorId] = author;

    return res;
}

void CPixivIndexTab::linkClicked(const QString &link)
{
    new CBrowserTab(gSet->activeWindow(),QUrl(link));
}

void CPixivIndexTab::processExtractorAction()
{
    auto *ac = qobject_cast<QAction*>(sender());
    if (!ac) return;

    auto *ex = CAbstractExtractor::extractorFactory(ac->data().toHash(),this);
    if (ex == nullptr) {
        QMessageBox::critical(this,QGuiApplication::applicationDisplayName(),
                              tr("Failed to initialize extractor."));
        return;
    }
    if (!gSet->startup()->setupThreadedWorker(ex)) {
        delete ex;
        return;
    }

    QStringList searchList;
    if (m_indexMode == CPixivIndexExtractor::imTagSearchIndex)
        searchList = m_indexId.split(QChar(' '),Qt::SkipEmptyParts);

    QMetaObject::invokeMethod(ex,&CAbstractThreadWorker::start,Qt::QueuedConnection);
}

void CPixivIndexTab::startTitlesAndTagsTranslation()
{
    const QStringList sl = m_model->getStringsForTranslation();
    if (sl.isEmpty()) return;

    Q_EMIT translateTitlesAndTags(sl);
}

void CPixivIndexTab::gotTitlesAndTagsTranslation(const QStringList &res)
{
    if (!res.isEmpty() && res.last().contains(QSL("ERROR"))) {
        QMessageBox::warning(this,QGuiApplication::applicationDisplayName(),
                             tr("Title translation failed."));
        return;
    }

    m_model->setStringsFromTranslation(res);
}

void CPixivIndexTab::updateTranslatorProgress(int pos)
{
    if (pos>=0) {
        ui->progressTranslator->setValue(pos);
        if (!ui->frameTranslator->isVisible())
            ui->frameTranslator->show();
    } else {
        ui->frameTranslator->hide();
    }
}

void CPixivIndexTab::modelSorted(const QList<QPersistentModelIndex> &parents, QAbstractItemModel::LayoutChangeHint hint)
{
    Q_UNUSED(parents)
    Q_UNUSED(hint)

    const int idx = m_proxyModel->sortColumn();
    if ((idx>=0) && (idx<ui->comboSort->count())) {
        ui->comboSort->blockSignals(true);
        ui->comboSort->setCurrentIndex(idx);
        ui->comboSort->blockSignals(false);
    }
}

void CPixivIndexTab::comboSortChanged(int index)
{
    if (index<0) return;

    if (m_extractorMode == CPixivIndexExtractor::emNovels) {
        ui->table->sortByColumn(index,m_proxyModel->sortOrder());
    } else {
        m_proxyModel->sort(index,m_proxyModel->sortOrder());
    }
}

void CPixivIndexTab::sortReverseClicked()
{
    Qt::SortOrder order = Qt::AscendingOrder;
    if (m_proxyModel->sortOrder() == Qt::AscendingOrder)
        order = Qt::DescendingOrder;

    int index = ui->comboSort->currentIndex();
    if (m_extractorMode == CPixivIndexExtractor::emNovels) {
        ui->table->sortByColumn(index,order);
    } else {
        m_proxyModel->sort(index,order);
    }
}

void CPixivIndexTab::filterChanged(const QString &filter)
{
    if (m_proxyModel)
        m_proxyModel->setFilterFixedString(filter);

    updateCountLabel();
}

void CPixivIndexTab::mangaSettingsChanged()
{
    if (m_extractorMode == CPixivIndexExtractor::emArtworks) {
        ui->list->update();
    }
}

CPixivIndexModel::CPixivIndexModel(QObject *parent, CPixivIndexTab *tab, const QJsonArray &list)
    : QAbstractTableModel(parent),
      m_list(list),
      m_tab(tab)
{
    
    updateTags();
}

CPixivIndexModel::~CPixivIndexModel() = default;

int CPixivIndexModel::rowCount(const QModelIndex &parent) const
{
    if (!checkIndex(parent))
        return 0;
    if (parent.isValid())
        return 0;
    if (m_maxRowCount >= 0)
        return std::min(m_maxRowCount,static_cast<int>(m_list.count()));

    if (m_tab->extractorMode() == CPixivIndexExtractor::emArtworks) {
        if (m_tab->table()->palette().base().color() != gSet->settings()->mangaBackgroundColor) {
            QPalette pl = m_tab->table()->palette();
            pl.setBrush(QPalette::Base,QBrush(gSet->settings()->mangaBackgroundColor));
            m_tab->table()->setPalette(pl);
        }
    }

    return m_list.count();
}

int CPixivIndexModel::columnCount(const QModelIndex &parent) const
{
    if (!checkIndex(parent))
        return 0;
    if (parent.isValid())
        return 0;

    return basicHeaders().count() + m_tags.count();
}

QVariant CPixivIndexModel::data(const QModelIndex &index, int role) const
{
    if (m_tab.isNull())
        return QVariant();

    if (!checkIndex(index,CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return QVariant();

    const bool artworksMode = (m_tab->extractorMode() == CPixivIndexExtractor::emArtworks);
    const int row = index.row();
    const int col = index.column();
    const auto w = m_list.at(row).toObject();
    const QString translatedTitle = w.value(QSL("translatedTitle")).toString();
    const QString wTitle = (!translatedTitle.isEmpty() ? translatedTitle : w.value(QSL("title")).toString());
    static const QIcon bookmarkedIcon = QIcon::fromTheme(QSL("emblem-favorite"));
    static const QIcon unknownIcon = QIcon::fromTheme(QSL("unknown"));

    if (role == Qt::DisplayRole) {
        switch (col) {
            case 0:
                if (artworksMode) {
                    return wTitle;
                } else {
                    return w.value(QSL("id")).toString();
                }
            case 1: return wTitle;
            case 2: return w.value(QSL("userName")).toString();
            case 3: return QSL("%1").arg(w.value(QSL("textCount")).toInt());
            case 4: {
                const QDateTime dt = QDateTime::fromString(w.value(QSL("createDate")).toString(),
                                                           Qt::ISODate);
                return dt.toString(QSL("yyyy/MM/dd hh:mm"));
            }
            case 5: return QSL("%1").arg(w.value(QSL("bookmarkCount")).toInt());
            case 6: return w.value(QSL("seriesTitle")).toString();
            default: { // tag columns
                const QString tag = getTagForColumn(col);
                if (!tag.isEmpty()) {
                    const QJsonArray wtags = w.value(QSL("tags")).toArray();
                    if (wtags.contains(QJsonValue(tag)))
                        return tag;
                }
                return QVariant();
            }
        }

    } else if (role == Qt::ToolTipRole) {
        QString tooltip;
        if (col == 1) {
            const QString translatedTitle = w.value(QSL("translatedTitle")).toString();
            if (!translatedTitle.isEmpty())
                tooltip = w.value(QSL("title")).toString();
        } else if (!m_translatedTags.isEmpty()) { // tag columns
            int tagNum = 0;
            const QString tag = getTagForColumn(col,&tagNum);
            if (!tag.isEmpty()) {
                const QJsonArray wtags = w.value(QSL("tags")).toArray();
                if (wtags.contains(QJsonValue(tag)))
                    tooltip = m_translatedTags.at(tagNum);
            }
        }
        if (!tooltip.isEmpty())
            return tooltip;

    } else if (role == Qt::DecorationRole) {
        if ((col == 0) && artworksMode) { // artwork cover
            QSize pixmapSize(CDefaults::mangaCoverSize,CDefaults::mangaCoverSize);
            if (pixmapSize != m_cachedPixmapSize)
                m_cachedCovers.clear();
            m_cachedPixmapSize = pixmapSize;

            QPixmap rp(pixmapSize);
            QPainter cp(&rp);
            cp.fillRect(0,0,rp.width(),rp.height(),gSet->settings()->mangaBackgroundColor);

            if (m_cachedCovers.contains(row)) {
                cp.drawImage(0,0,m_cachedCovers.value(row));
            } else {
                QImage p = cover(index);
                if (p.isNull()) {
                    QPixmap icon = QIcon::fromTheme(QSL("edit-download")).pixmap(pixmapSize);
                    QRect iconRect = icon.rect();
                    iconRect.moveCenter(rp.rect().center());
                    cp.drawPixmap(iconRect.topLeft(),icon);
                } else {
                    p = p.scaled(rp.size(),Qt::KeepAspectRatio,Qt::SmoothTransformation);
                    m_cachedCovers[row] = p;
                    cp.drawImage(0,0,p);
                }
            }
            printCoverInfo(&cp,index);
            return rp;
        }

        if (col == 5) { // bookmarks column
            if (w.value(QSL("bookmarkData")).isNull())
                return unknownIcon;
            return bookmarkedIcon;
        }
    } else if (role == Qt::ForegroundRole && artworksMode) {
        return gSet->ui()->getMangaForegroundColor();
    }

    if (role == CDefaults::pixivSortRole) {
        switch (col) {
            case 0: return w.value(QSL("id")).toString().toInt();
            case 1: return w.value(QSL("title")).toString();
            case 2: return w.value(QSL("userName")).toString();
            case 3: {
                if (artworksMode) {
                    return w.value(QSL("pageCount")).toInt();
                }
                return w.value(QSL("textCount")).toInt();
            }
            case 4: return QDateTime::fromString(w.value(QSL("createDate")).toString(),
                                                 Qt::ISODate);
            case 5: return w.value(QSL("bookmarkCount")).toInt();
            case 6: return w.value(QSL("seriesTitle")).toString();
            default: { // tag columns
                const QString tag = getTagForColumn(col);
                if (!tag.isEmpty()) {
                    const QJsonArray wtags = w.value(QSL("tags")).toArray();
                    if (wtags.contains(QJsonValue(tag)))
                        return tag;
                }
                return QVariant();
            }
        }
    }

    return QVariant();
}

void CPixivIndexModel::printCoverInfo(QPainter *painter, const QModelIndex &index) const
{
    if (m_tab.isNull() || (m_tab->extractorMode() != CPixivIndexExtractor::emArtworks) ||
            !checkIndex(index,CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return;

    const QChar heart(0x2665);
    const int textMargin = 4;
    const int textBorder = 3;
    const double roundRadius = 2.0;
    const double bookmarkFontFactor = 1.5;
    const int row = index.row();
    const auto w = m_list.at(row).toObject();
    QString text;
    QRect textRect;

    painter->save();

    QFont f = painter->font();
    f.setWeight(QFont::Bold);
    painter->setFont(f);

    // page count
    text = QSL("%1").arg(w.value(QSL("pageCount")).toInt());

    textRect = painter->fontMetrics().boundingRect(text);
    textRect = textRect.marginsAdded(QMargins(textBorder,textBorder,textBorder,textBorder));
    textRect.moveTopRight(QPoint(painter->device()->width() - textMargin,
                                 textMargin));
    painter->setPen(QColor(Qt::white));
    painter->setBrush(QColor(Qt::darkGray));
    painter->drawRoundedRect(textRect,roundRadius,roundRadius);
    painter->drawText(textRect,Qt::AlignCenter,text);

    // self-bookmark sign
    if (!w.value(QSL("bookmarkData")).isNull()) {
        f.setPointSizeF(bookmarkFontFactor * f.pointSizeF());
        painter->setFont(f);

        text = heart;
        textRect = painter->fontMetrics().boundingRect(text);
        textRect = textRect.marginsAdded(QMargins(textBorder,textBorder,textBorder,textBorder));
        textRect.moveTopLeft(QPoint(textMargin,textMargin));
        painter->setPen(QColor(Qt::red));
        painter->drawText(textRect,Qt::AlignCenter,text);
    }

    painter->restore();
}

Qt::ItemFlags CPixivIndexModel::flags(const QModelIndex &index) const
{
    if (!checkIndex(index,CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return Qt::NoItemFlags;

    return (Qt::ItemIsSelectable | Qt::ItemIsEnabled);
}

QVariant CPixivIndexModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole) {
        if (orientation == Qt::Horizontal) {
            QStringList headers = basicHeaders();

            headers.reserve(m_tags.count());
            for (const auto& tag : qAsConst(m_tags))
                headers.append(QSL("T:[%1]").arg(tag));

            if (section>=0 && section<headers.count())
                return headers.at(section);
        } else {
            return QSL("%1").arg(section+1);
        }
    }
    return QVariant();
}

CStringHash CPixivIndexModel::authors() const
{
    return m_authors;
}

bool CPixivIndexModel::isEmpty() const
{
    return m_list.isEmpty();
}

int CPixivIndexModel::count() const
{
    return m_list.count();
}

QJsonObject CPixivIndexModel::item(const QModelIndex &index) const
{
    if (!checkIndex(index,CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return QJsonObject();

    return m_list.at(index.row()).toObject();
}

QString CPixivIndexModel::tag(const QModelIndex &index) const
{
    if (!checkIndex(index,CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return QString();

    if (index.column()<basicHeaders().count())
        return QString();

    return data(index,Qt::DisplayRole).toString();
}

QString CPixivIndexModel::text(const QModelIndex &index) const
{
    if (!checkIndex(index,CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return QString();

    return data(index,Qt::DisplayRole).toString();
}

QImage CPixivIndexModel::cover(const QModelIndex &index) const
{
    if (!checkIndex(index,CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return QImage();

    static const QString start = QSL("data:image/");
    const QString dataUrl = item(index).value(QSL("url")).toString();

    if (!dataUrl.startsWith(start,Qt::CaseInsensitive)) {
        QUrl url(dataUrl);
        if (((gSet->settings()->pixivFetchCovers == CStructures::pxfmLazyFetch) ||
             m_tab->extractorMode() == CPixivIndexExtractor::emArtworks)
                && url.isValid() && dataUrl.startsWith(QSL("http"),Qt::CaseInsensitive)) {
            (const_cast<CPixivIndexModel *>(this))->fetchCover(index,url);
        }
        return QImage();
    }

    return CGenericFuncs::dataUrlToImage(dataUrl);
}

void CPixivIndexModel::fetchCover(const QModelIndex &index, const QUrl &url)
{
    if (!checkIndex(index,CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return;

    const QString coverImgUrl = url.toString();
    if (coverImgUrl.contains(QSL("common/images"))) {
        const QString data = gSet->net()->getPixivCommonCover(coverImgUrl);
        if (!data.isEmpty()) {
            setCoverImage(index,data);
            return;
        }
    }

    auto *extractor = new CPixivIndexExtractor(this);

    connect(extractor,&CPixivIndexExtractor::auxImgLoadFinished,this,
            [this,extractor,index](const QUrl& origin, const QString& data){
        Q_UNUSED(origin)
        if (!data.isEmpty()) {
            setCoverImage(index,data);
        }
        extractor->deleteLater();
    });

    QMetaObject::invokeMethod(gSet->auxNetworkAccessManager(),[this,url,extractor]{
        QNetworkRequest req(url);
        req.setRawHeader("referer",m_tab->coversOrigin().toString().toUtf8());
        QNetworkReply *rplImg = gSet->net()->auxNetworkAccessManagerGet(req);
        connect(rplImg,&QNetworkReply::finished,extractor,&CPixivIndexExtractor::subImageFinished);
    },Qt::QueuedConnection);
}

void CPixivIndexModel::setCoverImage(const QModelIndex &idx, const QString &data)
{
    if (!checkIndex(idx,CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return;

    int row = idx.row();
    QJsonObject obj = m_list.at(row).toObject();
    obj.insert(QSL("url"),QJsonValue(data));
    m_list[row] = obj;

    Q_EMIT dataChanged(index(row,0), index(row,0), { Qt::DecorationRole });
    Q_EMIT coverUpdated(idx,CGenericFuncs::dataUrlToImage(data));
}

QSize CPixivIndexModel::preferredGridSize(int iconSize) const
{
    QFont f = QApplication::font();
    if (!m_tab.isNull() && !m_tab->table().isNull())
        f = m_tab->table()->font();
    QFontMetrics fm(f);
    return QSize(iconSize + CDefaults::previewWidthMargin,
                 iconSize + fm.height() * 4);
}

QStringList CPixivIndexModel::getStringsForTranslation() const
{
    QStringList res;
    res.reserve(m_list.count() + m_tags.count());
    for (const auto &w : qAsConst(m_list))
        res.append(w.toObject().value(QSL("title")).toString());
    res.append(m_tags);
    return res;
}

void CPixivIndexModel::setStringsFromTranslation(const QStringList &translated)
{
    if (translated.isEmpty()
            || ((m_list.count() + m_tags.count()) != translated.count())) return;

    for (int i=0;i<m_list.count();i++) {
        QJsonObject obj = m_list.at(i).toObject();
        obj.insert(QSL("translatedTitle"),translated.at(i));
        m_list[i] = obj;
    }

    m_translatedTags.append(translated.mid(m_list.count()));

    Q_EMIT dataChanged(index(0,1),index(m_list.count(),1),{ Qt::ToolTipRole, Qt::DisplayRole });
    Q_EMIT dataChanged(index(0,basicHeaders().count()),
                       index(m_list.count(),basicHeaders().count() + m_tags.count() - 1),
                       { Qt::ToolTipRole });
}

QStringList CPixivIndexModel::basicHeaders() const
{
    static const QStringList res({ tr("ID"), tr("Title"), tr("Author"), tr("Size"), tr("Date"),
                                   tr("Bookmarked"), tr("Series") });
    return res;
}

QStringList CPixivIndexModel::getTags() const
{
    return m_tags;
}

QString CPixivIndexModel::getTagForColumn(int column, int *tagNumber) const
{
    int tagNum = column - basicHeaders().count();

    if (tagNumber != nullptr)
        *tagNumber = tagNum;

    if (tagNum>=0 && tagNum<m_tags.count())
        return m_tags.at(tagNum);

    return QString();
}

int CPixivIndexModel::getColumnForTag(const QString &tag) const
{
    int col = m_tags.indexOf(tag);
    if (col<0) return col;

    return (col + basicHeaders().count());
}

QJsonArray CPixivIndexModel::toJsonArray() const
{
    return m_list;
}

void CPixivIndexModel::overrideRowCount(int maxRowCount)
{
    m_maxRowCount = maxRowCount;
}

void CPixivIndexModel::updateTags()
{
    m_tags.clear();
    m_authors.clear();
    m_translatedTags.clear();

    for (const auto &w : qAsConst(m_list)) {
        const QJsonObject obj = w.toObject();
        const QJsonArray wtags = obj.value(QSL("tags")).toArray();
        for (const auto& t : qAsConst(wtags)) {
            const QString tag = t.toString();
            if (!m_tags.contains(tag))
                m_tags.append(tag);
        }

        const QString authorId = obj.value(QSL("userId")).toString();
        m_authors[authorId] = obj.value(QSL("userName")).toString();
    }
    std::sort(m_tags.begin(), m_tags.end());
}
