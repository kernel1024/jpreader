#include <QSortFilterProxyModel>
#include <QJsonArray>
#include <QMenu>
#include <QThread>
#include <QMessageBox>

#include "pixivindextab.h"
#include "global/control.h"
#include "global/network.h"
#include "global/startup.h"
#include "global/ui.h"
#include "utils/genericfuncs.h"
#include "browser/browser.h"
#include "extractors/abstractextractor.h"
#include "ui_pixivindextab.h"

namespace CDefaults {
const int pixivSortRole = Qt::UserRole + 1;
const int maxRowCountColumnResize = 25;
const double previewProps = 600.0/400.0;
const auto coverLabelDataUrl = "dataUrl";
}

CPixivIndexTab::CPixivIndexTab(QWidget *parent, const QJsonArray &list,
                               CPixivIndexExtractor::IndexMode indexMode,
                               const QString &indexId, const QUrlQuery &sourceQuery,
                               const QString &extractorFilterDesc, const QUrl &coversOrigin) :
    CSpecTabContainer(parent),
    ui(new Ui::CPixivIndexTab),
    m_indexId(indexId),
    m_coversOrigin(coversOrigin),
    m_sourceQuery(sourceQuery),
    m_indexMode(indexMode)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose,true);
    setTabTitle(tr("Pixiv list"));

    double coverHeight = static_cast<double>(ui->editDescription->maximumHeight());
    double coverWidth = coverHeight / CDefaults::previewProps;
    ui->labelCover->setMinimumWidth(static_cast<int>(coverWidth));

    ui->table->setContextMenuPolicy(Qt::CustomContextMenu);

    m_model = new CPixivTableModel(this,list);
    m_proxyModel = new QSortFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setSortRole(CDefaults::pixivSortRole);
    m_proxyModel->setFilterKeyColumn(-1);
    ui->table->setModel(m_proxyModel);

    connect(ui->table,&QTableView::activated,this,&CPixivIndexTab::novelActivated);
    connect(ui->table->selectionModel(),&QItemSelectionModel::currentRowChanged,
            this,&CPixivIndexTab::tableSelectionChanged);
    connect(ui->table,&QTableView::customContextMenuRequested,this,&CPixivIndexTab::tableContextMenu);
    connect(ui->labelHead,&QLabel::linkActivated,this,&CPixivIndexTab::linkClicked);
    connect(ui->buttonReport,&QPushButton::clicked,this,&CPixivIndexTab::htmlReport);
    connect(ui->buttonSave,&QPushButton::clicked,this,&CPixivIndexTab::saveToFile);
    connect(ui->buttonTranslateTitles,&QPushButton::clicked,this,&CPixivIndexTab::startTitlesAndTagsTranslation);
    connect(ui->editDescription,&QTextBrowser::anchorClicked,this,[this](const QUrl& url){
        linkClicked(url.toString());
    });
    connect(ui->editFilter,&QLineEdit::textEdited,this,&CPixivIndexTab::filterChanged);

    connect(m_proxyModel,&QSortFilterProxyModel::layoutChanged,this,&CPixivIndexTab::modelSorted);
    connect(ui->comboSort,&QComboBox::currentIndexChanged,this,&CPixivIndexTab::comboSortChanged);

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
    const QString indexId = data.value(QSL("indexId")).toString();
    const QUrlQuery sourceQuery(data.value(QSL("sourceQuery")).toString());
    const QString filterDesc = data.value(QSL("filterDesc")).toString();
    const QUrl coversOrigin(data.value(QSL("coversOrigin")).toString());

    return new CPixivIndexTab(parentWidget,list,indexMode,indexId,sourceQuery,filterDesc,coversOrigin);
}

QString CPixivIndexTab::title() const
{
    return m_title;
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

    if (m_model->isEmpty()) {
        ui->labelHead->setText(tr("Nothing found."));

    } else {
        QString header;
        switch (m_indexMode) {
            case CPixivIndexExtractor::imWorkIndex: header = tr("works"); break;
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
            QUrl u(QSL("https://www.pixiv.net/tags/%1/novels").arg(m_indexId));
            u.setQuery(m_sourceQuery);
            header = QSL("Pixiv %1 list for <a href=\"%2\">"
                         "%3</a>.").arg(header,u.toString(QUrl::FullyEncoded),author);
        } else {
            header = QSL("Pixiv %1 list for <a href=\"https://www.pixiv.net/users/%2\">"
                         "%3</a>.").arg(header,m_indexId,author);
        }
        ui->labelHead->setText(header);

        ui->comboSort->blockSignals(true);
        ui->comboSort->addItems(m_model->getTags());
        ui->comboSort->blockSignals(false);

        updateCountLabel();
    }
}

void CPixivIndexTab::updateCountLabel()
{
    QString res = tr("Loaded %1 novels").arg(m_model->count());
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
    static const QString b64marker = QSL(";base64,");
    static const QString start = QSL("data:image/");

    const QString dataUrl = m_model->item(index).value(QSL("url")).toString();

    ui->labelCover->clear();
    ui->labelCover->setProperty(CDefaults::coverLabelDataUrl,dataUrl);

    if (!dataUrl.startsWith(start,Qt::CaseInsensitive)) {
        QUrl url(dataUrl);
        if ((gSet->settings()->pixivFetchCovers == CStructures::pxfmLazyFetch)
                && url.isValid() && dataUrl.startsWith(QSL("http"),Qt::CaseInsensitive)) {
            fetchCoverLabel(index,url);
        }
        return;
    }

    int b64markerPos = dataUrl.indexOf(b64marker,Qt::CaseInsensitive);
    if (b64markerPos < 0) return;
    const QString base64 = dataUrl.mid(b64markerPos + b64marker.length());

    auto result = QByteArray::fromBase64Encoding(base64.toLatin1());
    if (result.decodingStatus != QByteArray::Base64DecodingStatus::Ok) return;

    QImage cover = QImage::fromData(result.decoded);
    if (cover.isNull()) return;
    cover = cover.scaled(1,ui->editDescription->height(),
                         Qt::AspectRatioMode::KeepAspectRatioByExpanding,
                         Qt::SmoothTransformation);

    ui->labelCover->setPixmap(QPixmap::fromImage(cover));
}

void CPixivIndexTab::fetchCoverLabel(const QModelIndex &index, const QUrl &url)
{
    const QString coverImgUrl = url.toString();
    if (coverImgUrl.contains(QSL("common/images"))) {
        const QString data = gSet->net()->getPixivCommonCover(coverImgUrl);
        if (!data.isEmpty()) {
            m_model->setCoverImageForUrl(coverImgUrl,data);
            setCoverLabel(index);
            return;
        }
    }

    auto *extractor = new CPixivIndexExtractor(this);

    connect(extractor,&CPixivIndexExtractor::auxImgLoadFinished,this,
            [this,index,extractor](const QUrl& origin, const QString& data){
        // Check if other query is launched while we downloaded cover
        if (origin.toString() == ui->labelCover->property(CDefaults::coverLabelDataUrl).toString()) {
            // Update cover and widget
            if (!origin.isEmpty() && !data.isEmpty()) {
                m_model->setCoverImageForUrl(origin,data);
                setCoverLabel(index);
            }
        }
        extractor->deleteLater();
    });

    QMetaObject::invokeMethod(gSet->auxNetworkAccessManager(),[this,url,extractor]{
        QNetworkRequest req(url);
        req.setRawHeader("referer",m_coversOrigin.toString().toUtf8());
        QNetworkReply *rplImg = gSet->net()->auxNetworkAccessManagerGet(req);
        connect(rplImg,&QNetworkReply::finished,extractor,&CPixivIndexExtractor::subImageFinished);
    },Qt::QueuedConnection);
}

void CPixivIndexTab::tableSelectionChanged(const QModelIndex &current, const QModelIndex &previous)
{
    Q_UNUSED(previous)

    ui->editDescription->clear();

    if (!current.isValid()) return;

    auto *proxy = qobject_cast<QAbstractProxyModel *>(ui->table->model());
    if (proxy) {
        const QModelIndex idx = proxy->mapToSource(current);
        const QJsonObject item = m_model->item(idx);
        const QStringList tags = jsonToTags(item.value(QSL("tags")).toArray());
        QString desc;
        if (!tags.isEmpty())
            desc = QSL("<b>Tags:</b> %1.<br/>").arg(tags.join(QSL(" / ")));

        desc.append(QSL("<b>Description:</b> %1.<br/>")
                    .arg(item.value(QSL("description")).toString()));

        ui->editDescription->setHtml(desc);

        if (ui->labelCover->isVisible())
            setCoverLabel(idx);
    }
}

void CPixivIndexTab::novelActivated(const QModelIndex &index)
{
    if (!index.isValid()) return;

    auto *proxy = qobject_cast<QAbstractProxyModel *>(ui->table->model());
    if (proxy) {
        const QString id = m_model->item(proxy->mapToSource(index))
                           .value(QSL("id")).toString();
        if (!id.isEmpty()) {
            new CBrowserTab(gSet->activeWindow(),QUrl(
                                   QSL("https://www.pixiv.net/novel/show.php?id=%1").arg(id)));
        }
    }
}

void CPixivIndexTab::tableContextMenu(const QPoint &pos)
{
    QModelIndex idx = ui->table->indexAt(pos);
    if (!idx.isValid()) return;

    auto *proxy = qobject_cast<QAbstractProxyModel *>(ui->table->model());
    if (proxy == nullptr) return;
    idx = proxy->mapToSource(idx);

    const QString text = m_model->text(idx);
    const QJsonObject item = m_model->item(idx);
    const QString id = item.value(QSL("id")).toString();
    const QString user = item.value(QSL("userId")).toString();
    const QString userName = item.value(QSL("userName")).toString();
    const QString series = item.value(QSL("seriesId")).toString();
    const QString title = item.value(QSL("title")).toString();
    const QString tag = m_model->tag(idx);

    QUrl novelUrl;
    if (!id.isEmpty())
        novelUrl = QUrl(QSL("https://www.pixiv.net/novel/show.php?id=%1").arg(id));
    QUrl userUrl;
    if (!user.isEmpty())
        userUrl = QUrl(QSL("https://www.pixiv.net/users/%1").arg(user));
    QUrl seriesUrl;
    if (!series.isEmpty())
        seriesUrl = QUrl(QSL("https://www.pixiv.net/novel/series/%1").arg(series));
    QUrl tagUrl;
    if (!tag.isEmpty())
        tagUrl = QUrl(QSL("https://www.pixiv.net/tags/%1/novels").arg(tag));

    QMenu cm;
    if (!novelUrl.isEmpty()) {
        cm.addAction(tr("Open novel in new browser tab"),gSet,[novelUrl](){
            new CBrowserTab(gSet->activeWindow(),novelUrl);
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

    cm.exec(ui->table->mapToGlobal(pos));
}

void CPixivIndexTab::htmlReport()
{
    QString html;
    CStringHash authors;

    auto *proxy = qobject_cast<QAbstractProxyModel *>(ui->table->model());
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

    new CBrowserTab(gSet->activeWindow(),QUrl(),QStringList(),true,html);
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
    connect(ex,&CAbstractExtractor::novelReady,gSet,[searchList]
            (const QString &html, bool focus, bool translate, bool alternateTranslate){
        auto *sv = new CBrowserTab(gSet->activeWindow(),QUrl(),searchList,focus,html);
        sv->setRequestAutotranslate(translate);
        sv->setRequestAlternateAutotranslate(alternateTranslate);
    },Qt::QueuedConnection);

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

    const QString sortTag = m_model->getTagForColumn(m_proxyModel->sortColumn());
    int idx = -1;
    if (!sortTag.isEmpty())
        idx = ui->comboSort->findText(sortTag);

    ui->comboSort->blockSignals(true);
    ui->comboSort->setCurrentIndex(idx);
    ui->comboSort->blockSignals(false);
}

void CPixivIndexTab::comboSortChanged(int index)
{
    if (index<0) return;

    int col = m_model->getColumnForTag(ui->comboSort->currentText());
    if (col<0) return;

    ui->table->sortByColumn(col,m_proxyModel->sortOrder());
}

void CPixivIndexTab::filterChanged(const QString &filter)
{
    if (m_proxyModel)
        m_proxyModel->setFilterFixedString(filter);

    updateCountLabel();
}

CPixivTableModel::CPixivTableModel(QObject *parent, const QJsonArray &list)
    : QAbstractTableModel(parent),
      m_list(list)
{
    updateTags();
}

CPixivTableModel::~CPixivTableModel() = default;

int CPixivTableModel::rowCount(const QModelIndex &parent) const
{
    if (!checkIndex(parent))
        return 0;
    if (parent.isValid())
        return 0;
    if (m_maxRowCount >= 0)
        return std::min(m_maxRowCount,static_cast<int>(m_list.count()));

    return m_list.count();
}

int CPixivTableModel::columnCount(const QModelIndex &parent) const
{
    if (!checkIndex(parent))
        return 0;
    if (parent.isValid())
        return 0;

    return basicHeaders().count() + m_tags.count();
}

QVariant CPixivTableModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index,CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return QVariant();

    const int row = index.row();
    const int col = index.column();
    const auto w = m_list.at(row).toObject();

    if (role == Qt::DisplayRole) {
        switch (col) {
            case 0: return w.value(QSL("id")).toString();
            case 1: {
                const QString translatedTitle = w.value(QSL("translatedTitle")).toString();
                if (!translatedTitle.isEmpty())
                    return translatedTitle;
                return w.value(QSL("title")).toString();
            }
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
    }

    if (role == CDefaults::pixivSortRole) {
        switch (col) {
            case 0: return w.value(QSL("id")).toString().toInt();
            case 1: return w.value(QSL("title")).toString();
            case 2: return w.value(QSL("userName")).toString();
            case 3: return w.value(QSL("textCount")).toInt();
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

Qt::ItemFlags CPixivTableModel::flags(const QModelIndex &index) const
{
    if (!checkIndex(index,CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return Qt::NoItemFlags;

    return (Qt::ItemIsSelectable | Qt::ItemIsEnabled);
}

QVariant CPixivTableModel::headerData(int section, Qt::Orientation orientation, int role) const
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

CStringHash CPixivTableModel::authors() const
{
    return m_authors;
}

bool CPixivTableModel::isEmpty() const
{
    return m_list.isEmpty();
}

int CPixivTableModel::count() const
{
    return m_list.count();
}

QJsonObject CPixivTableModel::item(const QModelIndex &index) const
{
    if (!checkIndex(index,CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return QJsonObject();

    return m_list.at(index.row()).toObject();
}

QString CPixivTableModel::tag(const QModelIndex &index) const
{
    if (!checkIndex(index,CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return QString();

    if (index.column()<basicHeaders().count())
        return QString();

    return data(index,Qt::DisplayRole).toString();
}

QString CPixivTableModel::text(const QModelIndex &index) const
{
    if (!checkIndex(index,CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return QString();

    return data(index,Qt::DisplayRole).toString();
}

QStringList CPixivTableModel::getStringsForTranslation() const
{
    QStringList res;
    res.reserve(m_list.count() + m_tags.count());
    for (const auto &w : qAsConst(m_list))
        res.append(w.toObject().value(QSL("title")).toString());
    res.append(m_tags);
    return res;
}

void CPixivTableModel::setStringsFromTranslation(const QStringList &translated)
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

QStringList CPixivTableModel::basicHeaders() const
{
    static const QStringList res({ tr("ID"), tr("Title"), tr("Author"), tr("Size"), tr("Date"),
                                   tr("Bookmarked"), tr("Series") });
    return res;
}

QStringList CPixivTableModel::getTags() const
{
    return m_tags;
}

QString CPixivTableModel::getTagForColumn(int column, int *tagNumber) const
{
    int tagNum = column - basicHeaders().count();

    if (tagNumber != nullptr)
        *tagNumber = tagNum;

    if (tagNum>=0 && tagNum<m_tags.count())
        return m_tags.at(tagNum);

    return QString();
}

int CPixivTableModel::getColumnForTag(const QString &tag) const
{
    int col = m_tags.indexOf(tag);
    if (col<0) return col;

    return (col + basicHeaders().count());
}

void CPixivTableModel::setCoverImageForUrl(const QUrl &url, const QString &data)
{
    for (int i=0; i<m_list.count(); i++) {
        QJsonObject obj = m_list.at(i).toObject();
        const QString coverImgUrl = obj.value(QSL("url")).toString();
        if (coverImgUrl == url.toString()) {
            obj.insert(QSL("url"),QJsonValue(data));
            m_list[i] = obj;
        }
    }
}

QJsonArray CPixivTableModel::toJsonArray() const
{
    return m_list;
}

void CPixivTableModel::overrideRowCount(int maxRowCount)
{
    m_maxRowCount = maxRowCount;
}

void CPixivTableModel::updateTags()
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
