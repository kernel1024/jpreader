#ifdef WITH_SRCHILITE
#include <srchilite/sourcehighlight.h>
#include <string>
#include <sstream>
#endif

#include "sourceviewer.h"
#include "genericfuncs.h"
#include "ui_sourceviewer.h"

using namespace htmlcxx;

CSourceViewer::CSourceViewer(CSnippetViewer *origin) :
    QDialog(origin->parentWnd),
    ui(new Ui::CSourceViewer)
{
    ui->setupUi(this);

    move(origin->mapToGlobal(origin->pos())+QPoint(50,50));
    resize(5*origin->width()/6,5*origin->height()/6);

    setWindowTitle(QString("Source - %1").arg(origin->getDocTitle()));
    ui->labelTitle->setText(origin->getDocTitle());
    ui->labelUrl->setText(origin->getUrl().toString());

    origin->txtBrowser->page()->toHtml([this](const QString& html) {
        updateSource(html);
    });
}

CSourceViewer::~CSourceViewer()
{
    delete ui;
}

QString CSourceViewer::reformatSource(const QString& html)
{
    HTML::ParserDom parser;
    parser.parse(html);
    tree<HTML::Node> tree = parser.getTree();
    CHTMLNode doc(tree);
    QString dst;
    generateHTML(doc,dst,true,0);
    return dst;
}

void CSourceViewer::updateSource(const QString &src)
{
#ifdef WITH_SRCHILITE
    srchilite::SourceHighlight sourceHighlight;
    std::stringstream sin, sout;
    sin << reformatSource(src).toStdString();
    sourceHighlight.highlight(sin, sout, "html.lang");

    ui->editSource->setHtml(QString::fromStdString(sout.str()));
#else
    QTextCharFormat fmt = ui->editSource->currentCharFormat();
    fmt.setFontFamily("Courier New");
    ui->editSource->setCurrentCharFormat(fmt);
    ui->editSource->setPlainText(src);
#endif
}
