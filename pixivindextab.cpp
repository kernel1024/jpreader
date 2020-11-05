#include <QSortFilterProxyModel>
#include <QJsonArray>
#include <QMenu>
#include <QThread>
#include <QMessageBox>
#include "globalcontrol.h"
#include "snviewer.h"
#include "pixivindextab.h"
#include "extractors/abstractextractor.h"
#include "ui_pixivindextab.h"

namespace CDefaults {
const int pixivSortRole = Qt::UserRole + 1;
}

CPixivIndexTab::CPixivIndexTab(QWidget *parent, const QVector<QJsonObject> &list,
                               CPixivIndexExtractor::IndexMode indexMode,
                               const QString &indexId, const QUrlQuery &sourceQuery) :
    CSpecTabContainer(parent),
    ui(new Ui::CPixivIndexTab),
    m_indexId(indexId),
    m_sourceQuery(sourceQuery),
    m_indexMode(indexMode)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose,true);
    setTabTitle(tr("Pixiv list"));

    ui->table->setContextMenuPolicy(Qt::CustomContextMenu);

    m_model = new CPixivTableModel(this,list);
    auto *proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSourceModel(m_model);
    proxyModel->setSortRole(CDefaults::pixivSortRole);
    ui->table->setModel(proxyModel);

    connect(ui->table,&QTableView::activated,this,&CPixivIndexTab::novelActivated);
    connect(ui->table->selectionModel(),&QItemSelectionModel::currentRowChanged,
            this,&CPixivIndexTab::tableSelectionChanged);
    connect(ui->table,&QTableView::customContextMenuRequested,this,&CPixivIndexTab::tableContextMenu);
    connect(ui->labelHead,&QLabel::linkActivated,this,&CPixivIndexTab::linkClicked);
    connect(ui->buttonReport,&QPushButton::clicked,this,&CPixivIndexTab::htmlReport);
    connect(ui->editDescription,&QTextBrowser::anchorClicked,this,[this](const QUrl& url){
        linkClicked(url.toString());
    });

    updateWidgets();
    bindToTab(parentWnd()->tabMain);
}

CPixivIndexTab::~CPixivIndexTab()
{
    delete ui;
}

QString CPixivIndexTab::title() const
{
    return m_title;
}

void CPixivIndexTab::updateWidgets()
{
    ui->labelHead->clear();
    ui->labelCount->clear();
    ui->editDescription->clear();

    ui->table->resizeColumnsToContents();

    auto *proxy = qobject_cast<QAbstractProxyModel *>(ui->table->model());
    if (proxy)
        ui->labelCount->setText(tr("Found %1 novels.").arg(proxy->rowCount()));

    if (m_model->isEmpty()) {
        ui->labelHead->setText(tr("Nothing found."));

    } else {
        QString header;
        switch (m_indexMode) {
            case CPixivIndexExtractor::WorkIndex: header = tr("works"); break;
            case CPixivIndexExtractor::BookmarksIndex: header = tr("bookmarks"); break;
            case CPixivIndexExtractor::TagSearchIndex: header = tr("tag search"); break;
        }
        QString author = m_model->authors().value(m_indexId);
        if (author.isEmpty())
            author = tr("author");
        if (m_indexMode == CPixivIndexExtractor::TagSearchIndex)
            author = m_indexId;

        m_title = tr("Pixiv %1 for %2").arg(header,author);
        setTabTitle(m_title);
        if (m_indexMode == CPixivIndexExtractor::TagSearchIndex) {
            header = QSL("Pixiv %1 list for <a href=\"https://www.pixiv.net/tags/%2/novels?%4\">"
                         "%3</a>.").arg(header,m_indexId,author,m_sourceQuery.query());
        } else {
            header = QSL("Pixiv %1 list for <a href=\"https://www.pixiv.net/users/%2\">"
                         "%3</a>.").arg(header,m_indexId,author);
        }
        ui->labelHead->setText(header);
    }
}

void CPixivIndexTab::tableSelectionChanged(const QModelIndex &current, const QModelIndex &previous)
{
    Q_UNUSED(previous)

    ui->editDescription->clear();

    if (!current.isValid()) return;

    auto *proxy = qobject_cast<QAbstractProxyModel *>(ui->table->model());
    if (proxy) {
        ui->editDescription->setHtml(m_model->item(proxy->mapToSource(current))
                                     .value(QSL("description")).toString());
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
            new CSnippetViewer(gSet->activeWindow(),QUrl(
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
            new CSnippetViewer(gSet->activeWindow(),novelUrl);
        });
    }
    if (!seriesUrl.isEmpty()) {
        cm.addAction(tr("Open series list in new browser tab"),gSet,[seriesUrl](){
            new CSnippetViewer(gSet->activeWindow(),seriesUrl);
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
            QStringList tags;
            const QJsonArray wtags = w.value(QSL("tags")).toArray();
            tags.reserve(wtags.count());
            for (const auto& t : qAsConst(wtags))
                tags.append(t.toString());

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
        case CPixivIndexExtractor::WorkIndex: header = tr("works"); break;
        case CPixivIndexExtractor::BookmarksIndex: header = tr("bookmarks"); break;
        case CPixivIndexExtractor::TagSearchIndex: header = tr("tag search"); break;
    }
    QString author = authors.value(m_indexId);
    if (author.isEmpty())
        author = tr("author");
    if (m_indexMode == CPixivIndexExtractor::TagSearchIndex)
        author = m_indexId;

    if (m_indexMode == CPixivIndexExtractor::TagSearchIndex) {
        header = QSL("<h3>Pixiv %1 list for <a href=\"https://www.pixiv.net/tags/%2/novels?%4\">"
                     "%3</a>.</h3>").arg(header,m_indexId,author,m_sourceQuery.query());
    } else {
        header = QSL("<h3>Pixiv %1 list for <a href=\"https://www.pixiv.net/users/%2\">"
                     "%3</a>.</h3>").arg(header,m_indexId,author);
    }
    html.prepend(header);

    html = CGenericFuncs::makeSimpleHtml(m_title,html);

    new CSnippetViewer(gSet->activeWindow(),QUrl(),QStringList(),true,html);
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
    new CSnippetViewer(gSet->activeWindow(),QUrl(link));
}

void CPixivIndexTab::processExtractorAction()
{
    auto *ac = qobject_cast<QAction*>(sender());
    if (!ac) return;

    auto *ex = CAbstractExtractor::extractorWorkerFactory(ac->data().toHash(),this);
    if (ex == nullptr) {
        QMessageBox::critical(this,QGuiApplication::applicationDisplayName(),
                              tr("Failed to initialize extractor."));
        return;
    }

    connect(ex,&CAbstractExtractor::novelReady,gSet,[]
            (const QString &html, bool focus, bool translate){
        auto *sv = new CSnippetViewer(gSet->activeWindow(),QUrl(),QStringList(),focus,html);
        sv->setRequestAutotranslate(translate);
    },Qt::QueuedConnection);

    QMetaObject::invokeMethod(ex,&CAbstractExtractor::start,Qt::QueuedConnection);
}

CPixivTableModel::CPixivTableModel(QObject *parent, const QVector<QJsonObject> &list)
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
    const auto w = m_list.at(row);
    const int basicWidth = basicHeaders().count();

    if (role == Qt::DisplayRole) {
        switch (col) {
            case 0: return w.value(QSL("id")).toString();
            case 1: return w.value(QSL("title")).toString();
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
                int tagNum = col - basicWidth;
                if (tagNum>=0 && tagNum<m_tags.count()) {
                    const QJsonArray wtags = w.value(QSL("tags")).toArray();
                    const QString tag = m_tags.at(tagNum);
                    if (wtags.contains(QJsonValue(tag)))
                        return tag;
                }
                return QVariant();
            }
        }
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
                int tagNum = col - basicWidth;
                if (tagNum>=0 && tagNum<m_tags.count()) {
                    const QJsonArray wtags = w.value(QSL("tags")).toArray();
                    const QString tag = m_tags.at(tagNum);
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
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        QStringList headers = basicHeaders();

        headers.reserve(m_tags.count());
        for (const auto& tag : qAsConst(m_tags))
            headers.append(QSL("T:[%1]").arg(tag));

        if (section>=0 && section<headers.count())
            return headers.at(section);
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

QJsonObject CPixivTableModel::item(const QModelIndex &index) const
{
    if (!checkIndex(index,CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return QJsonObject();

    return m_list.at(index.row());
}

QString CPixivTableModel::tag(const QModelIndex &index) const
{
    if (!checkIndex(index,CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return QString();

    if (index.column()<basicHeaders().count())
        return QString();

    return data(index,Qt::DisplayRole).toString();
}

QStringList CPixivTableModel::basicHeaders() const
{
    static const QStringList res({ tr("ID"), tr("Title"), tr("Author"), tr("Size"), tr("Date"),
                                   tr("Bookmarked"), tr("Series") });
    return res;
}

void CPixivTableModel::updateTags()
{
    m_tags.clear();
    m_authors.clear();

    for (const auto &w : qAsConst(m_list)) {
        const QJsonArray wtags = w.value(QSL("tags")).toArray();
        for (const auto& t : qAsConst(wtags)) {
            const QString tag = t.toString();
            if (!m_tags.contains(tag))
                m_tags.append(tag);
        }

        const QString authorId = w.value(QSL("userId")).toString();
        m_authors[authorId] = w.value(QSL("userName")).toString();
    }
    std::sort(m_tags.begin(), m_tags.end());
}
