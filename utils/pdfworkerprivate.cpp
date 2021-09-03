#include "pdfworkerprivate.h"
#include <QByteArray>
#include <QFileInfo>
#include <QMessageLogger>
#include <QScopedPointer>
#include <QBuffer>
#include <numeric>

#include "genericfuncs.h"
#include "global/control.h"

#ifdef WITH_POPPLER

extern "C" {
#include <zlib.h>
}
#include <memory>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

#include <poppler-config.h>
#include <GlobalParams.h>
#include <Object.h>
#include <Dict.h>
#include <Catalog.h>
#include <Page.h>
#include <PDFDoc.h>
#include <Error.h>
#include <PDFDocFactory.h>
#include <TextOutputDev.h>
#include <PDFDocEncoding.h>

#pragma GCC diagnostic pop

#include <poppler/cpp/poppler-version.h>
#if POPPLER_VERSION_MAJOR==21
    #if POPPLER_VERSION_MINOR<3
        #define ZPDF_PRE2103_API 1
    #endif
#endif

#endif // WITH_POPPLER

#include <QDebug>

CPDFWorkerPrivate::CPDFWorkerPrivate(QObject *parent)
    : QObject(parent),
      m_pageSeparator(QSL("##JPREADER_NEWPAGE##"))
{

}

#ifdef WITH_POPPLER

void CPDFWorkerPrivate::metaString(QString& out, Dict *infoDict, const char* key,
                const QString& fmt)
{
    Object obj;
    QString res;
    if (static_cast<void>(obj = infoDict->lookup(key)), obj.isString()) {
        const GooString *s1 = obj.getString();
        const QByteArray ba(s1->c_str());
        res = CGenericFuncs::encodeHtmlEntities(
                  CGenericFuncs::detectDecodeToUnicode(ba));
    }
    if (!res.isEmpty())
        out.append(QString(fmt).arg(res));
}

void CPDFWorkerPrivate::metaDate(QString& out, Dict *infoDict, const char* key, const QString& fmt)
{
    Object obj;

    if (static_cast<void>(obj = infoDict->lookup(key)), obj.isString()) {
        QString s = QString::fromUtf8(obj.getString()->c_str());
        if (s.startsWith(QSL("D:")))
            s.remove(0,2);
        out.append(QString(fmt).arg(s));
    }
}

void popplerError(ErrorCategory category, Goffset pos, const char *msg)
{
    static int loggedPopplerErrors = 0;
    const int maxPopplerErrors = 100;

    if (loggedPopplerErrors>maxPopplerErrors) return;
    loggedPopplerErrors++;

    int ipos = static_cast<int>(pos);

    switch (category) {
        case ErrorCategory::errSyntaxError:
            QMessageLogger(nullptr, ipos, nullptr, "Poppler").critical() << "Syntax:" << msg;
            break;
        case ErrorCategory::errSyntaxWarning:
            QMessageLogger(nullptr, ipos, nullptr, "Poppler").warning() << "Syntax:" << msg;
            break;
        case ErrorCategory::errConfig:
            QMessageLogger(nullptr, ipos, nullptr, "Poppler").warning() << "Config:" << msg;
            break;
        case ErrorCategory::errIO:
            QMessageLogger(nullptr, ipos, nullptr, "Poppler").critical() << "IO:" << msg;
            break;
        case ErrorCategory::errNotAllowed:
            QMessageLogger(nullptr, ipos, nullptr, "Poppler").warning() << "Permissions and DRM:" << msg;
            break;
        case ErrorCategory::errUnimplemented:
            QMessageLogger(nullptr, ipos, nullptr, "Poppler").info() << "Unimplemented:" << msg;
            break;
        case ErrorCategory::errInternal:
            QMessageLogger(nullptr, ipos, nullptr, "Poppler").critical() << "Internal:" << msg;
            break;
        case ErrorCategory::errCommandLine:
            QMessageLogger(nullptr, ipos, nullptr, "Poppler").warning() << "Incorrect parameters:" << msg;
            break;
    }
}

void CPDFWorkerPrivate::outputToString(void *stream, const char *text, int len)
{
    static const QString vertForm
            = QSL("\uFE30\uFE31\uFE32\uFE33\uFE34\uFE35\uFE36\uFE37\uFE38\uFE39\uFE3A"
                             "\uFE3B\uFE3C\uFE3D\uFE3E\uFE3F\uFE40\uFE41\uFE42\uFE43\uFE44\uFE45"
                             "\uFE46\uFE47\uFE48\u22EE"
                             "\uFE10\uFE11\uFE12\uFE13\uFE14\uFE15\uFE16\uFE17\uFE18\uFE19");
    // ︰ ︱ ︲ ︳ ︴ ︵ ︶ ︷ ︸ ︹ ︺
    // ︻ ︼ ︽ ︾ ︿ ﹀ ﹁ ﹂ ﹃ ﹄ ﹅
    // ﹆ ﹇ ﹈ ⋮
    // ︐ ︑ ︒ ︓ ︔ ︕ ︖ ︗ ︘ ︙
    static const QString vertFormTr =
            QSL("\u2025\u2014\u2013\u005F\uFE4F\uFF08\uFF09\uFF5B\uFF5D\u3014\u3015"
                           "\u3010\u3011\u300A\u300B\u3008\u3009\u300C\u300D\u300E\u300F\u3002"
                           "\u3002\uFF3B\uFF3D\u2026"
                           "\uFF0C\u3001\u3002\uFF1A\uFF1B\uFF01\uFF1F\u3016\u3017\u2026");

    if (stream==nullptr) return;
    auto *worker = reinterpret_cast<CPDFWorkerPrivate *>(stream);
    QString tx = QString::fromUtf8(text,len);

    for (int i=0;i<tx.length();i++) {
        int vtidx = vertForm.indexOf(tx.at(i)); // replace vertical forms with normal ones
        if (vtidx>=0)
            tx.replace(i,1,vertFormTr.at(vtidx));
    }

    worker->m_text.append(tx);
    worker->m_outLengths.append(tx.length());

    if (tx.length()==1 && (tx.at(0).isLetter() || tx.at(0).isPunct() || tx.at(0).isSymbol())) {
        worker->m_prevblock = true;
    } else {
        if (!worker->m_prevblock) worker->m_text.append(u'\n');
        worker->m_prevblock = false;
    }
}

QString CPDFWorkerPrivate::formatPdfText(const QString& text)
{
    const static QString openingQuotation =
            QSL("\u3008\u300A\u300C\u300E\u3010\u3014\u3016\u3018\u301A\u201C"
                           "\u00AB\u2039\u2018\u0022\u0028\u005B\uFF08\uFF3B");
    // 〈《「『【〔〖〘〚 “ « ‹ ‘ " ( [
    const static QString closingQuotation =
            QSL("\u3009\u300B\u300D\u300F\u3011\u3015\u3017\u3019\u301B\u201D"
                           "\u00BB\u203A\u2019\u0022\u0029\u005D\uFF09\uFF3D");
    //  〉》」』】〕〗〙〛” » › ’ " ) ]
    const static QString endMarkers =
            QSL("\u0021\u002E\u003F\uFF01\uFF0E\uFF1F\u3002\u2026");
    // ! . ? 。…

    const static int maxParagraphLength = 150;
    const double minimalHorizontalLen = 2.0;
    const ushort maxControlChar = 0x1f;

    int sumlen = std::reduce(m_outLengths.constBegin(),m_outLengths.constEnd());

    double avglen = (static_cast<double>(sumlen))/m_outLengths.count();
    bool isVerticalText = (avglen<minimalHorizontalLen);

    QString s = text;

    // replace multi-newlines with paragraph markers
    QRegularExpression exp(QSL("\n{2,}"));
    int pos = 0;
    QRegularExpressionMatch match;
    while (s.indexOf(exp,pos,&match) != -1) {
        int pos = match.capturedStart(0);
        int length = match.capturedLength(0);
        s.remove(pos,length);
        if (pos>0) {
            QChar pm = s.at(pos-1);
            if (!((pm.isLetter() && !isVerticalText) ||
                  (pm.isLetterOrNumber() && isVerticalText) ||
                  (pm.isPunct() && !endMarkers.contains(pm)))) {
                s.insert(pos,QSL("</p><p>"));
            } else if (pm.script()<=QChar::Script_Syriac) { // still insert spaces for common european scripts
                s.insert(pos,u' ');
            }
        }
    }

    // delete remaining newlines
    s.remove(u'\n');

    // replace page separators
    s = s.replace(u'\f',QSL("</p>%1<p>").arg(m_pageSeparator));

    s = QSL("<p>") + s + QSL("</p>");

    int idx = 0;
    while (idx<s.length()) { // remove-replace pass
        QChar c = s.at(idx);

        if (c.unicode()<=maxControlChar) { // remove old control characters
            s.remove(idx,1);
            continue;
        }

        idx++;
    }

    idx = 0;
    int clen = 0;
    bool denyNewline = false;
    while (idx<s.length()) { // formatting pass
        QChar c = s.at(idx);
        QChar pc(0x0000);
        if (idx>0)
            pc = s.at(idx-1);
        QChar nc(0x0000);
        if (idx<s.length()-1)
            nc = s.at(idx+1);

        if (!c.isLetter()) {
            if (openingQuotation.contains(c) && pc.isLetter()) { // inline quotation
                denyNewline = true;
            } else if (openingQuotation.contains(c) && // quotation start...
                    (closingQuotation.contains(pc) || //...after previous similar quotation block
                     endMarkers.contains(pc) || // after sentence or paragraph
                     pc.isPunct())) { // after punctuation separators
                // insert newline before opening quotation
                s.insert(idx,u'\n');
                idx++;
                clen = 0;
            } else if ((closingQuotation.contains(c) && (!denyNewline)) ||
                       (endMarkers.contains(c) && (clen>maxParagraphLength))) {
                // insert newline after closing quotation
                // -- or after long sentences
                s.insert(idx+1,u'\n');
                idx+=2;
                clen = 0;
            } else if (closingQuotation.contains(c) && denyNewline) {
                denyNewline = false;
            } else {
                clen++;
            }
        } else {
            clen++;
        }
        idx++;
    }
    s = s.replace(u'\n',QSL("</p>\n<p>"));

    return s;
}

int CPDFWorkerPrivate::zlibInflate(const char* src, int srcSize, uchar *dst, int dstSize)
{
    z_stream strm;
    int ret = 0;

    strm.zalloc = nullptr;
    strm.zfree = nullptr;
    strm.opaque = nullptr;
    strm.avail_in = static_cast<uint>(srcSize);
    strm.next_in = reinterpret_cast<uchar*>(const_cast<char*>(src));

    ret = inflateInit(&strm);
    if (ret != Z_OK)
        return -1;

    strm.avail_out = static_cast<uint>(dstSize);
    strm.next_out = dst;
    ret = inflate(&strm,Z_NO_FLUSH);
    inflateEnd(&strm);

    if ((ret != Z_OK) && (ret != Z_STREAM_END)) {
        return -2;
    }

    if ((ret == Z_OK) && (strm.avail_out == 0)) {
        return -3;
    }

    return (dstSize - static_cast<int>(strm.avail_out));
}

QString CPDFWorkerPrivate::pdfToText(bool* error, const QString &filename)
{
    // conversion parameters
    const double resolution = 72.0;
    const bool physLayout = false;
    const double fixedPitch = 0;
    const bool rawOrder = true;
    const int bppRGB888 = 8;

    QString result;

    QFileInfo fi(filename);
    GooString fileName(filename.toUtf8().constData());
    Object info;
    int lastPage = 0;

    result.clear();
    m_outLengths.clear();

#ifdef ZPDF_PRE2103_API
    PDFDoc *doc = PDFDocFactory().createPDFDoc(fileName);
#else
    std::unique_ptr<PDFDoc> doc(PDFDocFactory().createPDFDoc(fileName));
#endif

    if (!doc->isOk()) {
#ifdef ZPDF_PRE2103_API
        delete doc;
#endif
        qCritical() << "pdfToText: Cannot create PDF Doc object";
        result = QSL("pdfToText: Cannot create PDF Doc object");
        *error = true;
        return result;
    }

    // write HTML header
    result.append(QSL("<html><head>\n"));
    info = doc->getDocInfo();
    if (info.isDict()) {
        Object obj;
        if (static_cast<void>(obj = info.getDict()->lookup("Title")), obj.isString()) {
            metaString(result, info.getDict(), "Title", QSL("<title>%1</title>\n"));
        } else {
            result.append(QSL("<title>%1</title>\n").arg(fi.baseName()));
        }
        metaString(result, info.getDict(), "Subject",
                   QSL("<meta name=\"Subject\" content=\"%1\"/>\n"));
        metaString(result, info.getDict(), "Keywords",
                   QSL("<meta name=\"Keywords\" content=\"%1\"/>\n"));
        metaString(result, info.getDict(), "Author",
                   QSL("<meta name=\"Author\" content=\"%1\"/>\n"));
        metaString(result, info.getDict(), "Creator",
                   QSL("<meta name=\"Creator\" content=\"%1\"/>\n"));
        metaString(result, info.getDict(), "Producer",
                   QSL("<meta name=\"Producer\" content=\"%1\"/>\n"));
        metaDate(result, info.getDict(), "CreationDate",
                 QSL("<meta name=\"CreationDate\" content=\"%1\"/>\n"));
        metaDate(result, info.getDict(), "LastModifiedDate",
                 QSL("<meta name=\"ModDate\" content=\"%1\"/>\n"));
    }
    result.append(QSL("</head>\n"));
    result.append(QSL("<body>\n"));

    lastPage = doc->getNumPages();

    m_text.clear();

    // write text
    QScopedPointer<TextOutputDev> textOut(new TextOutputDev(&CPDFWorkerPrivate::outputToString,static_cast<void*>(this),
                                physLayout, fixedPitch, rawOrder));
    if (!textOut.isNull() && textOut->isOk()) {
        doc->displayPages(textOut.data(), 1, lastPage, resolution, resolution, 0,
                          true, false, false);
    } else {
#ifdef ZPDF_PRE2103_API
        delete doc;
#endif
        qCritical() << "pdfToText: Cannot create TextOutput object";
        result = QSL("pdfToText: Cannot create TextOutput object");
        *error = true;
        return result;
    }

    QHash<int,QVector<QByteArray> > images;
    if (gSet->settings()->pdfExtractImages) {
        for (int pageNum=1;pageNum<=lastPage;pageNum++) {
            Dict *dict = doc->getPage(pageNum)->getResourceDict();
            if (dict->lookup("XObject").isDict()) {
                Dict *xolist = dict->lookup("XObject").getDict();

                for (int xo_idx=0;xo_idx<xolist->getLength();xo_idx++) {
                    Object stype;
                    Object xitem = xolist->getVal(xo_idx);
                    if (!xitem.isStream()) continue;
                    if (!xitem.streamGetDict()->lookup("Subtype").isName("Image")) continue;

                    QImage img;
                    BaseStream* data = xitem.getStream()->getBaseStream();
                    int size = static_cast<int>(data->getLength());
                    QByteArray ba;
                    ba.resize(size);
                    data->doGetChars(size,reinterpret_cast<unsigned char *>(ba.data()));

                    StreamKind kind = xitem.getStream()->getKind();
                    if (kind==StreamKind::strFlate && // zlib stream
                            xitem.streamGetDict()->lookup("Width").isInt() &&
                            xitem.streamGetDict()->lookup("Height").isInt() &&
                            xitem.streamGetDict()->lookup("BitsPerComponent").isInt()) {
                        int dwidth = xitem.streamGetDict()->lookup("Width").getInt();
                        int dheight = xitem.streamGetDict()->lookup("Height").getInt();
                        int dBPP = xitem.streamGetDict()->lookup("BitsPerComponent").getInt();

                        if (dBPP == bppRGB888) {
                            img = QImage(dwidth,dheight,QImage::Format_RGB888);
                            int sz = zlibInflate(ba.constData(),ba.size(),
                                                 img.bits(),static_cast<int>(img.sizeInBytes()));
                            if (sz<0) {
                                qWarning() << tr("Failed to uncompress page from PDF stream %1 at page %2")
                                              .arg(xo_idx).arg(pageNum);
                            }
                        } else {
                            qWarning() << tr("Unsupported image stream %1 at page %2 (kind = %3, BPP = %4)")
                                          .arg(xo_idx).arg(pageNum).arg(kind).arg(dBPP);
                        }
                    } else if (kind==StreamKind::strDCT) { // JPEG stream
                        img = QImage::fromData(ba);
                    } else {
                        qWarning() << tr("Unsupported image stream %1 at page %2 (kind = %3)")
                                      .arg(xo_idx).arg(pageNum).arg(kind);
                    }
                    ba.clear();

                    if (!img.isNull()) {
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
                        ba.clear();
                        QBuffer buf(&ba);
                        buf.open(QIODevice::WriteOnly);
                        img.save(&buf,"JPEG",gSet->settings()->pdfImageQuality);
                        images[pageNum].append(ba);
                        ba.clear();
                    }
                }
            }
        }
    }

    m_text = formatPdfText(m_text);
    QStringList sltext = m_text.split(m_pageSeparator);
    m_text.clear();
    int idx = 1;
    while (!sltext.isEmpty()) {
        if (idx>1)
            m_text.append(QSL("<hr>"));

        const auto imgList = images.value(idx);
        for (const QByteArray& img : imgList) {
            m_text.append(QSL("<img src=\"data:image/jpeg;base64,"));
            m_text.append(QString::fromLatin1(img.toBase64()));
            m_text.append(QSL("\" />&nbsp;"));
        }
        m_text.append(sltext.takeFirst());
        idx++;
    }


    result.append(m_text);
    result.append(QSL("</body>\n"));
    result.append(QSL("</html>\n"));

#ifdef ZPDF_PRE2103_API
    delete doc;
#endif

    *error = false;
    return result;
}

void CPDFWorkerPrivate::initPdfToText()
{
    const char* textEncoding = "UTF-8";
    globalParams = std::make_unique<GlobalParams>();
    setErrorCallback(&(popplerError));
    globalParams->setTextEncoding(const_cast<char *>(textEncoding));
}

void CPDFWorkerPrivate::freePdfToText()
{
}

#endif // WITH_POPPLER
