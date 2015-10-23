#include <QTextCodec>
#include <QStringList>
#include <QMimeData>
#include <QString>
#include <QTime>
#include <QStandardPaths>
#include <QFileInfo>
#include <QMutex>

#include <iostream>
#include <unistd.h>
#include <magic.h>
#include <unicode/utypes.h>
#include <unicode/localpointer.h>
#include <unicode/uenum.h>
#include <unicode/ucsdet.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include "genericfuncs.h"
#include "globalcontrol.h"

using namespace std;

bool syslogOpened = false;
QMutex loggerMutex;

void stdConsoleOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    loggerMutex.lock();

    QString lmsg = QString();

    int line = context.line;
    QString file = QString(context.file);
    QString category = QString(context.category);
    if (category==QString("default"))
        category.clear();
    else
        category.append(' ');
    int logpri = LOG_NOTICE;

    switch (type) {
        case QtDebugMsg:
            lmsg = QString("%1Debug: %2 (%3:%4)").arg(category).arg(msg).arg(file).arg(line);
            logpri = LOG_DEBUG;
            break;
        case QtWarningMsg:
            lmsg = QString("%1Warning: %2 (%3:%4)").arg(category).arg(msg).arg(file).arg(line);
            logpri = LOG_WARNING;
            break;
        case QtCriticalMsg:
            lmsg = QString("%1Critical: %2 (%3:%4)").arg(category).arg(msg).arg(file).arg(line);
            logpri = LOG_CRIT;
            break;
        case QtFatalMsg:
            lmsg = QString("%1Fatal: %2 (%3:%4)").arg(category).arg(msg).arg(file).arg(line);
            logpri = LOG_ALERT;
            break;
        case QtInfoMsg:
            lmsg = QString("%1Info: %2 (%3:%4)").arg(category).arg(msg).arg(file).arg(line);
            logpri = LOG_INFO;
            break;
    }

    if (!lmsg.isEmpty()) {
        QString fmsg = QTime::currentTime().toString("h:mm:ss") + " "+lmsg;
        debugMessages << fmsg;
        while (debugMessages.count()>5000)
            debugMessages.removeFirst();
        fmsg.append('\n');

        if (qEnvironmentVariableIsSet("QT_LOGGING_TO_CONSOLE"))
            fprintf(stderr, "%s", fmsg.toLocal8Bit().constData());
        else {
            if (!syslogOpened) {
                syslogOpened = true;
                openlog("jpreader", LOG_PID, LOG_USER);
            }
            syslog(logpri, "%s", lmsg.toLocal8Bit().constData());
        }

        if (gSet!=NULL && gSet->logWindow!=NULL)
            QMetaObject::invokeMethod(gSet->logWindow,"updateMessages");
    }

    loggerMutex.unlock();
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
    ucsdet_close(csd);

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

QString getClipboardContent(bool noFormatting, bool plainpre) {
    QString res = "";
    QString cbContents = "";
    QString cbContentsUnformatted = "";

    QClipboard *cb = QApplication::clipboard();
    if (cb->mimeData(QClipboard::Clipboard)->hasHtml()) {
        cbContents = cb->mimeData(QClipboard::Clipboard)->html();
        cbContentsUnformatted = cb->mimeData(QClipboard::Clipboard)->text();
    } else if (cb->mimeData(QClipboard::Clipboard)->hasText()) {
        cbContents = cb->mimeData(QClipboard::Clipboard)->text();
        cbContentsUnformatted = cb->mimeData(QClipboard::Clipboard)->text();
    }

    if (plainpre && !cbContentsUnformatted.isEmpty()) {
        res=cbContentsUnformatted;
        if (res.indexOf("\r\n")>=0)
            res.replace("\r\n","</p><p>");
        else
            res.replace("\n","</p><p>");
        res = "<p>"+res+"</p>";
        res = makeSimpleHtml("<...>",res);
    } else {
        if (noFormatting) {
            if (!cbContentsUnformatted.isEmpty())
                res = cbContentsUnformatted;
        } else {
            if (!cbContents.isEmpty())
                res = cbContents;
            else if (!cbContentsUnformatted.isEmpty())
                res = makeSimpleHtml("<...>",cbContentsUnformatted);
        }
    }
    return res;
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

QString getSaveFileNameD ( QWidget * parent, const QString & caption, const QString & dir, const QString & filter, QString * selectedFilter, QFileDialog::Options options, QString preselectFileName )
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

    if (!preselectFileName.isEmpty())
        d.selectFile(preselectFileName);

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
            "ISO 8859-14" << "cp 1252" << "CP850" << "x-MacRoman");
    enc << (QStringList() << "Central European" << "ISO 8859-2" << "ISO 8859-3" << "cp 1250"
            << "x-MacCentralEurope");
    enc << (QStringList() << "Baltic" << "ISO 8859-4" << "ISO 8859-13" << "cp 1257");
    enc << (QStringList() << "Turkish" << "cp 1254" << "ISO 8859-9" << "x-MacTurkish");
    enc << (QStringList() << "Cyrillic" << "KOI8-R" << "ISO 8859-5" << "cp 1251" << "KOI8-U"
            << "CP866" << "x-MacCyrillic");
    enc << (QStringList() << "Chinese" << "HZ" << "GBK" << "GB18030" << "BIG5" << "BIG5-HKSCS"
            << "ISO-2022-CN" << "ISO-2022-CN-EXT");
    enc << (QStringList() << "Korean" << "CP949" << "ISO-2022-KR");
    enc << (QStringList() << "Japanese" << "EUC-JP" << "SHIFT_JIS" << "ISO-2022-JP"
            << "ISO-2022-JP-2" << "ISO-2022-JP-1");
    enc << (QStringList() << "Greek" << "ISO 8859-7" << "cp 1253" << "x-MacGreek");
    enc << (QStringList() << "Arabic" << "ISO 8859-6" << "cp 1256");
    enc << (QStringList() << "Hebrew" << "ISO 8859-8" << "cp 1255" << "CP862");
    enc << (QStringList() << "Thai" << "CP874");
    enc << (QStringList() << "Vietnamese" << "CP1258");
    enc << (QStringList() << "Unicode" << "UTF-8" << "UTF-7" << "UTF-16" << "UTF-16BE" << "UTF-16LE"
            << "UTF-32" << "UTF-32BE" << "UTF-32LE");
    enc << (QStringList() << "Other" << "windows-1258" << "IBM874" << "TSCII" << "Macintosh");
    return enc;
}

QString bool2str(bool value)
{
    if (value)
        return QString("on");
    else
        return QString("off");
}

QString bool2str2(bool value)
{
    if (value)
        return QString("TRUE");
    else
        return QString("FALSE");
}

QString formatBytes(qint64 sz) {
    QString s;
    float msz;
    if (sz<1024.0) {
        msz=sz;
        s="b";
    } else if (sz<(1024.0 * 1024.0)) {
        msz=sz/1024.0;
        s="Kb";
    } else if (sz<(1024.0 * 1024.0 * 1024.0)) {
        msz=sz/(1024.0*1024.0);
        s="Mb";
    } else if (sz<(1024.0 * 1024.0 * 1024.0 * 1024.0)) {
        msz=sz/(1024.0 * 1024.0 * 1024.0);
        s="Gb";
    } else {
        msz=sz/(1024.0 * 1024.0 * 1024.0 * 1024.0);
        s="Tb";
    }
    s=QString("%1").arg(msz,0,'f',2)+" "+s;
    return s;
}

QString getTmpDir()
{
    QFileInfo fi(QDir::homePath() + QDir::separator() + "tmp");
    if (fi.exists() && fi.isDir() && fi.isWritable())
        return QDir::homePath() + QDir::separator() + "tmp";

    return QStandardPaths::writableLocation(QStandardPaths::TempLocation);
}
