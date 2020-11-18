/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the demonstration applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "bookmarks.h"

#include "xbel.h"
#include "global/control.h"
#include "utils/genericfuncs.h"

#include <QBuffer>
#include <QFile>
#include <QMimeData>
#include <QBuffer>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QIcon>
#include <QFileDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QToolButton>
#include <QLocale>

#include <QDebug>

#include "ui_bookmarks.h"
#include "ui_addbookmarkdialog.h"

namespace CDefaults {
const auto sBookmarksOld = "Old Bookmarks";
const auto sBookmarksMenu = "Bookmarks Menu";
}

BookmarksManager::BookmarksManager(QObject *parent)
    : QObject(parent)
{
}

BookmarksManager::~BookmarksManager()
{
    delete m_bookmarkRootNode;
    m_bookmarkRootNode = nullptr;
}

void BookmarksManager::changeExpanded()
{
}

void BookmarksManager::load(const QByteArray& data)
{
    if (m_loaded)
        return;
    m_loaded = true;
    m_bookmarkRootNode = nullptr;

    QBuffer bookmarkFile;
    bookmarkFile.setData(data);

    XbelReader reader;
    bookmarkFile.open(QIODevice::ReadOnly);
    m_bookmarkRootNode = reader.read(&bookmarkFile);
    if (reader.error() != QXmlStreamReader::NoError) {
        qWarning() << tr("Error when loading bookmarks on line %1, column %2:\n%3")
                      .arg(reader.lineNumber())
                      .arg(reader.columnNumber())
                      .arg(reader.errorString());
    }
    bookmarkFile.close();

    if (!m_bookmarkRootNode) return;

    BookmarkNode *menu = nullptr;
    BookmarkNode *old = nullptr;
    QVector<BookmarkNode*> others;
    others.reserve(m_bookmarkRootNode->children.count());
    for (int i = m_bookmarkRootNode->children.count() - 1; i >= 0; --i) {
        BookmarkNode *node = m_bookmarkRootNode->children.at(i);
        if (node->type == BookmarkNode::Folder) {
            // Automatically convert
            if (node->title == tr("Menu") && menu == nullptr) {
                node->title = tr(CDefaults::sBookmarksMenu);
            }
            if (node->title == tr(CDefaults::sBookmarksMenu) && menu == nullptr) {
                menu = node;
            }
            if (node->title == tr(CDefaults::sBookmarksOld) && old == nullptr) {
                old = node;
            }
        } else {
            others.append(node);
        }
        m_bookmarkRootNode->remove(node);
    }
    Q_ASSERT(m_bookmarkRootNode->children.count() == 0);

    if (!menu) {
        menu = new BookmarkNode(BookmarkNode::Folder, m_bookmarkRootNode);
        menu->title = tr(CDefaults::sBookmarksMenu);
    } else {
        m_bookmarkRootNode->add(menu);
    }
    if (old) {
        m_bookmarkRootNode->add(old);
    }

    for (int i = 0; i < others.count(); ++i)
        menu->add(others.at(i));
}

QByteArray BookmarksManager::save() const
{
    if (!m_loaded)
        return QByteArray();

    XbelWriter writer;
    QBuffer bookmarkFile;
    bookmarkFile.open(QIODevice::WriteOnly);
    if (!writer.write(&bookmarkFile, m_bookmarkRootNode))
        qWarning() << "BookmarkManager: error saving to buffer";
    bookmarkFile.close();
    return bookmarkFile.data();
}

void BookmarksManager::addBookmark(BookmarkNode *parent, BookmarkNode *node, int row)
{
    if (!m_loaded)
        return;
    Q_ASSERT(parent);
    parent->add(node, row);
    Q_EMIT entryAdded(node);
}

void BookmarksManager::removeBookmark(BookmarkNode *node)
{
    if (!m_loaded)
        return;

    Q_ASSERT(node);
    BookmarkNode *parent = node->parent;
    int row = parent->children.indexOf(node);
    parent->remove(node);
    Q_EMIT entryRemoved(parent,row,node);
}

void BookmarksManager::setTitle(BookmarkNode *node, const QString &newTitle)
{
    if (!m_loaded)
        return;

    Q_ASSERT(node);
    node->title = newTitle;
    Q_EMIT entryChanged(node);
}

void BookmarksManager::setUrl(BookmarkNode *node, const QString &newUrl)
{
    if (!m_loaded)
        return;

    Q_ASSERT(node);
    node->url = newUrl;
    Q_EMIT entryChanged(node);
}

BookmarkNode *BookmarksManager::bookmarks()
{
    if (!m_loaded) return nullptr;
    return m_bookmarkRootNode;
}

BookmarkNode *BookmarksManager::menu() const
{
    if (!m_loaded) return nullptr;

    for (int i = m_bookmarkRootNode->children.count() - 1; i >= 0; --i) {
        BookmarkNode *node = m_bookmarkRootNode->children.at(i);
        if (node->title == tr(CDefaults::sBookmarksMenu))
            return node;
    }
    Q_ASSERT(false);
    return nullptr;
}

BookmarksModel *BookmarksManager::bookmarksModel()
{
    if (!m_bookmarkModel)
        m_bookmarkModel = new BookmarksModel(this, this);
    return m_bookmarkModel;
}

void BookmarksManager::populateBookmarksMenu(QMenu *menuWidget, CMainWindow* wnd, const BookmarkNode* node) const
{
    if (!m_loaded) return;

    QMenu* submenu = nullptr;
    QAction* a = nullptr;
    QString url;
    const BookmarkNode* nd = node;
    if (!nd)
        nd = menu();
    QStringList urls;

    for (int i = 0; i < nd->children.count();i++) {
        const BookmarkNode* subnode = nd->children.at(i);
        switch (subnode->type) {
            case BookmarkNode::Folder:
                submenu = menuWidget->addMenu(subnode->title);
                submenu->setStyleSheet(QSL("QMenu { menu-scrollable: 1; }"));
                submenu->setToolTipsVisible(true);
                populateBookmarksMenu(submenu, wnd, subnode);
                break;
            case BookmarkNode::Bookmark:
                a = menuWidget->addAction(subnode->title,wnd,&CMainWindow::openBookmark);
                url = subnode->url;
                a->setData(QUrl(url));
                a->setStatusTip(url);
                a->setToolTip(url);
                urls << url;
                break;
            case BookmarkNode::Separator:
                menuWidget->addSeparator();
                break;
            default:
                break;
        }
    }

    if (!urls.isEmpty()) {
        menuWidget->addSeparator();
        a = menuWidget->addAction(tr("Open all bookmarks"),wnd,&CMainWindow::openBookmark);
        a->setData(urls);
    }
}

void BookmarksManager::importBookmarks()
{
    QString fileName = CGenericFuncs::getOpenFileNameD(nullptr, tr("Open File"),
                                                       QString(),
                                                       QStringList( { tr("XBEL (*.xbel *.xml)") } ));
    if (fileName.isEmpty())
        return;

    XbelReader reader;
    BookmarkNode *importRootNode = reader.read(fileName);
    if (reader.error() != QXmlStreamReader::NoError) {
        QMessageBox::warning(nullptr, QSL("Loading Bookmark"),
                             tr("Error when loading bookmarks on line %1, column %2:\n"
                                "%3").arg(reader.lineNumber()).arg(reader.columnNumber()).arg(reader.errorString()));
    }

    importRootNode->type = BookmarkNode::Folder;
    importRootNode->title = tr("Imported %1")
                            .arg(QLocale::system().toString(QDate::currentDate(),QLocale::ShortFormat));
    addBookmark(menu(), importRootNode);
}

void BookmarksManager::exportBookmarks()
{
    QString fileName = CGenericFuncs::getSaveFileNameD(nullptr, tr("Save File"),
                                                       tr("%1 Bookmarks.xbel")
                                                       .arg(QCoreApplication::applicationName()),
                                                       QStringList( { tr("XBEL (*.xbel *.xml)") } ));
    if (fileName.isEmpty())
        return;

    XbelWriter writer;
    if (!writer.write(fileName, m_bookmarkRootNode))
        QMessageBox::critical(nullptr, tr("Export error"), tr("error saving bookmarks"));
}

BookmarksModel::BookmarksModel(BookmarksManager *bookmarkManager, QObject *parent)
    : QAbstractItemModel(parent)
    , m_bookmarksManager(bookmarkManager)
    , m_bookmarksMimeType(QSL("application/bookmarks.xbel"))
{
    connect(bookmarkManager, &BookmarksManager::entryAdded,
            this, &BookmarksModel::entryAdded);
    connect(bookmarkManager, &BookmarksManager::entryRemoved,
            this, &BookmarksModel::entryRemoved);
    connect(bookmarkManager, &BookmarksManager::entryChanged,
            this, &BookmarksModel::entryChanged);
}

QModelIndex BookmarksModel::index(BookmarkNode *node) const
{
    BookmarkNode *parent = node->parent;
    if (!parent)
        return QModelIndex();
    return createIndex(parent->children.indexOf(node), 0, node);
}

void BookmarksModel::entryAdded(BookmarkNode *item)
{
    Q_ASSERT(item && item->parent);
    int row = item->parent->children.indexOf(item);
    BookmarkNode *parent = item->parent;
    // item was already added so remove beore beginInsertRows is called
    parent->remove(item);
    beginInsertRows(index(parent), row, row);
    parent->add(item, row);
    endInsertRows();
}

void BookmarksModel::entryRemoved(BookmarkNode *parent, int row, BookmarkNode *item)
{
    // item was already removed, re-add so beginRemoveRows works
    parent->add(item, row);
    beginRemoveRows(index(parent), row, row);
    parent->remove(item);
    endRemoveRows();
}

void BookmarksModel::entryChanged(BookmarkNode *item)
{
    QModelIndex idx = index(item);
    Q_EMIT dataChanged(idx, idx);
}

bool BookmarksModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if (row < 0 || count <= 0 || row + count > rowCount(parent))
        return false;

    BookmarkNode *bookmarkNode = node(parent);
    for (int i = row + count - 1; i >= row; --i) {
        BookmarkNode *node = bookmarkNode->children.at(i);
        if (node == m_bookmarksManager->menu())
            continue;

        m_bookmarksManager->removeBookmark(node);
    }
    return true;
}

QVariant BookmarksModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
            case 0: return tr("Title");
            case 1: return tr("Address");
            default: return QString();
        }
    }
    return QAbstractItemModel::headerData(section, orientation, role);
}

QVariant BookmarksModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.model() != this)
        return QVariant();

    const int separatorLength = 50;
    const QChar separatorDot(0xB7);

    const BookmarkNode *bookmarkNode = node(index);
    switch (role) {
        case Qt::EditRole:
        case Qt::DisplayRole:
            if (bookmarkNode->type == BookmarkNode::Separator) {
                switch (index.column()) {
                    case 0: return QString(separatorLength, separatorDot);
                    default: return QString();
                }
            }

            switch (index.column()) {
                case 0: return bookmarkNode->title;
                case 1: return bookmarkNode->url;
                default: return QString();
            }
        case BookmarksModel::UrlRole: return QUrl(bookmarkNode->url);
        case BookmarksModel::UrlStringRole: return bookmarkNode->url;
        case BookmarksModel::TypeRole: return bookmarkNode->type;
        case BookmarksModel::SeparatorRole:
            return (bookmarkNode->type == BookmarkNode::Separator);
        case Qt::DecorationRole:
            if (index.column() == 0) {
                if (bookmarkNode->type == BookmarkNode::Folder)
                    return QApplication::style()->standardIcon(QStyle::SP_DirIcon);
                return QApplication::style()->standardIcon(QStyle::SP_FileIcon);
            }
            return QVariant();
        default: return QVariant();
    }
}

int BookmarksModel::columnCount(const QModelIndex &parent) const
{
    return (parent.column() > 0) ? 0 : 2;
}

int BookmarksModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;

    if (!parent.isValid())
        return m_bookmarksManager->bookmarks()->children.count();

    const BookmarkNode *item = static_cast<BookmarkNode*>(parent.internalPointer());
    return item->children.count();
}

QModelIndex BookmarksModel::index(int row, int column, const QModelIndex &parent) const
{
    if (row < 0 || column < 0 || row >= rowCount(parent) || column >= columnCount(parent))
        return QModelIndex();

    // get the parent node
    BookmarkNode *parentNode = node(parent);
    return createIndex(row, column, parentNode->children.at(row));
}

QModelIndex BookmarksModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    BookmarkNode *itemNode = node(index);
    BookmarkNode *parentNode = (itemNode ? itemNode->parent : nullptr);
    if (!parentNode || parentNode == m_bookmarksManager->bookmarks())
        return QModelIndex();

    // get the parent's row
    BookmarkNode *grandParentNode = parentNode->parent;
    int parentRow = grandParentNode->children.indexOf(parentNode);
    Q_ASSERT(parentRow >= 0);
    return createIndex(parentRow, 0, parentNode);
}

bool BookmarksModel::hasChildren(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return true;
    const BookmarkNode *parentNode = node(parent);
    return (parentNode->type == BookmarkNode::Folder);
}

Qt::ItemFlags BookmarksModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    Qt::ItemFlags flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;

    BookmarkNode *bookmarkNode = node(index);

    if (bookmarkNode != m_bookmarksManager->menu()) {
        flags |= Qt::ItemIsDragEnabled;
        if (bookmarkNode->type != BookmarkNode::Separator)
            flags |= Qt::ItemIsEditable;
    }
    if (hasChildren(index))
        flags |= Qt::ItemIsDropEnabled;
    return flags;
}

Qt::DropActions BookmarksModel::supportedDropActions () const
{
    return Qt::CopyAction | Qt::MoveAction;
}

QStringList BookmarksModel::mimeTypes() const
{
    QStringList types;
    types << m_bookmarksMimeType;
    return types;
}

QMimeData *BookmarksModel::mimeData(const QModelIndexList &indexes) const
{
    auto *mimeData = new QMimeData();
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    for (const QModelIndex& index : indexes) {
        if (index.column() != 0 || !index.isValid())
            continue;
        QByteArray encodedData;
        QBuffer buffer(&encodedData);
        buffer.open(QBuffer::ReadWrite);
        XbelWriter writer;
        const BookmarkNode *parentNode = node(index);
        writer.write(&buffer, parentNode);
        stream << encodedData;
    }
    mimeData->setData(m_bookmarksMimeType, data);
    return mimeData;
}

bool BookmarksModel::dropMimeData(const QMimeData *data,
                                  Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
    if (action == Qt::IgnoreAction)
        return true;

    if (!data->hasFormat(m_bookmarksMimeType)
            || column > 0)
        return false;

    QByteArray ba = data->data(m_bookmarksMimeType);
    QDataStream stream(&ba, QIODevice::ReadOnly);
    if (stream.atEnd())
        return false;

    while (!stream.atEnd()) {
        QByteArray encodedData;
        stream >> encodedData;
        QBuffer buffer(&encodedData);
        buffer.open(QBuffer::ReadOnly);

        XbelReader reader;
        BookmarkNode *rootNode = reader.read(&buffer);
        QVector<BookmarkNode*> children = rootNode->children;
        for (int i = 0; i < children.count(); ++i) {
            BookmarkNode *bookmarkNode = children.at(i);
            rootNode->remove(bookmarkNode);
            row = qMax(0, row);
            BookmarkNode *parentNode = node(parent);
            m_bookmarksManager->addBookmark(parentNode, bookmarkNode, row);
        }
        delete rootNode;
    }
    return true;
}

bool BookmarksModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || (flags(index) & Qt::ItemIsEditable) == 0)
        return false;

    BookmarkNode *item = node(index);

    switch (role) {
        case Qt::EditRole:
        case Qt::DisplayRole:
            if (index.column() == 0) {
                m_bookmarksManager->setTitle(item, value.toString());
                break;
            }
            if (index.column() == 1) {
                m_bookmarksManager->setUrl(item, value.toString());
                break;
            }
            return false;
        case BookmarksModel::UrlRole:
            m_bookmarksManager->setUrl(item, value.toUrl().toString());
            break;
        case BookmarksModel::UrlStringRole:
            m_bookmarksManager->setUrl(item, value.toString());
            break;
        default:
            return false;
    }

    return true;
}

BookmarkNode *BookmarksModel::node(const QModelIndex &index) const
{
    auto *itemNode = static_cast<BookmarkNode*>(index.internalPointer());
    if (!itemNode)
        return m_bookmarksManager->bookmarks();
    return itemNode;
}


AddBookmarkProxyModel::AddBookmarkProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

int AddBookmarkProxyModel::columnCount(const QModelIndex &parent) const
{
    return qMin(1, QSortFilterProxyModel::columnCount(parent));
}

bool AddBookmarkProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    QModelIndex idx = sourceModel()->index(source_row, 0, source_parent);
    return sourceModel()->hasChildren(idx);
}

AddBookmarkDialog::AddBookmarkDialog(const QString &url, const QString &title, QWidget *parent, BookmarksManager *bookmarkManager)
    : QDialog(parent)
    , ui(new Ui::AddBookmarkDialog)
    , m_bookmarksManager(bookmarkManager)
{
    const int viewIndent = 10;

    setWindowFlags(Qt::Sheet);
    if (!m_bookmarksManager)
        m_bookmarksManager = gSet->bookmarksManager();
    ui->setupUi(this);
    m_url = url;
    auto *view = new QTreeView(this);
    auto *model = m_bookmarksManager->bookmarksModel();
    m_proxyModel = new AddBookmarkProxyModel(this);
    m_proxyModel->setSourceModel(model);
    view->setModel(m_proxyModel);
    view->expandAll();
    view->header()->setStretchLastSection(true);
    view->header()->hide();
    view->setItemsExpandable(false);
    view->setRootIsDecorated(false);
    view->setIndentation(viewIndent);
    ui->location->setModel(m_proxyModel);
    view->show();
    ui->location->setView(view);
    BookmarkNode *menu = m_bookmarksManager->menu();
    QModelIndex idx = m_proxyModel->mapFromSource(model->index(menu));
    view->setCurrentIndex(idx);
    ui->location->setCurrentIndex(idx.row());
    ui->name->setText(title);
}

void AddBookmarkDialog::accept()
{
    QModelIndex index = ui->location->view()->currentIndex();
    index = m_proxyModel->mapToSource(index);
    if (!index.isValid())
        index = m_bookmarksManager->bookmarksModel()->index(0, 0);
    BookmarkNode *parent = m_bookmarksManager->bookmarksModel()->node(index);
    auto *bookmark = new BookmarkNode(BookmarkNode::Bookmark);
    bookmark->url = m_url;
    bookmark->title = ui->name->text();
    m_bookmarksManager->addBookmark(parent, bookmark);
    QDialog::accept();
}

TreeProxyModel::TreeProxyModel(QObject *parent, BookmarksModel *sourceModel)
    : QSortFilterProxyModel(parent)
    , m_sourceModel(sourceModel)
{
    setFilterCaseSensitivity(Qt::CaseInsensitive);
}

bool TreeProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    QModelIndex idx = m_sourceModel->index(source_row,0,source_parent);
    if (!idx.isValid()) return false;

    BookmarkNode* node = m_sourceModel->node(idx);
    if (node->type==BookmarkNode::Root || node->type==BookmarkNode::Folder)
        return true; // show all folders

    return QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
}

BookmarksDialog::BookmarksDialog(QWidget *parent, BookmarksManager *manager)
    : QDialog(parent)
    , ui(new Ui::BookmarksDialog)
{
    ui->setupUi(this);

    const int headerMaxCharWidth = 40;

    m_bookmarksManager = manager;
    if (!m_bookmarksManager)
        m_bookmarksManager = gSet->bookmarksManager();

    ui->tree->setUniformRowHeights(true);
    ui->tree->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tree->setSelectionMode(QAbstractItemView::ContiguousSelection);
    ui->tree->setTextElideMode(Qt::ElideMiddle);
    m_bookmarksModel = m_bookmarksManager->bookmarksModel();
    m_proxyModel = new TreeProxyModel(this, m_bookmarksModel);
    connect(ui->search, &QLineEdit::textChanged,
            m_proxyModel, &TreeProxyModel::setFilterFixedString);
    connect(ui->removeButton, &QPushButton::clicked, ui->tree, &EditTreeView::removeOne);
    m_proxyModel->setSourceModel(m_bookmarksModel);
    ui->tree->setModel(m_proxyModel);
    ui->tree->setDragDropMode(QAbstractItemView::InternalMove);
    ui->tree->setExpanded(m_proxyModel->index(0, 0), true);
    ui->tree->setAlternatingRowColors(true);
    QFontMetrics fm(font());
    int header = fm.boundingRect(QLatin1Char('m')).width() * headerMaxCharWidth;
    ui->tree->header()->resizeSection(0, header);
    ui->tree->header()->setStretchLastSection(true);
    connect(ui->tree, &EditTreeView::activated,
            this, &BookmarksDialog::openEx);
    ui->tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree, &EditTreeView::customContextMenuRequested,
            this, &BookmarksDialog::customContextMenuRequested);
    connect(ui->tree, &EditTreeView::removeNode,
            this, &BookmarksDialog::removeNode);
    connect(ui->addFolderButton, &QPushButton::clicked,
            this, &BookmarksDialog::newFolder);
    connect(ui->addSeparatorButton, &QPushButton::clicked,
            this, &BookmarksDialog::newSeparator);
    expandNodes(m_bookmarksManager->bookmarks());
}

BookmarksDialog::~BookmarksDialog()
{
    if (saveExpandedNodes(ui->tree->rootIndex()))
        m_bookmarksManager->changeExpanded();
}

bool BookmarksDialog::saveExpandedNodes(const QModelIndex &parent)
{
    bool changed = false;
    for (int i = 0; i < m_proxyModel->rowCount(parent); ++i) {
        QModelIndex child = m_proxyModel->index(i, 0, parent);
        QModelIndex sourceIndex = m_proxyModel->mapToSource(child);
        BookmarkNode *childNode = m_bookmarksModel->node(sourceIndex);
        bool wasExpanded = childNode->expanded;
        if (ui->tree->isExpanded(child)) {
            childNode->expanded = true;
            changed |= saveExpandedNodes(child);
        } else {
            childNode->expanded = false;
        }
        changed |= (wasExpanded != childNode->expanded);
    }
    return changed;
}

void BookmarksDialog::removeNode(const QModelIndex &node)
{
    if (!node.isValid()) return;
    QModelIndex idx = m_proxyModel->mapToSource(node);
    if (!idx.isValid()) return;

    BookmarkNode *childNode = m_bookmarksModel->node(idx);

    if (*childNode!=*(gSet->bookmarksManager()->menu())) // this is not predefined folder
        m_bookmarksManager->removeBookmark(childNode);
}

void BookmarksDialog::expandNodes(BookmarkNode *node)
{
    const QVector<BookmarkNode *> nl = node->children;
    for (auto * const child : nl) {
        if (child->expanded) {
            QModelIndex idx = m_bookmarksModel->index(child);
            idx = m_proxyModel->mapFromSource(idx);
            ui->tree->setExpanded(idx, true);
            expandNodes(child);
        }
    }
}

void BookmarksDialog::customContextMenu(const QPoint &pos)
{
    QMenu menu;
    QModelIndex index = ui->tree->indexAt(pos);
    index = index.sibling(index.row(), 0);
    if (index.isValid() && !ui->tree->model()->hasChildren(index)) {
        menu.addAction(tr("Open"), this, &BookmarksDialog::open);
        menu.addSeparator();
    }
    menu.addAction(tr("Delete"), ui->tree, &EditTreeView::removeOne);
    menu.exec(QCursor::pos());
}

void BookmarksDialog::open()
{
    QModelIndex index = ui->tree->currentIndex();
    openEx(index);
}

void BookmarksDialog::openEx(const QModelIndex& index)
{
    if (!index.parent().isValid())
        return;
    Q_EMIT openUrl(index.sibling(index.row(), 1).data(BookmarksModel::UrlRole).toUrl());
}

void BookmarksDialog::newFolder()
{
    QModelIndex currentIndex = ui->tree->currentIndex();
    QModelIndex idx = currentIndex;

    int row = 0;
    if (idx.isValid()) {
        if (!idx.model()->hasChildren(idx)) {
            idx = idx.parent();
            row = currentIndex.row() + 1;
        }
    } else {
        idx = ui->tree->rootIndex();
    }

    idx = m_proxyModel->mapToSource(idx);
    BookmarkNode *parent = m_bookmarksManager->bookmarksModel()->node(idx);
    auto *node = new BookmarkNode(BookmarkNode::Folder);
    node->title = tr("New Folder");
    m_bookmarksManager->addBookmark(parent, node, row);
}

void BookmarksDialog::newSeparator()
{
    QModelIndex currentIndex = ui->tree->currentIndex();
    QModelIndex idx = currentIndex;

    int row = 0;
    if (idx.isValid()) {
        if (!idx.model()->hasChildren(idx)) {
            idx = idx.parent();
            row = currentIndex.row() + 1;
        }
    } else {
        idx = ui->tree->rootIndex();
    }

    idx = m_proxyModel->mapToSource(idx);
    BookmarkNode *parent = m_bookmarksManager->bookmarksModel()->node(idx);
    auto *node = new BookmarkNode(BookmarkNode::Separator);
    m_bookmarksManager->addBookmark(parent, node, row);
}
