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

#include "xbel.h"

#include <QtCore/QFile>

BookmarkNode::BookmarkNode(BookmarkNode::Type type, BookmarkNode *parent) :
     expanded(false)
   , m_parent(parent)
   , m_type(type)
{
    if (parent)
        parent->add(this);
}

BookmarkNode::~BookmarkNode()
{
    if (m_parent)
        m_parent->remove(this);
    qDeleteAll(m_children);
    m_parent = nullptr;
    m_type = BookmarkNode::Root;
}

bool BookmarkNode::operator==(const BookmarkNode &other)
{
    if (url != other.url
        || title != other.title
        || desc != other.desc
        || expanded != other.expanded
        || m_type != other.m_type
        || m_children.count() != other.m_children.count())
        return false;

    for (int i = 0; i < m_children.count(); ++i)
        if (!((*(m_children[i])) == (*(other.m_children[i]))))
            return false;
    return true;
}

bool BookmarkNode::operator!=(const BookmarkNode &other)
{
    return !operator==(other);
}

BookmarkNode::Type BookmarkNode::type() const
{
    return m_type;
}

void BookmarkNode::setType(Type type)
{
    m_type = type;
}

QVector<BookmarkNode *> BookmarkNode::children() const
{
    return m_children;
}

BookmarkNode *BookmarkNode::parent() const
{
    return m_parent;
}

void BookmarkNode::add(BookmarkNode *child, int offset)
{
    Q_ASSERT(child->m_type != Root);
    if (child->m_parent)
        child->m_parent->remove(child);
    child->m_parent = this;
    if (-1 == offset)
        offset = m_children.size();
    m_children.insert(offset, child);
}

void BookmarkNode::remove(BookmarkNode *child)
{
    child->m_parent = nullptr;
    m_children.removeAll(child);
}

BookmarkNode *XbelReader::read(const QString &fileName)
{
    QFile file(fileName);
    if (!file.exists()) {
        return new BookmarkNode(BookmarkNode::Root);
    }
    file.open(QFile::ReadOnly);
    return read(&file);
}

BookmarkNode *XbelReader::read(QIODevice *device)
{
    auto root = new BookmarkNode(BookmarkNode::Root);
    setDevice(device);
    if (readNextStartElement()) {
        QString version = attributes().value(QStringLiteral("version")).toString();
        if (name() == QStringLiteral("xbel")
            && (version.isEmpty() || version == QStringLiteral("1.0"))) {
            readXBEL(root);
        } else {
            raiseError(QObject::tr("The file is not an XBEL version 1.0 file."));
        }
    }
    return root;
}

void XbelReader::readXBEL(BookmarkNode *parent)
{
    Q_ASSERT(isStartElement() && name() == QStringLiteral("xbel"));

    while (readNextStartElement()) {
        if (name() == QStringLiteral("folder"))
            readFolder(parent);
        else if (name() == QStringLiteral("bookmark"))
            readBookmarkNode(parent);
        else if (name() == QStringLiteral("separator"))
            readSeparator(parent);
        else
            skipCurrentElement();
    }
}

void XbelReader::readFolder(BookmarkNode *parent)
{
    Q_ASSERT(isStartElement() && name() == QStringLiteral("folder"));

    auto folder = new BookmarkNode(BookmarkNode::Folder, parent);
    folder->expanded = (attributes().value(QStringLiteral("folded")) == QStringLiteral("no"));

    while (readNextStartElement()) {
        if (name() == QStringLiteral("title"))
            readTitle(folder);
        else if (name() == QStringLiteral("desc"))
            readDescription(folder);
        else if (name() == QStringLiteral("folder"))
            readFolder(folder);
        else if (name() == QStringLiteral("bookmark"))
            readBookmarkNode(folder);
        else if (name() == QStringLiteral("separator"))
            readSeparator(folder);
        else
            skipCurrentElement();
    }
}

void XbelReader::readTitle(BookmarkNode *parent)
{
    Q_ASSERT(isStartElement() && name() == QStringLiteral("title"));
    parent->title = readElementText();
}

void XbelReader::readDescription(BookmarkNode *parent)
{
    Q_ASSERT(isStartElement() && name() == QStringLiteral("desc"));
    parent->desc = readElementText();
}

void XbelReader::readSeparator(BookmarkNode *parent)
{
    new BookmarkNode(BookmarkNode::Separator, parent);
    // empty elements have a start and end element
    readNext();
}

void XbelReader::readBookmarkNode(BookmarkNode *parent)
{
    Q_ASSERT(isStartElement() && name() == QStringLiteral("bookmark"));
    auto bookmark = new BookmarkNode(BookmarkNode::Bookmark, parent);
    bookmark->url = attributes().value(QStringLiteral("href")).toString();
    while (readNextStartElement()) {
        if (name() == QStringLiteral("title"))
            readTitle(bookmark);
        else if (name() == QStringLiteral("desc"))
            readDescription(bookmark);
        else
            skipCurrentElement();
    }
    if (bookmark->title.isEmpty())
        bookmark->title = QObject::tr("Unknown title");
}


XbelWriter::XbelWriter()
{
    setAutoFormatting(true);
}

bool XbelWriter::write(const QString &fileName, const BookmarkNode *root)
{
    QFile file(fileName);
    if (!root || !file.open(QFile::WriteOnly))
        return false;
    return write(&file, root);
}

bool XbelWriter::write(QIODevice *device, const BookmarkNode *root)
{
    setDevice(device);

    writeStartDocument();
    writeDTD(QStringLiteral("<!DOCTYPE xbel>"));
    writeStartElement(QStringLiteral("xbel"));
    writeAttribute(QStringLiteral("version"), QStringLiteral("1.0"));
    if (root->type() == BookmarkNode::Root) {
        for (int i = 0; i < root->children().count(); ++i)
            writeItem(root->children().at(i));
    } else {
        writeItem(root);
    }

    writeEndDocument();
    return true;
}

void XbelWriter::writeItem(const BookmarkNode *parent)
{
    switch (parent->type()) {
    case BookmarkNode::Folder:
        writeStartElement(QStringLiteral("folder"));
        writeAttribute(QStringLiteral("folded"), parent->expanded ? QStringLiteral("no") : QStringLiteral("yes"));
        writeTextElement(QStringLiteral("title"), parent->title);
        for (int i = 0; i < parent->children().count(); ++i)
            writeItem(parent->children().at(i));
        writeEndElement();
        break;
    case BookmarkNode::Bookmark:
        writeStartElement(QStringLiteral("bookmark"));
        if (!parent->url.isEmpty())
            writeAttribute(QStringLiteral("href"), parent->url);
        writeTextElement(QStringLiteral("title"), parent->title);
        if (!parent->desc.isEmpty())
            writeAttribute(QStringLiteral("desc"), parent->desc);
        writeEndElement();
        break;
    case BookmarkNode::Separator:
        writeEmptyElement(QStringLiteral("separator"));
        break;
    default:
        break;
    }
}