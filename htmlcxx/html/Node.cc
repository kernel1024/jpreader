#include <QString>
#include <QChar>
#include "Node.h"
#include <QDebug>

using namespace std;
using namespace htmlcxx;
using namespace HTML;

void Node::parseAttributes()
{
    if (!(this->isTag())) return;

    // Rewritten with QString/QChar for UTF-8 only support

    QString lText = mText;

    mAttributes.clear();
    mAttributesOrder.clear();

    if (!lText.contains(u'<')) return;

    QString::Iterator ptr = lText.data();
    QString::Iterator end = ptr;

    // Skip up to tag start
    ptr += lText.indexOf(u'<');

    // Chop opening braces
    ++ptr;

    // Skip initial blankspace
    while (ptr < lText.end() && ptr->isSpace()) ++ptr;

    // Skip tagname
    if (!ptr->isLetter()) return;
    while (ptr < lText.end() && !ptr->isSpace()) ++ptr;

    // Skip blankspace after tagname
    while (ptr < lText.end() && ptr->isSpace()) ++ptr;

    while (ptr < lText.end() && *ptr != u'>')
    {
        QString key;
        QString val;

        // skip unrecognized
        while (ptr < lText.end() && !ptr->isLetterOrNumber() && !ptr->isSpace()) ++ptr;

        // skip blankspace
        while (ptr < lText.end() && ptr->isSpace()) ++ptr;

        end = ptr;
        while (end < lText.end() && (end->isLetterOrNumber() || *end == u'-')) ++end;
        key = QString(ptr, static_cast<int>(end - ptr)).toLower();
        ptr = end;

        // skip blankspace
        while (ptr < lText.end() && ptr->isSpace()) ++ptr;

        if (*ptr == u'=')
        {
            ++ptr;
            while (ptr < lText.end() && ptr->isSpace()) ++ptr;
            if (*ptr == u'"' || *ptr == u'\'')
            {
                QChar quote = *ptr;
                QString::Iterator pptr = ptr;
                pptr++;
                QString tmp(pptr);
                QString::Iterator end = pptr;
                int qidx = tmp.indexOf(quote);
                if (qidx>=0) {
                    end += qidx;
                } else {
                    //b = mText.find_first_of(" >", a+1);
                    int end1 = tmp.indexOf(u' ');
                    int end2 = tmp.indexOf(u'>');
                    if (end1>=0 && end1 < end2) end += end1;
                    else if (end2>=0) end += end2;
                    else return;
                    if (end >= lText.end()) return;
                }
                QString::Iterator begin = ptr + 1;
                while (begin < lText.end() && begin->isSpace() && begin < end) ++begin;
                QString::Iterator trimmed_end = end - 1;
                while (trimmed_end->isSpace() && trimmed_end >= begin) --trimmed_end;
                val = QString(begin, static_cast<int>(trimmed_end - begin + 1));
                ptr = end + 1;
            } else {
                end = ptr;
                while (end < lText.end() && !end->isSpace() && *end != u'>') end++;
                val = QString(ptr, static_cast<int>(end - ptr));
                ptr = end;
            }

//            qDebug() << key << " = " << val;
            mAttributes.insert(key, val);
            mAttributesOrder.append(key);
        } else {
//            qDebug() << "D: " << key;
            mAttributes.insert(key, QString());
            mAttributesOrder.append(key);
        }
    }
}

bool Node::operator==(const Node &rhs) const
{
    if (!isTag() || !rhs.isTag()) return false;
    return (tagName().compare(rhs.tagName(),Qt::CaseInsensitive) == 0);
}

Node::operator QString() const {
	if (isTag()) return this->tagName();
	return this->text();
}

Node::operator std::string() const
{
    if (isTag()) return this->tagName().toStdString();
    return this->text().toStdString();
}

Node::Node()
    : mOffset(0),
    mLength(0),
    mIsHtmlTag(false),
    mComment(false)
{
    mText.clear();
    mClosingText.clear();
    mTagName.clear();
    mAttributes.clear();
    mAttributesOrder.clear();
}

ostream &Node::operator<<(ostream &stream) const {
    stream << std::string(*this);
    return stream;
}
