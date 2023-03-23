#include "htmlparser.h"

using namespace htmlcxx;

CHTMLParser::CHTMLParser() = default;

CHTMLNode CHTMLParser::parseHTML(const QString &src)
{
    HTML::ParserDom parser;
    parser.parse(src);
    const CHTMLNode res(parser.getTree());
    return res;
}

void CHTMLParser::generateHTML(const CHTMLNode &src, QString &html, bool reformat, int depth)
{
    if (src.isTag && !src.tagName.isEmpty()) {
        html.append(QSL("<")+src.tagName);
        for (const QString &key : qAsConst(src.attributesOrder)) {
            const QString val = src.attributes.value(key);
            if (!val.contains(u'"')) {
                html.append(QSL(" %1=\"%2\"").arg(key,val));
            } else {
                html.append(QSL(" %1='%2'").arg(key,val));
            }
        }
        html.append(QSL(">"));
    } else {
        html.append(src.text);
    }

    for (const CHTMLNode &node : qAsConst(src.children))
        generateHTML(node,html,reformat,depth+1);

    html.append(src.closingText);
    if (reformat)
        html.append(u'\n');
}

void CHTMLParser::generatePlainText(const CHTMLNode &src, QString &html, int depth)
{
    if (src.isTextNode())
        html.append(QSL("%1\n").arg(src.text));

    for (const CHTMLNode &node : qAsConst(src.children))
        generatePlainText(node,html,depth+1);
}

void CHTMLParser::replaceLocalHrefs(CHTMLNode& node, const QUrl& baseUrl)
{
    if (node.tagName.toLower()==QSL("a")) {
        if (node.attributes.contains(QSL("href"))) {
            const QUrl ref = QUrl(node.attributes.value(QSL("href")));
            if (ref.isRelative())
                node.attributes[QSL("href")]=baseUrl.resolved(ref).toString();
        }
    }

    for (auto &child : node.children) {
        replaceLocalHrefs(child,baseUrl);
    }
}

CHTMLNode::CHTMLNode(tree<HTML::Node> const & node)
{
    tree<HTML::Node>::iterator it = node.begin();

    it->parseAttributes();

    text = it->text();
    tagName = it->tagName();
    closingText = it->closingText();
    isTag = it->isTag();
    isComment = it->isComment();
    attributes = it->attributes();
    attributesOrder = it->attributesOrder();
    children.reserve(static_cast<int>(it.number_of_children()));
    for (unsigned i=0; i<it.number_of_children(); i++ )
        children << CHTMLNode(node.child(it,i));
}

CHTMLNode::CHTMLNode(const QString &innerText)
    : text(innerText),
      tagName(innerText)
{
    // creates text node
}

bool CHTMLNode::operator==(const CHTMLNode &s) const
{
    return (text == s.text);
}

bool CHTMLNode::operator!=(const CHTMLNode &s) const
{
    return !operator==(s);
}

void CHTMLNode::normalize()
{
    // combine sequence of text nodes into one node
    int i = 0;
    while (i<(children.count()-1)) {
        if (children.at(i).isTextNode() &&
                children.at(i+1).isTextNode()) {
            if (!children.at(i+1).text.trimmed().isEmpty())
                children[i].text += children.at(i+1).text;
            children.removeAt(i+1);
            i = 0;
            continue;
        }
        i++;
    }
}

bool CHTMLNode::isTextNode() const
{
    return (!isTag && !isComment);
}
