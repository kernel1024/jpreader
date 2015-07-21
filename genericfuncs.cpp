#include <QTextCodec>
#include <QStringList>
#include <QMimeData>
#include <QMutex>
#include <QString>
#include <QTime>

#include <iostream>
#include <unistd.h>
#include <magic.h>
#include <unicode/utypes.h>
#include <unicode/localpointer.h>
#include <unicode/uenum.h>
#include <unicode/ucsdet.h>
#include <stdio.h>
#include <stdlib.h>
#include "genericfuncs.h"
#include "globalcontrol.h"
#include "SAX/filter/Writer.hpp"
#include "SAX/helpers/CatchErrorHandler.hpp"
#include "taggle/Taggle.hpp"

using namespace std;

void stdConsoleOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QString lmsg = QString();

    int line = 0;
    const char *file = "<unsupported>";
    line = context.line;
    file = context.file;

    switch (type) {
        case QtDebugMsg:
            lmsg = QString("Debug: %1 (%2:%3)").arg(msg).arg(file).arg(line);
            break;
        case QtWarningMsg:
            lmsg = QString("Warning: %1 (%2:%3)").arg(msg).arg(file).arg(line);
            break;
        case QtCriticalMsg:
            lmsg = QString("Critical: %1 (%2:%3)").arg(msg).arg(file).arg(line);
            break;
        case QtFatalMsg:
            lmsg = QString("Fatal: %1 (%2:%3)").arg(msg).arg(file).arg(line);
            break;
        case QtInfoMsg:
            lmsg = QString("Info: %1 (%2:%3)").arg(msg).arg(file).arg(line);
            break;
    }

    if (!lmsg.isEmpty()) {
        lmsg = QTime::currentTime().toString("h:mm:ss") + " "+lmsg;
        debugMessages << lmsg;
        while (debugMessages.count()>5000)
            debugMessages.removeFirst();
        lmsg.append('\n');
        fprintf(stderr, lmsg.toLocal8Bit().constData());

        if (gSet!=NULL && gSet->logWindow!=NULL)
            QMetaObject::invokeMethod(gSet->logWindow,"updateMessages");
    }
}

QString detectMIME(QString filename)
{
    magic_t myt = magic_open(MAGIC_ERROR|MAGIC_MIME_TYPE);
    magic_load(myt,NULL);
    QByteArray bma = filename.toUtf8();
    const char* bm = bma.data();
    const char* mg = magic_file(myt,bm);
    if (mg==NULL) {
        qCritical() << "libmagic error: " << magic_errno(myt) << QString::fromUtf8(magic_error(myt));
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
        qCritical() << "libmagic error: " << magic_errno(myt) << QString::fromUtf8(magic_error(myt));
        return QString("text/plain");
    }
    QString mag(mg);
    magic_close(myt);
    return mag;
}

QString detectEncodingName(QByteArray content) {
    QString codepage = "";

    if (!gSet->forcedCharset.isEmpty() &&
            QTextCodec::availableCodecs().contains(gSet->forcedCharset.toLatin1()))
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
        return QTextCodec::codecForName(codepage.toLatin1());
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

QMutex xmlizerMutex;

QByteArray XMLizeHTML(QByteArray html, QString encoding)
{
    xmlizerMutex.lock();
	Arabica::SAX::Taggle<std::string> parser;
    std::ostringstream sink;
    Arabica::SAX::Writer<std::string> writer(sink, 0);
    Arabica::SAX::CatchErrorHandler<std::string> eh;

    writer.setParent(parser);
    writer.setErrorHandler(eh);

    std::istringstream iss(html.data(),std::istringstream::in);
    Arabica::SAX::InputSource<std::string> is;

    std::string enc = encoding.toLatin1().constData();
    is.setByteStream(iss);
    is.setEncoding(enc);

    writer.parse(is);

    if(eh.errorsReported())
    {
        QString err(eh.errors().c_str());
        qCritical() << err;
        eh.reset();
    }

    QByteArray xmldoc(sink.str().c_str());
    xmlizerMutex.unlock();
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

QString getOpenFileNameD ( QWidget * parent, const QString & caption, const QString & dir, const QString & filter, QString * selectedFilter, QFileDialog::Options options )
{
    return QFileDialog::getOpenFileName(parent,caption,dir,filter,selectedFilter,options);
}

QStringList getOpenFileNamesD ( QWidget * parent, const QString & caption, const QString & dir, const QString & filter, QString * selectedFilter, QFileDialog::Options options )
{
    return QFileDialog::getOpenFileNames(parent,caption,dir,filter,selectedFilter,options);
}

QStringList getSuffixesFromFilter(const QString& filter)
{
    QStringList res;
    res.clear();
    if (filter.isEmpty()) return res;

    res = filter.split(";;",QString::SkipEmptyParts);
    if (res.isEmpty()) return res;
    QString ex = res.first();
    res.clear();

    if (ex.isEmpty()) return res;

    ex.remove(QRegExp("^.*\\("));
    ex.remove(QRegExp("\\).*$"));
    ex.remove(QRegExp("^.*\\."));
    res = ex.split(" ");

    return res;
}

QString getSaveFileNameD ( QWidget * parent, const QString & caption, const QString & dir, const QString & filter, QString * selectedFilter, QFileDialog::Options options )
{
    QFileDialog d(parent,caption,dir,filter);
    d.setFileMode(QFileDialog::AnyFile);
    d.setOptions(options);
    d.setAcceptMode(QFileDialog::AcceptSave);

    QStringList sl;
    if (selectedFilter!=NULL && !selectedFilter->isEmpty())
        sl=getSuffixesFromFilter(*selectedFilter);
    else
        sl=getSuffixesFromFilter(filter);
    if (!sl.isEmpty())
        d.setDefaultSuffix(sl.first());

    if (selectedFilter && !selectedFilter->isEmpty())
            d.selectNameFilter(*selectedFilter);
    if (d.exec()==QDialog::Accepted) {
        if (selectedFilter!=NULL)
            *selectedFilter=d.selectedNameFilter();
        if (!d.selectedFiles().isEmpty())
            return d.selectedFiles().first();
        else
            return QString();
    } else
        return QString();
}

QString	getExistingDirectoryD ( QWidget * parent, const QString & caption, const QString & dir, QFileDialog::Options options )
{
    return QFileDialog::getExistingDirectory(parent,caption,dir,options);
}

QList<QStringList> encodingsByScript()
{
    QList<QStringList> enc;
    enc << (QStringList() << "Western European" << "ISO 8859-1" << "ISO 8859-15" <<
            "ISO 8859-14" << "cp 1252" << "IBM850");
    enc << (QStringList() << "Central European" << "ISO 8859-2" << "ISO 8859-3" << "cp 1250");
    enc << (QStringList() << "Baltic" << "ISO 8859-4" << "ISO 8859-13" << "cp 1257");
    enc << (QStringList() << "Turkish" << "cp 1254" << "ISO 8859-9");
    enc << (QStringList() << "Cyrillic" << "KOI8-R" << "ISO 8859-5" << "cp 1251" << "KOI8-U" << "IBM866");
    enc << (QStringList() << "Chinese Traditional" << "Big5" << "Big5-HKSCS");
    enc << (QStringList() << "Chinese Simplified" << "GB18030" << "GBK" << "GB2312");
    enc << (QStringList() << "Korean" << "EUC-KR");
    enc << (QStringList() << "Japanese" << "sjis" << "jis7" << "EUC-JP");
    enc << (QStringList() << "Greek" << "ISO 8859-7" << "cp 1253");
    enc << (QStringList() << "Arabic" << "ISO 8859-6" << "cp 1256");
    enc << (QStringList() << "Hebrew" << "ISO 8859-8" << "ISO 8859-8-I" << "cp 1255");
    enc << (QStringList() << "Thai" << "TIS620" << "ISO 8859-11");
    enc << (QStringList() << "Unicode" << "UTF-8" << "UTF-16" << "utf7" << "ucs2" << "ISO 10646-UCS-2");
    enc << (QStringList() << "Northern Saami" << "winsami2");
    enc << (QStringList() << "Other" << "windows-1258" << "IBM874" << "TSCII");
    return enc;
}

QString bool2str(bool value)
{
    if (value)
        return QString("on");
    else
        return QString("off");
}
