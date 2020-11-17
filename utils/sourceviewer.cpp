#ifdef WITH_SRCHILITE
#include <srchilite/sourcehighlight.h>
#include <string>
#include <sstream>
#endif

#include "sourceviewer.h"
#include "utils/genericfuncs.h"
#include "translator/translator.h"
#include "ui_sourceviewer.h"

CSourceViewer::CSourceViewer(CBrowserTab *origin, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CSourceViewer)
{
    const QPoint windowOffset(50,50);
    const int windowWidthFrac = 80;
    const int windowHeightFrac = 80;
    const int urlElidingCharacterWidth = 90;

    ui->setupUi(this);

    move(origin->mapToGlobal(origin->pos()) + windowOffset);
    resize(windowWidthFrac*origin->width()/100,
           windowHeightFrac*origin->height()/100);

    setWindowTitle(tr("Source - %1").arg(origin->getDocTitle()));
    ui->labelTitle->setText(origin->getDocTitle());

    QString url = origin->getUrl().toString();
    if (url.startsWith(QSL("data")))
        url = QSL("data-url (RFC 2397)");
    url = CGenericFuncs::elideString(url,urlElidingCharacterWidth,Qt::ElideMiddle);
    ui->labelUrl->setText(url);

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
    htmlcxx::HTML::ParserDom parser;
    parser.parse(html);
    tree<htmlcxx::HTML::Node> tree = parser.getTree();
    CHTMLNode doc(tree);
    QString dst;
    CTranslator::generateHTML(doc,dst,true,0);
    return dst;
}

void CSourceViewer::updateSource(const QString &src)
{
#ifdef WITH_SRCHILITE
    srchilite::SourceHighlight sourceHighlight;
    std::stringstream sin;
    std::stringstream sout;
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
