#include <QBuffer>
#include "htmlimagesextractor.h"
#include "utils/genericfuncs.h"
#include "global/control.h"
#include "global/network.h"
#include "htmlcxx/html/ParserDom.h"

using namespace htmlcxx;

CHtmlImagesExtractor::CHtmlImagesExtractor(QObject *parent, CBrowserTab *snv)
    : CAbstractExtractor(parent,snv)
{
}

void CHtmlImagesExtractor::setParams(const QString &source, const QUrl &origin,
                                     bool translate, bool alternateTranslate, bool focus)
{
    m_translate = translate;
    m_alternateTranslate = alternateTranslate;
    m_focus = focus;
    m_html = source;
    m_origin = origin;
}

QString CHtmlImagesExtractor::workerDescription() const
{
    return tr("HTML images extractor");
}

void CHtmlImagesExtractor::startMain()
{
    HTML::ParserDom parser;
    parser.parse(m_html);

    tree<HTML::Node> tr = parser.getTree();

    m_doc = CHTMLNode(tr);

    examineNode(m_doc);

    handleImages();

    finalizeHtml(); // Just in case
}

void CHtmlImagesExtractor::subImageFinished(QNetworkReply* rpl, CHTMLAttributesHash* attrs)
{
    if (rpl!=nullptr && (rpl->error() == QNetworkReply::NoError)) {
        QByteArray ba = rpl->readAll();
        addLoadedRequest(ba.size());

        QImage img;

        if (img.loadFromData(ba)) {
            ba.clear();
            const char* header = "data:image/jpeg;base64,";
            QByteArray out(header);
            QBuffer buf(&ba);
            buf.open(QIODevice::WriteOnly);

            img.save(&buf,"JPEG",gSet->settings()->pdfImageQuality);
            out.append(ba.toBase64());

            if (out.length() > CDefaults::maxDataUrlFileSize) {
                out = QByteArray(header);
                buf.seek(0);
                ba.clear();

                if (img.width()>img.height()) {
                    if (img.width()>gSet->settings()->pdfImageMaxSize) {
                        img = img.scaledToWidth(gSet->settings()->pdfImageMaxSize,
                                                Qt::SmoothTransformation);
                    }
                } else {
                    if (img.height()>gSet->settings()->pdfImageMaxSize) {
                        img = img.scaledToHeight(gSet->settings()->pdfImageMaxSize,
                                                 Qt::SmoothTransformation);
                    }
                }
                img.save(&buf,"JPEG",gSet->settings()->pdfImageQuality);
                out.append(ba.toBase64());
            }
            (*attrs)[QSL("src")] = QString::fromUtf8(out);
        }
    }
    m_worksImgFetch--;
    finalizeHtml();
}

void CHtmlImagesExtractor::finalizeHtml()
{
    if (m_worksImgFetch>0) return;

    QString res;
    CTranslator::generateHTML(m_doc,res);

    Q_EMIT novelReady(res,m_focus,m_translate,m_alternateTranslate);
    Q_EMIT finished();
}

void CHtmlImagesExtractor::handleImages()
{
    m_worksImgFetch.storeRelease(m_imgUrls.count());
    if (!m_imgUrls.isEmpty()) {
        for(const auto &it : qAsConst(m_imgUrls)) {
            QUrl url((*it).value(QSL("src")).trimmed());
            QMetaObject::invokeMethod(gSet->auxNetworkAccessManager(),[this,url,it]{
                if (exitIfAborted()) return;
                QNetworkRequest req(url);
                req.setRawHeader("referer",m_origin.toString().toUtf8());
                req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::SameOriginRedirectPolicy);
                req.setMaximumRedirectsAllowed(CDefaults::httpMaxRedirects);
                QNetworkReply* rpl = gSet->net()->auxNetworkAccessManagerGet(req);
                connect(rpl,&QNetworkReply::finished,this,[this,rpl,it](){
                    subImageFinished(rpl,it);
                    rpl->deleteLater();
                });
            },Qt::QueuedConnection);
        }
    }
}

void CHtmlImagesExtractor::examineNode(CHTMLNode &node)
{
    if (exitIfAborted()) return;
    QApplication::processEvents();

    const QStringList &acceptedExt = CGenericFuncs::getSupportedImageExtensions();

    if (node.tagName.toLower()==QSL("img")) {
        if (node.attributes.contains(QSL("src"))) {
            node.attributes[QSL("style")]=QSL("display:block;");
            QString src = node.attributes.value(QSL("src")).trimmed();
            if (!src.isEmpty()) {
                QUrl imgUrl(src);
                QFileInfo fi(src);
                if (imgUrl.isValid() &&
                        acceptedExt.contains(fi.suffix(),Qt::CaseInsensitive)) {

                    if (imgUrl.isRelative())
                        imgUrl = m_origin.resolved(imgUrl);

                    m_imgUrls.append(&node.attributes);
                }
            }
        }
    }

    for(auto &child : node.children) {
        examineNode(child);
    }
}

