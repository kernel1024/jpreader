#ifndef CHTMLPARSER_H
#define CHTMLPARSER_H

#include <QString>
#include <QVector>
#include <QHash>
#include <QStringList>
#include <QUrl>

#include "html/ParserDom.h"

using CHTMLAttributesHash = QHash<QString,QString>;

class CHTMLNode
{
public:
    QString text, tagName, closingText;
    QVector<CHTMLNode> children;
    CHTMLAttributesHash attributes;
    QStringList attributesOrder;
    bool isTag { false };
    bool isComment { false };
    CHTMLNode() = default;
    ~CHTMLNode() = default;
    CHTMLNode(const CHTMLNode& other);
    explicit CHTMLNode(tree<htmlcxx::HTML::Node> const & node);
    explicit CHTMLNode(const QString& innerText);
    CHTMLNode &operator=(const CHTMLNode& other) = default;
    bool operator==(const CHTMLNode &s) const;
    bool operator!=(const CHTMLNode &s) const;
    void normalize();
    bool isTextNode() const;
};

Q_DECLARE_METATYPE(CHTMLNode)

class CHTMLParser
{
public:
    CHTMLParser();

    static const tree<htmlcxx::HTML::Node> &parseHTML(const QString& src);
    static void generateHTML(const CHTMLNode &src, QString &html, bool reformat = false,
                             int depth = 0);
    static void generatePlainText(const CHTMLNode &src, QString &html, int depth = 0);
    static void replaceLocalHrefs(CHTMLNode &node, const QUrl &baseUrl);

};

#endif // CHTMLPARSER_H
