/**
 * ========================================================================
 *
 * pdftotext.cc
 *
 *  Copyright 1997-2003 Glyph & Cog, LLC
 *
 *  Modified for Debian by Hamish Moffatt, 22 May 2002.
 *
 * ========================================================================
 *
 * ========================================================================
 *
 *  Modified under the Poppler project - http://poppler.freedesktop.org
 *
 *  All changes made under the Poppler project to this file are licensed
 *  under GPL version 2 or later
 *
 * Copyright (C) 2006 Dominic Lachowicz <cinamod@hotmail.com>
 * Copyright (C) 2007-2008, 2010, 2011 Albert Astals Cid <aacid@kde.org>
 * Copyright (C) 2009 Jan Jockusch <jan@jockusch.de>
 * Copyright (C) 2010, 2013 Hib Eris <hib@hiberis.nl>
 * Copyright (C) 2010 Kenneth Berland <ken@hero.com>
 * Copyright (C) 2011 Tom Gleason <tom@buildadam.com>
 * Copyright (C) 2011 Steven Murdoch <Steven.Murdoch@cl.cam.ac.uk>
 * Copyright (C) 2013 Yury G. Kudryashov <urkud.urkud@gmail.com>
 * Copyright (C) 2013 Suzuki Toshiya <mpsuzuki@hiroshima-u.ac.jp>
 * Copyright (C) 2015 Jeremy Echols <jechols@uoregon.edu>
 *
 * To see a description of the changes please see the Changelog file that
 * came with your tarball or type make ChangeLog if you are building from git
 *
 * Adaptation for JPReader by kernel1024
 *
 * ========================================================================*/

#include "pdftotext.h"
#include "genericfuncs.h"
#include "structures.h"
#include <QByteArray>
#include <QMutex>
#include <QDebug>

#ifdef WITH_POPPLER

#include <poppler-config.h>
#include <poppler-version.h>
#include <GlobalParams.h>
#include <Object.h>
#include <Dict.h>
#include <Catalog.h>
#include <Page.h>
#include <PDFDoc.h>
#include <Error.h>
#include <PDFDocFactory.h>
#include <TextOutputDev.h>
#include <UnicodeMap.h>
#include <PDFDocEncoding.h>
#include <QMessageLogger>

#if POPPLER_VERSION_MAJOR==0
    #if POPPLER_VERSION_MINOR<58
        #define JPDF_PRE058_OBJECT_API 1
    #endif
#endif

void metaString(QString& out, Dict *infoDict, const char* key,
                const QString& fmt)
{
    Object obj;
    QString res;
#ifdef JPDF_PRE058_OBJECT_API
    if (infoDict->lookup(key, &obj)->isString()) {
#else
    if (static_cast<void>(obj = infoDict->lookup(key)), obj.isString()) {
#endif
        const GooString *s1 = obj.getString();
        QByteArray ba(s1->getCString());
        res = detectDecodeToUnicode(ba);
        res.replace("&",  "&amp;");
        res.replace("'",  "&apos;" );
        res.replace("\"", "&quot;" );
        res.replace("<",  "&lt;" );
        res.replace(">",  "&gt;" );
    }
    if (!res.isEmpty())
        out.append(QString(fmt).arg(res));

#ifdef JPDF_PRE058_OBJECT_API
    obj.free();
#else
    obj.setToNull();
#endif
}

void metaDate(QString& out, Dict *infoDict, const char* key, const QString& fmt)
{
    Object obj;

#ifdef JPDF_PRE058_OBJECT_API
    if (infoDict->lookup(key, &obj)->isString()) {
#else
    if (static_cast<void>(obj = infoDict->lookup(key)), obj.isString()) {
#endif
        const char *s = obj.getString()->getCString();
        if (s[0] == 'D' && s[1] == ':') {
            s += 2;
        }
        out.append(QString(fmt).arg(s));
    }

#ifdef JPDF_PRE058_OBJECT_API
    obj.free();
#else
    obj.setToNull();
#endif
}

static QIntList outLengths;
static QMutex pdfInterlock;

static void outputToString(void *stream, const char *text, int len)
{
    static bool prevblock = false;
    const static QString vertForm = "\uFE30\uFE31\uFE32\uFE33\uFE34\uFE35\uFE36\uFE37\uFE38\uFE39\uFE3A"
                                    "\uFE3B\uFE3C\uFE3D\uFE3E\uFE3F\uFE40\uFE41\uFE42\uFE43\uFE44\uFE45"
                                    "\uFE46\uFE47\uFE48\u22EE"
                                    "\uFE10\uFE11\uFE12\uFE13\uFE14\uFE15\uFE16\uFE17\uFE18\uFE19";
    // ︰ ︱ ︲ ︳ ︴ ︵ ︶ ︷ ︸ ︹ ︺
    // ︻ ︼ ︽ ︾ ︿ ﹀ ﹁ ﹂ ﹃ ﹄ ﹅
    // ﹆ ﹇ ﹈ ⋮
    // ︐ ︑ ︒ ︓ ︔ ︕ ︖ ︗ ︘ ︙
    const static QString vertFormTr = "\u2025\u2014\u2013\u005F\uFE4F\uFF08\uFF09\uFF5B\uFF5D\u3014\u3015"
                                      "\u3010\u3011\u300A\u300B\u3008\u3009\u300C\u300D\u300E\u300F\u3002"
                                      "\u3002\uFF3B\uFF3D\u2026"
                                      "\uFF0C\u3001\u3002\uFF1A\uFF1B\uFF01\uFF1F\u3016\u3017\u2026";

    if (stream==nullptr) return;
    QString* str = reinterpret_cast<QString *>(stream);
    QString tx = QString::fromUtf8(text,len);

    for (int i=0;i<tx.length();i++) {
        int vtidx = vertForm.indexOf(tx.at(i)); // replace vertical forms with normal ones
        if (vtidx>=0)
            tx.replace(i,1,vertFormTr.at(vtidx));
    }

    str->append(tx);
    outLengths.append(tx.length());

    if (tx.length()==1 && (tx.at(0).isLetter() || tx.at(0).isPunct() || tx.at(0).isSymbol())) {
        prevblock = true;
    } else {
        if (!prevblock) str->append("\n");
        prevblock = false;
    }
}

QString formatPdfText(const QString& text)
{
    const static QString openingQuotation = "\u3008\u300A\u300C\u300E\u3010\u3014\u3016\u3018\u301A\u201C"
                                            "\u00AB\u2039\u2018\u0022\u0028\u005B\uFF08\uFF3B";
    // 〈《「『【〔〖〘〚 “ « ‹ ‘ " ( [
    const static QString closingQuotation = "\u3009\u300B\u300D\u300F\u3011\u3015\u3017\u3019\u301B\u201D"
                                            "\u00BB\u203A\u2019\u0022\u0029\u005D\uFF09\uFF3D";
    //  〉》」』】〕〗〙〛” » › ’ " ) ]
    const static QString endMarkers = "\u0021\u002E\u003F\uFF01\uFF0E\uFF1F\u3002\u2026";
    // ! . ? 。…

    const static int maxParagraphLength = 150;

    int sumlen = 0;
    for (int i=0;i<outLengths.count();i++)
        sumlen += outLengths.at(i);
    double avglen = (double)sumlen/outLengths.count();
    bool isVerticalText = (avglen<2.0);

    QString s = text;

    // replace multi-newlines with paragraph markers
    QRegExp exp("\n{2,}");
    int pos = 0;
    while ((pos=exp.indexIn(s,pos)) != -1) {
        int length = exp.matchedLength();
        s.remove(pos,length);
        if (pos>0) {
            QChar pm = s.at(pos-1);
            if (!((pm.isLetter() && !isVerticalText) ||
                  (pm.isLetterOrNumber() && isVerticalText) ||
                  (pm.isPunct() && !endMarkers.contains(pm))))
                s.insert(pos,"</p><p>");
        }
    }


    // delete remaining newlines
    s.remove('\n');

    // replace page separators
    s = s.replace('\f',"</p><hr><p>");

    s = "<p>" + s + "</p>";

    int idx = 0;
    while (idx<s.length()) { // remove-replace pass
        QChar c = s.at(idx);

        if (c.unicode()<=0x1f) { // remove old control characters
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
                s.insert(idx,'\n');
                idx++;
                clen = 0;
            } else if (closingQuotation.contains(c) && (!denyNewline)) {
                //&& (nc.isLetter() || openingQuotation.contains(nc))) {
                // insert newline after closing quotation
                s.insert(idx+1,'\n');
                idx+=2;
                clen = 0;
            } else if (closingQuotation.contains(c) && denyNewline) {
                denyNewline = false;
            } else if (endMarkers.contains(c) && (clen>maxParagraphLength)) {
                // insert newline after long sentences
                s.insert(idx+1,'\n');
                idx+=2;
                clen = 0;
            } else
                clen++;
        } else
            clen++;
        idx++;
    }
    s = s.replace('\n',"</p>\n<p>");

    return s;
}

bool pdfToText(const QUrl& pdf, QString& result)
{
    // conversion parameters
    static int lastPage = 0;
    static double resolution = 72.0;
    static GBool physLayout = gFalse;
    static double fixedPitch = 0;
    static GBool rawOrder = gTrue;
    static char textEncoding[] = "UTF-8";

    PDFDoc *doc;
    GooString fileName(pdf.toString().toUtf8());
    TextOutputDev *textOut;
    UnicodeMap *uMap;
    Object info;

    pdfInterlock.lock();

    result.clear();
    outLengths.clear();

    globalParams->setTextEncoding(textEncoding);

    // get mapping to output encoding
    if (!(uMap = globalParams->getTextEncoding())) {
        qCritical() << "pdfToText: Couldn't get text encoding";
        pdfInterlock.unlock();
        return false;
    }

    doc = PDFDocFactory().createPDFDoc(fileName);

    if (!doc->isOk()) {
        delete doc;
        uMap->decRefCnt();
        qCritical() << "pdfToText: Cannot create PDF Doc object";
        pdfInterlock.unlock();
        return false;
    }

    // write HTML header
    result.append("<html><head>\n");
#ifdef JPDF_PRE058_OBJECT_API
    doc->getDocInfo(&info);
#else
    info = doc->getDocInfo();
#endif
    if (info.isDict()) {
        Object obj;
#ifdef JPDF_PRE058_OBJECT_API
        if (info.getDict()->lookup("Title", &obj)->isString()) {
#else
        if (static_cast<void>(obj = info.getDict()->lookup("Title")), obj.isString()) {
#endif
            metaString(result, info.getDict(), "Title", "<title>%1</title>\n");
        } else {
            result.append("<title></title>\n");
        }
#ifdef JPDF_PRE058_OBJECT_API
        obj.free();
#else
        obj.setToNull();
#endif
        metaString(result, info.getDict(), "Subject",
                   "<meta name=\"Subject\" content=\"%1\"/>\n");
        metaString(result, info.getDict(), "Keywords",
                   "<meta name=\"Keywords\" content=\"%1\"/>\n");
        metaString(result, info.getDict(), "Author",
                   "<meta name=\"Author\" content=\"%1\"/>\n");
        metaString(result, info.getDict(), "Creator",
                   "<meta name=\"Creator\" content=\"%1\"/>\n");
        metaString(result, info.getDict(), "Producer",
                   "<meta name=\"Producer\" content=\"%1\"/>\n");
        metaDate(result, info.getDict(), "CreationDate",
                 "<meta name=\"CreationDate\" content=\"%1\"/>\n");
        metaDate(result, info.getDict(), "LastModifiedDate",
                 "<meta name=\"ModDate\" content=\"%1\"/>\n");
    }
#ifdef JPDF_PRE058_OBJECT_API
    info.free();
#else
    info.setToNull();
#endif
    result.append("</head>\n");
    result.append("<body>\n");

    lastPage = doc->getNumPages();

    QString text;

    // write text
    textOut = new TextOutputDev(&outputToString,static_cast<void*>(&text),
                                physLayout, fixedPitch, rawOrder);
    if (textOut->isOk()) {
        doc->displayPages(textOut, 1, lastPage, resolution, resolution, 0,
                          gTrue, gFalse, gFalse);
    } else {
        delete textOut;
        delete doc;
        uMap->decRefCnt();
        qCritical() << "pdfToText: Cannot create TextOutput object";
        pdfInterlock.unlock();
        return false;
    }

    delete textOut;

    result.append(formatPdfText(text));
    result.append("</body>\n");
    result.append("</html>\n");

    delete doc;
    uMap->decRefCnt();

    pdfInterlock.unlock();
    return true;
}

static int loggedPopplerErrors = 0;

static void popplerError(void *data, ErrorCategory category, Goffset pos, char *msg)
{
    Q_UNUSED(data);

    if (loggedPopplerErrors>100) return;
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

void initPdfToText()
{
    globalParams = new GlobalParams();
    setErrorCallback(&popplerError,nullptr);
}

void freePdfToText()
{
    delete globalParams;
}

#else // WITH_POPPLER

void initPdfToText()
{
}

void freePdfToText()
{
}

bool pdfToText(const QUrl& pdf, QString& result)
{
    result.clear();
    qCritical() << "pdfToText unavailable, compiled without poppler support.";
    return false;
}

#endif // WITH_POPPLER
