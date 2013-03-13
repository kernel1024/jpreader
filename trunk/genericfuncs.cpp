#include <iostream>
#include <unistd.h>
#include <magic.h>
#include <unicode/utypes.h>
#include <unicode/localpointer.h>
#include <unicode/uenum.h>
#include <unicode/ucsdet.h>
#include "genericfuncs.h"
#include "globalcontrol.h"
#include "SAX/filter/Writer.hpp"
#include "SAX/helpers/CatchErrorHandler.hpp"
#include "taggle/Taggle.hpp"

#ifdef QB_KDEDIALOGS
#include <kfiledialog.h>
#include <kurl.h>
#endif

using namespace std;

QString detectMIME(QString filename)
{
    magic_t myt = magic_open(MAGIC_ERROR|MAGIC_MIME_TYPE);
    magic_load(myt,NULL);
    QByteArray bma = filename.toUtf8();
    const char* bm = bma.data();
    const char* mg = magic_file(myt,bm);
    if (mg==NULL) {
        qDebug() << "libmagic error: " << magic_errno(myt) << QString::fromUtf8(magic_error(myt));
        return QString("text/plain");
    }
    QString mag(mg);
    magic_close(myt);
    return mag;
}

QString detectMIME(QByteArray buf)
{
    magic_t myt = magic_open(MAGIC_ERROR|MAGIC_MIME_TYPE);
    magic_load(myt,NULL);
    const char* mg = magic_buffer(myt,buf.data(),buf.length());
    if (mg==NULL) {
        qDebug() << "libmagic error: " << magic_errno(myt) << QString::fromUtf8(magic_error(myt));
        return QString("text/plain");
    }
    QString mag(mg);
    magic_close(myt);
    return mag;
}

QString detectEncodingName(QByteArray content) {
    QString codepage = "";

	if (!gSet->forcedCharset.isEmpty() && QTextCodec::availableCodecs().contains(gSet->forcedCharset.toAscii()))
		return gSet->forcedCharset;

	QTextCodec* enc = QTextCodec::codecForLocale();
    QByteArray icu_enc = "";
    UErrorCode status = U_ZERO_ERROR;
    UCharsetDetector* csd = ucsdet_open(&status);
    ucsdet_setText(csd, content.constData(), content.length(), &status);
    const UCharsetMatch *ucm;
    ucm = ucsdet_detect(csd, &status);
    if (status==U_ZERO_ERROR && ucm!=NULL) {
        const char* cname = ucsdet_getName(ucm,&status);
        if (status==U_ZERO_ERROR) icu_enc = QByteArray(cname);
    }
    if (QTextCodec::availableCodecs().contains(icu_enc)) {
        enc = QTextCodec::codecForName(icu_enc);
    }

    codepage = QTextCodec::codecForHtml(content,enc)->name();
	if (codepage.contains("x-sjis",Qt::CaseInsensitive)) codepage="SJIS";
    return codepage;
}

QTextCodec* detectEncoding(QByteArray content) {
    QString codepage = detectEncodingName(content);

    if (!codepage.isEmpty())
        return QTextCodec::codecForName(codepage.toAscii());
    else {
        if (!QTextCodec::codecForLocale())
            return QTextCodec::codecForName("UTF-8");
        else
            return QTextCodec::codecForLocale();
    }
}

QString makeSimpleHtml(QString title, QString content)
{
    QString cnt = content.replace(QRegExp("\n{3,}"),"\n\n").replace("\n","<br />\n");
    QString cn="<html><head>";
    cn+="<META HTTP-EQUIV=\"Content-type\" CONTENT=\"text/html; charset=UTF-8;\">";
    cn+="<title>"+title+"</title></head>";
    cn+="<body>"+cnt+"</body></html>";
    return cn;
}

QByteArray XMLizeHTML(QByteArray html, QString encoding)
{
	Arabica::SAX::Taggle<std::string> parser;
    std::ostringstream sink;
    Arabica::SAX::Writer<std::string> writer(sink, 4);
    Arabica::SAX::CatchErrorHandler<std::string> eh;

    writer.setParent(parser);
    writer.setErrorHandler(eh);

    std::istringstream iss(html.data(),std::istringstream::in);
    Arabica::SAX::InputSource<std::string> is;

    std::string enc = encoding.toAscii().constData();
    is.setByteStream(iss);
    is.setEncoding(enc);

    writer.parse(is);

    if(eh.errorsReported())
    {
        QString err(eh.errors().c_str());
        qDebug() << err;
        eh.reset();
    }

    QByteArray xmldoc(sink.str().c_str());
	return xmldoc;
}

QString getClipboardContent(bool noFormatting, bool plainpre) {
    QString s = "";
    if (gSet->lastClipboardContents.isEmpty() && gSet->lastClipboardContentsUnformatted.isEmpty()) {
        QClipboard *cb = QApplication::clipboard();
        if (cb->mimeData(QClipboard::Clipboard)->hasHtml()) {
            gSet->lastClipboardContents = cb->mimeData(QClipboard::Clipboard)->html();
            gSet->lastClipboardContentsUnformatted = cb->mimeData(QClipboard::Clipboard)->text();
            gSet->lastClipboardIsHtml = true;
        } else if (cb->mimeData(QClipboard::Clipboard)->hasText()) {
            gSet->lastClipboardContents = cb->mimeData(QClipboard::Clipboard)->text();
            gSet->lastClipboardContentsUnformatted = cb->mimeData(QClipboard::Clipboard)->text();
            gSet->lastClipboardIsHtml = false;
        }
    }

    if (plainpre && !gSet->lastClipboardContentsUnformatted.isEmpty()) {
        s=gSet->lastClipboardContentsUnformatted;
        if (s.indexOf("\r\n")>=0)
            s.replace("\r\n","</p><p>");
        else
            s.replace("\n","</p><p>");
        s = "<p>"+s+"</p>";
        s = makeSimpleHtml("<...>",s);
    } else {
        if (noFormatting) {
            if (!gSet->lastClipboardContentsUnformatted.isEmpty())
                s = gSet->lastClipboardContentsUnformatted;
        } else {
            if (!gSet->lastClipboardContents.isEmpty())
                s = gSet->lastClipboardContents;
            else if (!gSet->lastClipboardContentsUnformatted.isEmpty())
                s = makeSimpleHtml("<...>",gSet->lastClipboardContentsUnformatted);
        }
    }
    return s;
}

QString fixMetaEncoding(QString data_utf8)
{
    int pos;
    QString header = data_utf8.left(512);
    QString dt = data_utf8;
    dt.remove(0,header.length());
    bool forceMeta = true;
    if ((pos = header.indexOf(QRegExp("http-equiv {0,}=",Qt::CaseInsensitive))) != -1) {
        if ((pos = header.lastIndexOf("<meta ", pos, Qt::CaseInsensitive)) != -1) {
            QRegExp rxcs("charset {0,}=",Qt::CaseInsensitive);
            pos = rxcs.indexIn(header,pos) + rxcs.matchedLength();
            if (pos > -1) {
                int pos2 = header.indexOf(QRegExp("['\"]",Qt::CaseInsensitive), pos+1);
                header.replace(pos, pos2-pos,"UTF-8");
                forceMeta = false;
            }
        }
    }
    if (forceMeta && ((pos = header.indexOf(QRegExp("</head",Qt::CaseInsensitive))) != -1)) {
        header.insert(pos,"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">");
    }
    dt = header+dt;
    return dt;
}

QString wordWrap(const QString &str, int wrapLength)
{
    QStringList stl = str.split(' ');
    QString ret = "";
    int cnt = 0;
    for (int i=0;i<stl.count();i++) {
        ret += stl.at(i) + " ";
        cnt += stl.at(i).length()+1;
        if (cnt>wrapLength) {
            ret += '\n';
            cnt = 0;
        }
    }
    stl.clear();
    return ret;
}

#ifdef QB_KDEDIALOGS
QString getKDEFilters(const QString & qfilter)
{
    QStringList f = qfilter.split(";;",QString::SkipEmptyParts);
    QStringList r;
    r.clear();
    for (int i=0;i<f.count();i++) {
        QString s = f.at(i);
        if (s.indexOf('(')<0) continue;
        QString ftitle = s.left(s.indexOf('(')).trimmed();
        s.remove(0,s.indexOf('(')+1);
        if (s.indexOf(')')<0) continue;
        s = s.left(s.indexOf(')'));
        if (s.isEmpty()) continue;
        QStringList e = s.split(' ');
        for (int j=0;j<e.count();j++) {
            if (r.contains(e.at(j),Qt::CaseInsensitive)) continue;
            if (ftitle.isEmpty())
                r << QString("%1").arg(e.at(j));
            else
                r << QString("%1|%2").arg(e.at(j)).arg(ftitle);
        }
    }

    int idx;
    r.sort();
    idx=r.indexOf(QRegExp("^\\*\\.jpg.*",Qt::CaseInsensitive));
    if (idx>=0) { r.swap(0,idx); }
    idx=r.indexOf(QRegExp("^\\*\\.png.*",Qt::CaseInsensitive));
    if (idx>=0) { r.swap(1,0); r.swap(0,idx); }

    QString sf = r.join("\n");
    return sf;
}
#endif

QString getOpenFileNameD ( QWidget * parent, const QString & caption, const QString & dir, const QString & filter, QString * selectedFilter, QFileDialog::Options options )
{
#ifdef QB_KDEDIALOGS
    UNUSED(selectedFilter);
    UNUSED(options);
    return KFileDialog::getOpenFileName(KUrl(dir),getKDEFilters(filter),parent,caption);
#else
    return QFileDialog::getOpenFileName(parent,caption,dir,filter,selectedFilter,options);
#endif
}

QStringList getOpenFileNamesD ( QWidget * parent, const QString & caption, const QString & dir, const QString & filter, QString * selectedFilter, QFileDialog::Options options )
{
#ifdef QB_KDEDIALOGS
    UNUSED(selectedFilter);
    UNUSED(options);
    return KFileDialog::getOpenFileNames(KUrl(dir),getKDEFilters(filter),parent,caption);
#else
    return QFileDialog::getOpenFileNames(parent,caption,dir,filter,selectedFilter,options);
#endif
}

QString getSaveFileNameD ( QWidget * parent, const QString & caption, const QString & dir, const QString & filter, QString * selectedFilter, QFileDialog::Options options )
{
#ifdef QB_KDEDIALOGS
    UNUSED(selectedFilter);
    UNUSED(options);
    return KFileDialog::getSaveFileName(KUrl(dir),getKDEFilters(filter),parent,caption);
#else
    return QFileDialog::getSaveFileName(parent,caption,dir,filter,selectedFilter,options);
#endif
}

QString	getExistingDirectoryD ( QWidget * parent, const QString & caption, const QString & dir, QFileDialog::Options options )
{
#ifdef QB_KDEDIALOGS
    UNUSED(options);
    return KFileDialog::getExistingDirectory(KUrl(dir),parent,caption);
#else
    return QFileDialog::getExistingDirectory(parent,caption,dir,options);
#endif
}
