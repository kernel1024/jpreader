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

#include <QObject>
#include <QAbstractItemModel>
#include <QByteArray>
#include <QSortFilterProxyModel>
#include <QDialog>
#include <QWidget>
#include <QMenu>

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

Q_SIGNALS:
    void entryAdded(BookmarkNode *item);
    void entryRemoved(BookmarkNode *parent, int row, BookmarkNode *item);
    void entryChanged(BookmarkNode *item);

public:
    explicit BookmarksManager(QObject *parent = nullptr);
    ~BookmarksManager() override;

    void addBookmark(BookmarkNode *parent, BookmarkNode *node, int row = -1);
    void removeBookmark(BookmarkNode *node);
    void setTitle(BookmarkNode *node, const QString &newTitle);
    void setUrl(BookmarkNode *node, const QString &newUrl);
    void changeExpanded();

    void load(const QByteArray &data);
    QByteArray save() const;

    BookmarkNode *bookmarks();
    BookmarkNode *menu();

    BookmarksModel *bookmarksModel();

    void populateBookmarksMenu(QMenu* menu, CMainWindow *wnd, const BookmarkNode *node = nullptr);

public Q_SLOTS:
    void importBookmarks();
    void exportBookmarks();

private Q_SLOTS:

private:
    bool m_loaded { false };
    BookmarkNode *m_bookmarkRootNode { nullptr };
    BookmarksModel *m_bookmarkModel { nullptr };

    Q_DISABLE_COPY(BookmarksManager)

};

/*!
    BookmarksModel is a QAbstractItemModel wrapper around the BookmarkManager
  */
class BookmarksModel : public QAbstractItemModel
{
    Q_OBJECT

public Q_SLOTS:
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

    explicit BookmarksModel(BookmarksManager *bookmarkManager, QObject *parent = nullptr);
    inline BookmarksManager *bookmarksManager() const { return m_bookmarksManager; }

    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& index= QModelIndex()) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    Qt::DropActions supportedDropActions () const override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    QStringList mimeTypes() const override;
    bool dropMimeData(const QMimeData *data,
        Qt::DropAction action, int row, int column, const QModelIndex &parent) override;
    bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;

    BookmarkNode *node(const QModelIndex &index) const;
    QModelIndex index(BookmarkNode *node) const;

private:

    BookmarksManager *m_bookmarksManager { nullptr };
};

/*
    Proxy model that filters out the bookmarks so only the folders
    are left behind.  Used in the add bookmark dialog combobox.
 */
class AddBookmarkProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit AddBookmarkProxyModel(QObject * parent = nullptr);
    int columnCount(const QModelIndex & parent = QModelIndex()) const override;

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
};

/*!
    Add bookmark dialog
 */
//#include "ui_addbookmarkdialog.h"
namespace Ui {
class AddBookmarkDialog;
}
class AddBookmarkDialog : public QDialog
{
    Q_OBJECT

public:
    AddBookmarkDialog(const QString &url, const QString &title, QWidget *parent = nullptr,
                      BookmarksManager *bookmarkManager = nullptr);

private Q_SLOTS:
    void accept() override;

private:
    Ui::AddBookmarkDialog *ui;
    QString m_url;
    BookmarksManager *m_bookmarksManager;
    AddBookmarkProxyModel *m_proxyModel;
};

//#include "ui_bookmarks.h"
class TreeProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    TreeProxyModel(QObject *parent, BookmarksModel *sourceModel);

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

private:
    BookmarksModel *m_sourceModel;
};

namespace Ui {
class BookmarksDialog;
}
class BookmarksDialog : public QDialog
{
    Q_OBJECT

Q_SIGNALS:
    void openUrl(const QUrl &url);

public:
    explicit BookmarksDialog(QWidget *parent = nullptr, BookmarksManager *manager = nullptr);
    ~BookmarksDialog() override;

private Q_SLOTS:
    void customContextMenuRequested(const QPoint &pos);
    void open() override;
    void openEx(const QModelIndex &index);
    void newFolder();
    void newSeparator();

private:
    void expandNodes(BookmarkNode *node);
    bool saveExpandedNodes(const QModelIndex &parent);
    void removeNode(const QModelIndex &node);

    Ui::BookmarksDialog *ui;
    BookmarksManager *m_bookmarksManager;
    BookmarksModel *m_bookmarksModel;
    TreeProxyModel *m_proxyModel;

    Q_DISABLE_COPY(BookmarksDialog)
};


#endif // BOOKMARKS_H
