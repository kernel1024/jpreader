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

#ifndef BOOKMARKS_H
#define BOOKMARKS_H

#include <QtCore/QObject>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QByteArray>
#include <QtWidgets/QDialog>
#include <QtWidgets/QWidget>
#include <QtWidgets/QMenu>

#include "structures.h"
#include "mainwindow.h"


/*!
    Bookmark manager, owner of the bookmarks, loads, saves and basic tasks
  */
class BookmarkNode;
class BookmarksModel;
class BookmarksManager : public QObject
{
    Q_OBJECT

signals:
    void entryAdded(BookmarkNode *item);
    void entryRemoved(BookmarkNode *parent, int row, BookmarkNode *item);
    void entryChanged(BookmarkNode *item);

public:
    BookmarksManager(QObject *parent = 0);
    ~BookmarksManager();

    void addBookmark(BookmarkNode *parent, BookmarkNode *node, int row = -1);
    void removeBookmark(BookmarkNode *node);
    void setTitle(BookmarkNode *node, const QString &newTitle);
    void setUrl(BookmarkNode *node, const QString &newUrl);
    void changeExpanded();

    void load(const QByteArray &data);
    QByteArray save() const;
    void importOldBookmarks(const QBookmarks &bookmarks);

    BookmarkNode *bookmarks();
    BookmarkNode *menu();

    BookmarksModel *bookmarksModel();

    void populateBookmarksMenu(QMenu* menu, CMainWindow *wnd, const BookmarkNode *node = nullptr);

public slots:
    void importBookmarks();
    void exportBookmarks();

private slots:

private:
    bool m_loaded;
    BookmarkNode *m_bookmarkRootNode;
    BookmarksModel *m_bookmarkModel;

};

/*!
    BookmarksModel is a QAbstractItemModel wrapper around the BookmarkManager
  */
class BookmarksModel : public QAbstractItemModel
{
    Q_OBJECT

public slots:
    void entryAdded(BookmarkNode *item);
    void entryRemoved(BookmarkNode *parent, int row, BookmarkNode *item);
    void entryChanged(BookmarkNode *item);

public:
    enum Roles {
        TypeRole = Qt::UserRole + 1,
        UrlRole = Qt::UserRole + 2,
        UrlStringRole = Qt::UserRole + 3,
        SeparatorRole = Qt::UserRole + 4
    };

    BookmarksModel(BookmarksManager *bookmarkManager, QObject *parent = 0);
    inline BookmarksManager *bookmarksManager() const { return m_bookmarksManager; }

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QModelIndex index(int, int, const QModelIndex& = QModelIndex()) const;
    QModelIndex parent(const QModelIndex& index= QModelIndex()) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    Qt::DropActions supportedDropActions () const;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex());
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);
    QMimeData *mimeData(const QModelIndexList &indexes) const;
    QStringList mimeTypes() const;
    bool dropMimeData(const QMimeData *data,
        Qt::DropAction action, int row, int column, const QModelIndex &parent);
    bool hasChildren(const QModelIndex &parent = QModelIndex()) const;

    BookmarkNode *node(const QModelIndex &index) const;
    QModelIndex index(BookmarkNode *node) const;

private:

    BookmarksManager *m_bookmarksManager;
};

/*
    Proxy model that filters out the bookmarks so only the folders
    are left behind.  Used in the add bookmark dialog combobox.
 */
#include <QtCore/QSortFilterProxyModel>
class AddBookmarkProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    AddBookmarkProxyModel(QObject * parent = 0);
    int columnCount(const QModelIndex & parent = QModelIndex()) const;

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const;
};

/*!
    Add bookmark dialog
 */
#include "ui_addbookmarkdialog.h"
class AddBookmarkDialog : public QDialog, public Ui_AddBookmarkDialog
{
    Q_OBJECT

public:
    AddBookmarkDialog(const QString &url, const QString &title, QWidget *parent = 0, BookmarksManager *bookmarkManager = 0);

private slots:
    void accept();

private:
    QString m_url;
    BookmarksManager *m_bookmarksManager;
    AddBookmarkProxyModel *m_proxyModel;
};

#include "ui_bookmarks.h"
// A modified QSortFilterProxyModel that always accepts the root nodes in the tree
// so filtering is only done on the children.
// Used in the HistoryDialog
class TreeProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    TreeProxyModel(QObject *parent = 0);

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const;
};


class BookmarksDialog : public QDialog, public Ui_BookmarksDialog
{
    Q_OBJECT

signals:
    void openUrl(const QUrl &url);

public:
    BookmarksDialog(QWidget *parent = 0, BookmarksManager *manager = 0);
    ~BookmarksDialog();

private slots:
    void customContextMenuRequested(const QPoint &pos);
    void open();
    void newFolder();

private:
    void expandNodes(BookmarkNode *node);
    bool saveExpandedNodes(const QModelIndex &parent);
    void removeNode(const QModelIndex &node);

    BookmarksManager *m_bookmarksManager;
    BookmarksModel *m_bookmarksModel;
    TreeProxyModel *m_proxyModel;
};


#endif // BOOKMARKS_H
