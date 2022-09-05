#include <QStringList>
#include <QMimeData>
#include <QString>
#include <QTime>
#include <QStandardPaths>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QLocale>
#include <QCryptographicHash>
#include <QtTest>

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include "genericfuncs.h"
#include "global/control.h"
#include "global/control_p.h"
#include "global/ui.h"

#include <unicode/utypes.h>
#include <unicode/localpointer.h>
#include <unicode/uenum.h>
#include <unicode/ucsdet.h>
#include <unicode/ucnv.h>

extern "C" {
#include <unistd.h>
#include <magic.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
}

CGenericFuncs::CGenericFuncs(QObject *parent)
    : QObject(parent)
{
}

void CGenericFuncs::profiler(int line, const QElapsedTimer& time)
{
    qInfo() << QSL("Profiling line #%1, ms: %2").arg(line).arg(time.elapsed());
}


bool CGenericFuncs::checkAndUnpackUrl(QUrl& url)
{
    // Extract jump url for pixiv
    if (url.host().endsWith(QSL("pixiv.net")) && url.path().startsWith(QSL("/jump"))) {
        QUrl ju(url.query(QUrl::FullyDecoded));
        if (ju.isValid()) {
            url = ju;
            return true;
        }
    }

    // Clean UTM markers
    if (url.hasQuery()) {
        QUrlQuery qr(url);
        if (qr.hasQueryItem(QSL("utm_source"))) {
            if (url.hasFragment())
                url.setFragment(QString());
            url.setQuery(QString());
        }
        return true;
    }

    // Url not modified
    return false;
}

QString CGenericFuncs::detectMIME(const QString &filename)
{
    magic_t myt = magic_open(MAGIC_ERROR|MAGIC_MIME_TYPE); // NOLINT
    magic_load(myt,nullptr);
    QByteArray bma = filename.toUtf8();
    const char* bm = bma.data();
    const char* mg = magic_file(myt,bm);
    if (mg==nullptr) {
        qCritical() << "libmagic error: " << magic_errno(myt) << QString::fromUtf8(magic_error(myt));
        return QSL("text/plain");
    }
    QString mag = QString::fromLatin1(mg);
    magic_close(myt);
    return mag;
}

QString CGenericFuncs::detectMIME(const QByteArray &buf)
{
    magic_t myt = magic_open(MAGIC_ERROR|MAGIC_MIME_TYPE); // NOLINT
    magic_load(myt,nullptr);
    const char* mg = magic_buffer(myt,buf.data(),static_cast<size_t>(buf.length()));
    if (mg==nullptr) {
        qCritical() << "libmagic error: " << magic_errno(myt) << QString::fromUtf8(magic_error(myt));
        return QSL("text/plain");
    }
    QString mag = QString::fromLatin1(mg);
    magic_close(myt);
    return mag;
}

QString CGenericFuncs::detectEncodingName(const QByteArray& content)
{
    QString res;
    QByteArray icu_enc;
    UErrorCode status = U_ZERO_ERROR;
    UCharsetDetector* csd = ucsdet_open(&status);
    ucsdet_setText(csd, content.constData(), content.length(), &status);
    const UCharsetMatch *ucm = ucsdet_detect(csd, &status);
    if (U_SUCCESS(status) && (ucm != nullptr)) {
        const char* cname = ucsdet_getName(ucm,&status);
        if (U_SUCCESS(status))
            icu_enc = QByteArray(cname);
    }
    if (!icu_enc.isEmpty())
        res = QString::fromUtf8(icu_enc);
    ucsdet_close(csd);

    if (res.contains(QSL("x-sjis"),Qt::CaseInsensitive))
        res = QSL("SJIS");

    return res;
}

QString CGenericFuncs::detectDecodeToUnicode(const QByteArray& content)
{
    if (content.isEmpty())
        return QString();

    UErrorCode status = U_ZERO_ERROR;
    UConverter *conv = nullptr;

    auto cleanup = qScopeGuard([&conv]{
        if (conv)
            ucnv_close(conv);
    });

    const QString encoding = detectEncodingName(content);
    if (encoding.isEmpty())
        return QString::fromUtf8(content); // fallback

    conv = ucnv_open(encoding.toUtf8().constData(),&status);
    if (!U_SUCCESS(status) && (conv == nullptr))
        return QString::fromUtf8(content);

    std::vector<UChar> targetBuf(content.length() / static_cast<uint8_t>(ucnv_getMinCharSize(conv)));
    int len = ucnv_toUChars(conv,targetBuf.data(),targetBuf.size(),content.constData(),content.length(),&status);
    if (!U_SUCCESS(status))
        return QString::fromUtf8(content);

    return QString::fromUtf16(targetBuf.data(),len);
}

QByteArray CGenericFuncs::detectDecodeToUtf8(const QByteArray& content)
{
    if (content.isEmpty())
        return QByteArray();

    UErrorCode status = U_ZERO_ERROR;
    UConverter *conv = nullptr;

    auto cleanup = qScopeGuard([&conv]{
        if (conv)
            ucnv_close(conv);
    });

    const QString encoding = detectEncodingName(content);
    if (encoding.toLower() == QSL("utf-8"))
        return content;

    if (encoding.isEmpty())
        return QByteArray();

    conv = ucnv_open(encoding.toUtf8().constData(),&status);
    if (!U_SUCCESS(status) && (conv == nullptr))
        return QByteArray();

    int utf16len = content.length() / static_cast<uint8_t>(ucnv_getMinCharSize(conv));
    QByteArray targetBuf(utf16len*4,'\0');
    int len = ucnv_fromAlgorithmic(conv,UCNV_UTF8,targetBuf.data(),targetBuf.size(),content.constData(),content.length(),&status);
    if (!U_SUCCESS(status))
        return QByteArray();

    targetBuf.truncate(len);

    return targetBuf;
}

QList<QByteArray> CGenericFuncs::availableICUCodecs()
{
    QList<QByteArray> codecs;
    int n = ucnv_countAvailable();
    for (int i = 0; i < n; ++i) {
        const char *name = ucnv_getAvailableName(i);
        UErrorCode error = U_ZERO_ERROR;
        const char *standardName = ucnv_getStandardName(name, "MIME", &error);
        if (U_FAILURE(error) || !standardName) {
            error = U_ZERO_ERROR;
            standardName = ucnv_getStandardName(name, "IANA", &error);
        }
        if (U_FAILURE(error))
            continue;
        error = U_ZERO_ERROR;
        int ac = ucnv_countAliases(standardName, &error);
        if (U_FAILURE(error))
            continue;
        for (int j = 0; j < ac; ++j) {
            error = U_ZERO_ERROR;
            const char *alias = ucnv_getAlias(standardName, j, &error);
            if (!U_SUCCESS(error))
                continue;
            codecs += alias;
        }
    }
    return codecs;
}

bool CGenericFuncs::codecIsValid(const QByteArray& name)
{
    const QList<QByteArray> list = CGenericFuncs::availableICUCodecs();
    const QString baName = QString::fromUtf8(name).toLower();
    return std::any_of(list.constBegin(),list.constEnd(),[name,baName](const QByteArray& codec){
        return (QString::fromUtf8(codec).toLower() == baName);
    });
}

bool CGenericFuncs::codecIsValid(const QString& name)
{
    return codecIsValid(name.toUtf8());
}

QString CGenericFuncs::unsplitMobileText(const QString& text)
{
    int idx = 0;
    QString buf = text;
    while ((idx = buf.indexOf(QSL("\n"),idx+1)) > 0) {
        if ((idx+1) >= buf.length()) break;
        if (buf.at(idx-1).isLetter() &&
                buf.at(idx+1).isLetter()) {
            buf.remove(idx,1);
        }
    }

    return buf;
}

QString CGenericFuncs::makeSafeFilename(const QString &text)
{
    QString res = text;
    static const QRegularExpression specialChars(QSL("[\\\\/:\\*\\?<>\\|\"]"));
    res.replace(specialChars,QSL(" "));
    return res.simplified();
}

QString CGenericFuncs::encodeHtmlEntities(const QString &text)
{
    QString res = text;
    res.replace(QSL("&"),  QSL("&amp;"));
    res.replace(QSL("'"),  QSL("&apos;" ));
    res.replace(QSL("\""), QSL("&quot;" ));
    res.replace(QSL("<"),  QSL("&lt;" ));
    res.replace(QSL(">"),  QSL("&gt;" ));
    return res;
}

QString CGenericFuncs::decodeHtmlEntities(const QString &text)
{
    QString res = text;
    res.replace(QSL("&amp;"),   QSL("&"));
    res.replace(QSL("&apos;" ), QSL("'"));
    res.replace(QSL("&quot;" ), QSL("\""));
    res.replace(QSL("&lt;" ),   QSL("<"));
    res.replace(QSL("&gt;" ),   QSL(">"));
    return res;
}

int CGenericFuncs::getHttpStatusFromReply(QNetworkReply *reply)
{
    int httpStatus = CDefaults::httpCodeClientUnknownError;
    QVariant vstat = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    if (vstat.isValid()) {
        bool ok = false;
        httpStatus = vstat.toInt(&ok);
    }

    return httpStatus;
}

QString CGenericFuncs::makeSimpleHtml(const QString &title, const QString &content,
                                      bool integratedTitle, const QUrl& origin)
{
    QString s = unsplitMobileText(content);
    static const QRegularExpression multiNewline(QSL("\n{3,}"));
    QString cnt = s.replace(multiNewline,QSL("\n\n"))
                  .replace(QSL("\n"),QSL("<br />\n"));
    QString cn(QSL("<html><head><META HTTP-EQUIV=\"Content-type\" "
                              "CONTENT=\"text/html; charset=UTF-8;\">"));
    cn.append(QSL("<title>%1</title>").arg(title));
    cn.append(QSL("<style> body { font-family:\"%1\"; } </style>").arg(gSet->settings()->fontFixed));
    cn.append(QSL("</head><body>"));
    if (integratedTitle) {
        cn.append(QSL("<h3>"));
        if (!origin.isEmpty()) {
            cn.append(QSL("<a href=\"%1\">%2</a>")
                      .arg(origin.toString(QUrl::FullyEncoded),title));
        }
        cn.append(QSL("</h3>"));
    }
    cn.append(cnt);
    cn.append(QSL("<br/><br/><span style='font-size:smaller;' id='jpreader_translator_desc'></span>"));
    cn.append(QSL("</body></html>"));
    return cn;
}

QString CGenericFuncs::makeSpecialUrlsHtml()
{
    static const QStringList internalUrls({
                                              QSL("chrome://accessibility"),
                                              QSL("chrome://appcache-internals"),
                                              QSL("chrome://blob-internals"),
                                              QSL("chrome://dino"),
                                              QSL("chrome://gpu"),
                                              QSL("chrome://histograms"),
                                              QSL("chrome://indexeddb-internals"),
                                              QSL("chrome://media-internals"),
                                              QSL("chrome://network-errors"),
                                              QSL("chrome://process-internals"),
                                              QSL("chrome://quota-internals"),
                                              QSL("chrome://sandbox"),
                                              QSL("chrome://serviceworker-internals"),
                                              QSL("chrome://webrtc-internals")
                                          });

    QString html;
    html.append(tr("Open these URLs in separate tab or copy-paste them."));
    html.append(QSL("<ul>"));
    for (const auto &url : internalUrls)
        html.append(QSL("<li><a href=\"%1\">%1</a></li>").arg(url));
    html.append(QSL("</ul>"));
    return makeSimpleHtml(tr("Chromium URLs"),html,true);
}

QString CGenericFuncs::getClipboardContent(bool noFormatting, bool plainpre) {
    QString res;
    QString cbContents;
    QString cbContentsUnformatted;

    QClipboard *cb = QApplication::clipboard();
    if (cb->mimeData(QClipboard::Clipboard)->hasText()) {
        cbContentsUnformatted = cb->mimeData(QClipboard::Clipboard)->text();
        if (cb->mimeData(QClipboard::Clipboard)->hasHtml()) {
            cbContents = cb->mimeData(QClipboard::Clipboard)->html();
        } else {
            cbContents = cb->mimeData(QClipboard::Clipboard)->text();
        }
    }

    if (plainpre && !cbContentsUnformatted.isEmpty()) {
        res=cbContentsUnformatted;
        if (res.indexOf(QSL("\r\n"))>=0) {
            res.replace(QSL("\r\n"),QSL("</p><p>"));
        } else {
            res.replace(QSL("\n"),QSL("</p><p>"));
        }
        res = QSL("<p>%1</p>").arg(res);
        res = makeSimpleHtml(QSL("<...>"),res);
    } else {
        if (noFormatting) {
            if (!cbContentsUnformatted.isEmpty())
                res = cbContentsUnformatted;
        } else {
            if (!cbContents.isEmpty()) {
                res = cbContents;
            } else if (!cbContentsUnformatted.isEmpty()) {
                res = makeSimpleHtml(QSL("<...>"),cbContentsUnformatted);
            }
        }
    }
    return res;
}

QString CGenericFuncs::wordWrap(const QString &str, int wrapLength)
{
    const QStringList stl = str.split(u' ');
    QString ret;
    int cnt = 0;
    for (const auto &st : stl) {
        ret += st + u' ';
        cnt += st.length()+1;
        if (cnt>wrapLength) {
            ret += u'\n';
            cnt = 0;
        }
    }
    return ret;
}

QString CGenericFuncs::formatFileSize(const QString& size)
{
    bool ok = false;
    qint64 sz = size.toLong(&ok);
    if (ok)
        return formatFileSize(sz);

    return size;
}

QString CGenericFuncs::formatSize(double size, int precision)
{
    const QString suffixes = QSL(" kMGTPEZY");
    const double base = 1000.0;
    const QChar separator(0x20);
    int power = 0;
    if (size > 0)
        power = static_cast<int>(std::log10(qAbs(size)) / 3);

    QString res = (power > 0)
        ? QString::number(size / std::pow(base, power), 'f', precision)
        : QString::number(size);

    if (power > 0) {
        if (power<suffixes.length()) {
            res.append(separator);
            res.append(suffixes.at(power));
        } else {
            res = QSL("<error>");
        }
    }

    return res;
}

QString CGenericFuncs::formatFileSize(qint64 size)
{
    const int precision = 2;
    return QLocale::c().formattedDataSize(size,precision,QLocale::DataSizeTraditionalFormat);
}

QString CGenericFuncs::elideString(const QString& text, int maxlen, Qt::TextElideMode mode)
{
    if (text.length()<maxlen)
        return text;

    if (mode == Qt::ElideRight)
        return QSL("%1...").arg(text.left(maxlen));

    if (mode == Qt::ElideLeft)
        return QSL("...%1").arg(text.right(maxlen));

    if (mode == Qt::ElideMiddle)
        return QSL("%1...%2").arg(text.left(maxlen/2),text.right(maxlen/2));

    return text;
}

QString CGenericFuncs::highlightSnippet(const QString& snippet, const QStringList& terms)
{
    QString res = snippet;

    int i = 0;
    for (const auto &term : terms) {
        QString ITerm = term.trimmed();
        QString snpColor = CSettings::snippetColors[i % CSettings::snippetColors.count()].name();
        int start = 0;
        int pos = -1;
        while ((pos = res.indexOf(ITerm,start,Qt::CaseInsensitive))>=0) {
            QString tag = QSL("<font color='%1'>").arg(snpColor);
            int pos2 = pos + tag.length() + ITerm.length();
            res.insert(pos,tag);
            res.insert(pos2,QSL("</font>"));
            start = pos2;
        }
        i++;
    }
    return res;
}

QString CGenericFuncs::getOpenFileNameD (QWidget * parent, const QString & caption, const QString & dir,
                                         const QStringList & filters, QString * selectedFilter)
{
    QFileDialog::Options opts;
    if (gSet->settings()->dontUseNativeFileDialog)
        opts = QFileDialog::DontUseNativeDialog | QFileDialog::DontUseCustomDirectoryIcons;

    QFileDialog dialog(parent,caption,dir);
    dialog.setOptions(opts);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilters(filters);
    if (gSet->d_func()->openFileDialogSize.isValid())
        dialog.resize(gSet->d_func()->openFileDialogSize);

    QString res;
    if (selectedFilter && !selectedFilter->isEmpty())
        dialog.selectNameFilter(*selectedFilter);
    if (dialog.exec() == QDialog::Accepted) {
        if (selectedFilter)
            *selectedFilter = dialog.selectedNameFilter();
        const auto selectedFiles = dialog.selectedFiles();
        if (!selectedFiles.isEmpty())
            res = selectedFiles.first();
    }

    gSet->d_func()->openFileDialogSize = dialog.size();
    return res;
}

QStringList CGenericFuncs::getOpenFileNamesD (QWidget * parent, const QString & caption, const QString & dir,
                                              const QStringList & filters, QString * selectedFilter)
{
    QFileDialog::Options opts;
    if (gSet->settings()->dontUseNativeFileDialog)
        opts = QFileDialog::DontUseNativeDialog | QFileDialog::DontUseCustomDirectoryIcons;

    QFileDialog dialog(parent,caption,dir);
    dialog.setOptions(opts);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::ExistingFiles);
    dialog.setNameFilters(filters);
    if (gSet->d_func()->openFileDialogSize.isValid())
        dialog.resize(gSet->d_func()->openFileDialogSize);

    QStringList res;
    if (selectedFilter && !selectedFilter->isEmpty())
        dialog.selectNameFilter(*selectedFilter);
    if (dialog.exec() == QDialog::Accepted) {
        if (selectedFilter)
            *selectedFilter = dialog.selectedNameFilter();
        res = dialog.selectedFiles();
    }

    gSet->d_func()->openFileDialogSize = dialog.size();
    return res;
}

QStringList CGenericFuncs::getSuffixesFromFilter(const QString& filter)
{
    // regex from qplatformdialoghelper.cpp
    static const QRegularExpression filterRegExp(QSL("^(.*)\\(([a-zA-Z0-9_.,*? +;#\\-\\[\\]@\\{\\}/!<>\\$%&=^~:\\|]*)\\)$"));
    static const QRegularExpression cleanWildcardsRx((QSL("^.*\\.")));
    QStringList res;
    res.clear();
    if (filter.isEmpty()) return res;

    const QStringList filters = filter.split(QSL(";;"),Qt::SkipEmptyParts);
    if (filters.isEmpty()) return res;

    for (const auto &filter : filters) {
        auto match = filterRegExp.match(filter);
        if (match.hasMatch()) {
            const QStringList suffixList = match.captured(2).split(u' ',Qt::SkipEmptyParts);
            for (const auto &suffix : suffixList) {
                QString s = suffix;
                s.remove(cleanWildcardsRx);
                res.append(s);
            }
        }
    }

    return res;
}

QString CGenericFuncs::getSaveFileNameD (QWidget * parent, const QString & caption, const QString & dir,
                                         const QStringList & filters, QString * selectedFilter,
                                         const QString & preselectFileName )
{
    QFileDialog::Options opts;
    if (gSet->settings()->dontUseNativeFileDialog)
        opts = QFileDialog::DontUseNativeDialog | QFileDialog::DontUseCustomDirectoryIcons;

    QFileDialog dialog(parent,caption,dir);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setOptions(opts);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setNameFilters(filters);
    if (gSet->d_func()->saveFileDialogSize.isValid())
        dialog.resize(gSet->d_func()->saveFileDialogSize);

    if (selectedFilter && !selectedFilter->isEmpty())
        dialog.selectNameFilter(*selectedFilter);

    if (!preselectFileName.isEmpty())
        dialog.selectFile(preselectFileName);

    QString res;
    if (dialog.exec()==QDialog::Accepted) {
        QString userFilter = dialog.selectedNameFilter();
        if (selectedFilter)
            *selectedFilter=userFilter;
        if (!userFilter.isEmpty()) {
            const auto suffixes = getSuffixesFromFilter(userFilter);
            dialog.setDefaultSuffix(suffixes.first());
        }

        const auto selectedFiles = dialog.selectedFiles();
        if (!selectedFiles.isEmpty())
            res = selectedFiles.first();
    }

    gSet->d_func()->saveFileDialogSize = dialog.size();
    return res;
}

QString	CGenericFuncs::getExistingDirectoryD (QWidget * parent, const QString & caption,
                                              const QString & dir, QFileDialog::Options options,
                                              const QString & suggestedName)
{
    QFileDialog::Options opts = options;
    if (gSet->settings()->dontUseNativeFileDialog)
        opts = QFileDialog::DontUseNativeDialog | QFileDialog::DontUseCustomDirectoryIcons;

    gSet->ui()->setFileDialogNewFolderName(suggestedName);
    QString res = QFileDialog::getExistingDirectory(parent,caption,dir,opts);
    gSet->ui()->setFileDialogNewFolderName(QString());
    return res;
}

QByteArray CGenericFuncs::hmacSha1(const QByteArray &key, const QByteArray &baseString)
{
    const int blockSize = 64; // HMAC-SHA-1 block size, defined in SHA-1 standard
    QByteArray mkey = key;
    if (mkey.length() > blockSize) { // if key is longer than block size (64), reduce key length with SHA-1 compression
        mkey = QCryptographicHash::hash(mkey, QCryptographicHash::Sha1);
    }

    // ascii characters 0x36 ("6") and 0x5c ("quot;) are selected because they have large
    // Hamming distance (http://en.wikipedia.org/wiki/Hamming_distance)
    const char innerPaddingChar = 0x36;
    const char outerPaddingChar = 0x5c;
    QByteArray innerPadding(blockSize, innerPaddingChar); // initialize inner padding with char "6"
    QByteArray outerPadding(blockSize, outerPaddingChar); // initialize outer padding with char "quot;

    for (int i = 0; i < mkey.length(); i++) {
         // XOR operation between every byte in key and innerpadding, of key length
        innerPadding[i] = static_cast<char>(static_cast<quint8>(innerPadding.at(i)) ^ static_cast<quint8>(mkey.at(i)));
         // XOR operation between every byte in key and outerpadding, of key length
        outerPadding[i] = static_cast<char>(static_cast<quint8>(outerPadding[i]) ^ static_cast<quint8>(mkey.at(i)));
    }

    // result = hash ( outerPadding CONCAT hash ( innerPadding CONCAT baseString ) ).toBase64
    QByteArray total = outerPadding;
    QByteArray part = innerPadding;
    part.append(baseString);
    total.append(QCryptographicHash::hash(part, QCryptographicHash::Sha1));
    QByteArray hashed = QCryptographicHash::hash(total, QCryptographicHash::Sha1);
    return hashed;
}

QImage CGenericFuncs::dataUrlToImage(const QString &dataUrl)
{
    static const QString b64marker = QSL(";base64,");
    int b64markerPos = dataUrl.indexOf(b64marker,Qt::CaseInsensitive);
    if (b64markerPos < 0) return QImage();
    const QString base64 = dataUrl.mid(b64markerPos + b64marker.length());

    auto result = QByteArray::fromBase64Encoding(base64.toLatin1());
    if (result.decodingStatus != QByteArray::Base64DecodingStatus::Ok) return QImage();
    return QImage::fromData(result.decoded);
}

QByteArray CGenericFuncs::signSHA256withRSA(const QByteArray &data, const QByteArray &privateKey)
{
    QByteArray res;

    int rc = 1;

    QByteArray digest = QCryptographicHash::hash(data,QCryptographicHash::Sha256);

    BIO *b = nullptr;
    RSA *r = nullptr;

    b = BIO_new_mem_buf(privateKey.constData(), privateKey.length());
    if (nullptr == b) {
        return res;
    }
    r = PEM_read_bio_RSAPrivateKey(b, nullptr, nullptr, nullptr);
    if (nullptr == r) {
        BIO_free(b);
        return res;
    }

    QScopedPointer<unsigned char,QScopedPointerPodDeleter> sig(reinterpret_cast<unsigned char*>(malloc(RSA_size(r)))); // NOLINT
    unsigned int sig_len = 0;

    if (nullptr == sig) {
        BIO_free(b);
        RSA_free(r);
        return res;
    }

    rc = RSA_sign(NID_sha256, reinterpret_cast<const unsigned char *>(digest.constData()),
                  digest.length(), sig.data(), &sig_len, r);
    if (1 == rc) {
        res = QByteArray(reinterpret_cast<char *>(sig.data()),static_cast<int>(sig_len));
    }
    BIO_free(b);
    RSA_free(r);
    return res;
}

QString CGenericFuncs::bool2str(bool value)
{
    if (value)
        return QSL("on");

    return QSL("off");
}

QString CGenericFuncs::bool2str2(bool value)
{
    if (value)
        return QSL("TRUE");

    return QSL("FALSE");
}

QString CGenericFuncs::paddedNumber(int value, int maxValue)
{
    const int base = 10;
    QString res = QSL("%1").arg(value,CGenericFuncs::numDigits(maxValue),base,QChar(u'0'));
    return res;
}

int CGenericFuncs::numDigits(int n)
{
    const int base = 10;
    const int minusBase = ((-1)*base)+1;

    if ((n >= 0) && (n < base))
        return 1;

    if ((n >= minusBase) && (n < 0))
        return 2;

    if (n<0)
        return 2 + numDigits(abs(n) / base);

    return 1 + numDigits(n / base);
}

QString CGenericFuncs::secsToString(qint64 seconds)
{
    const QChar zeroChar(u'0');

    qint64 secs = seconds;
    qint64 hours = seconds / CDefaults::oneHour;
    secs -= hours * CDefaults::oneHour;
    qint64 mins = secs / CDefaults::oneMinute;
    secs -= mins * CDefaults::oneMinute;

    if (hours>0) {
        return QSL("%1:%2:%3")
                .arg(hours)
                .arg(mins,2,CDefaults::intConversionBase,zeroChar)
                .arg(secs,2,CDefaults::intConversionBase,zeroChar);
    }

    if (mins>0) {
        return QSL("%1:%2")
                .arg(mins)
                .arg(secs,2,CDefaults::intConversionBase,zeroChar);
    }

    return QSL("%1 sec").arg(secs);
}

void CGenericFuncs::processedSleep(unsigned long secs)
{
    for (unsigned long i=0;i<secs;i++) {
        QThread::sleep(1);
        QApplication::processEvents();
    }
}

void CGenericFuncs::processedMSleep(unsigned long msecs)
{
    const int granularity = 50;
    for (unsigned long i=0;i<(msecs/granularity);i++) {
        QThread::msleep(granularity);
        QApplication::processEvents();
    }
}

void CGenericFuncs::processedMSleep(std::chrono::milliseconds msecs)
{
    using namespace std::chrono_literals;
    const std::chrono::milliseconds granularity = 50ms;
    for (auto i=0;i<(msecs/granularity);i++) {
        QThread::msleep(granularity.count());
        QApplication::processEvents();
    }

}

const QVector<QStringList> &CGenericFuncs::encodingsByScript()
{
    static const QVector<QStringList> enc = {
        { QSL("Western European"), QSL("ISO-8859-1"), QSL("ISO-8859-15"),
          QSL("ISO-8859-14"), QSL("cp1252"), QSL("CP850"), QSL("x-MacRoman") },
        { QSL("Central European"), QSL("ISO-8859-2"), QSL("ISO-8859-3"),
          QSL("cp1250"), QSL("x-MacCentralEurope") },
        { QSL("Baltic"), QSL("ISO-8859-4"), QSL("ISO-8859-13"),
          QSL("cp1257") },
        { QSL("Turkish"), QSL("cp1254"), QSL("ISO-8859-9"), QSL("x-MacTurkish") },
        { QSL("Cyrillic"), QSL("KOI8-R"), QSL("ISO-8859-5"), QSL("cp1251"),
          QSL("KOI8-U"), QSL("CP866"), QSL("x-MacCyrillic") },
        { QSL("Chinese"), QSL("HZ"), QSL("GBK"), QSL("GB18030"),
          QSL("BIG5"), QSL("BIG5-HKSCS"), QSL("ISO-2022-CN"),
          QSL("ISO-2022-CN-EXT") },
        { QSL("Korean"), QSL("MS949"), QSL("ISO-2022-KR") },
        { QSL("Japanese"), QSL("EUC-JP"), QSL("SHIFT_JIS"), QSL("ISO-2022-JP"),
          QSL("ISO-2022-JP-2"), QSL("ISO-2022-JP-1") },
        { QSL("Greek"), QSL("ISO-8859-7"), QSL("cp1253"), QSL("x-MacGreek") },
        { QSL("Arabic"), QSL("ISO-8859-6"), QSL("cp1256") },
        { QSL("Hebrew"), QSL("ISO-8859-8"), QSL("cp1255"), QSL("CP862") },
        { QSL("Thai"), QSL("MS874") },
        { QSL("Vietnamese"), QSL("CP1258") },
        { QSL("Unicode"), QSL("UTF-8"), QSL("UTF-7"), QSL("UTF-16"),
          QSL("UTF-16BE"), QSL("UTF-16LE"), QSL("UTF-32"), QSL("UTF-32BE"),
          QSL("UTF-32LE") },
        { QSL("Other"), QSL("windows-1258"), QSL("Macintosh") }
    };

    return enc;
}

const QStringList &CGenericFuncs::getSupportedImageExtensions()
{
    static const QStringList supportedImageExtensions {
        QSL("jpeg"), QSL("jpg"), QSL("jpe"), QSL("gif"),
                QSL("bmp"), QSL("png") };

    return supportedImageExtensions;
}

QString CGenericFuncs::getTmpDir()
{
    QFileInfo fi(QDir::homePath() + QDir::separator() + QSL("tmp"));
    if (fi.exists() && fi.isDir() && fi.isWritable())
        return QDir::homePath() + QDir::separator() + QSL("tmp");

    return QStandardPaths::writableLocation(QStandardPaths::TempLocation);
}

int CGenericFuncs::compareStringLists(const QStringList &left, const QStringList &right)
{
    int diff = left.count()-right.count();
    if (diff!=0) return diff;

    for (int i=0;i<left.count();i++) {
        diff = QString::compare(left.at(i),right.at(i));
        if (diff!=0) return diff;
    }

    return 0;
}

QString CGenericFuncs::extractFileTitle(const QString& fileContents)
{
    const int maxFileSize = 255;
    int pos = -1;
    int start = -1;
    int stop = -1;
    static const QRegularExpression titleStart(QSL("<title {0,}>"),QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression titleStop(QSL("</title {0,}>"),QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression insideTitle(QSL("^<title {0,}>"),QRegularExpression::CaseInsensitiveOption);

    if ((pos = fileContents.indexOf(titleStart)) != -1) {
        start = pos;
        if ((pos = fileContents.indexOf(titleStop)) != -1) {
            stop = pos;
            if (stop>start) {
                if ((stop-start)>maxFileSize)
                    stop = start + maxFileSize;
                QString s = fileContents.mid(start,stop-start);

                s.remove(insideTitle);
                s.remove(u'\r');
                s.remove(u'\n');
                return s;
            }
        }
    }
    return QString();
}

QString CGenericFuncs::convertPatternToRegExp(const QString &wildcardPattern)
{
    static const QRegularExpression multipleWildcards(QSL("\\*+"));
    static const QRegularExpression followingAnchors(QSL("\\^\\|$"));
    static const QRegularExpression leadingWildcards(QSL("^(\\*)"));
    static const QRegularExpression trailingWildcards(QSL("(\\*)$"));
    static const QRegularExpression specialSymbols(QSL("(\\W)"));
    static const QRegularExpression extendedAnchor(QSL("^\\\\\\|\\\\\\|"));
    static const QRegularExpression separatorPlaceholders(QSL("\\\\\\^"));
    static const QRegularExpression anchorExpStart(QSL("^\\\\\\|"));
    static const QRegularExpression anchorExpEnd(QSL("\\\\\\|$"));
    static const QRegularExpression wildcards(QSL("\\\\\\*"));

    QString pattern = wildcardPattern;
    return pattern.replace(multipleWildcards, QSL("*"))     // remove multiple wildcards
            .replace(followingAnchors, QSL("^"))            // remove anchors following separator placeholder
            .replace(leadingWildcards, QString())           // remove leading wildcards
            .replace(trailingWildcards, QString())          // remove trailing wildcards
            .replace(specialSymbols, QSL("\\\\1"))          // escape special symbols
            .replace(extendedAnchor,
                     QSL("^[\\w\\-]+:\\/+(?!\\/)(?:[^\\/]+\\.)?")) // process extended anchor at expression start
            .replace(separatorPlaceholders,
                     QSL("(?:[^\\w\\d\\-.%]|$)"))           // process separator placeholders
            .replace(anchorExpStart, QSL("^"))              // process anchor at expression start
            .replace(anchorExpEnd, QSL("$"))                // process anchor at expression end
            .replace(wildcards, QSL(".*"))                  // replace wildcards by .*
            ;
}

void CGenericFuncs::sendStringToWidget(QWidget *widget, const QString& s)
{
    if (widget==nullptr) return;
    for (const auto &c : s) {
        Qt::KeyboardModifiers mod = Qt::NoModifier;
        if (c.isLetter() && c.isUpper()) mod = Qt::ShiftModifier;
        QTest::sendKeyEvent(QTest::KeyAction::Click,widget,Qt::Key_A,c,mod);
    }
}

void CGenericFuncs::sendKeyboardInputToView(QWidget *widget, const QString& s)
{
    if (widget==nullptr) return;

    widget->setFocus();
    for (QObject *obj : widget->children()) {
        QWidget *w = qobject_cast<QWidget*>(obj);
        if (w)
            sendStringToWidget(w,s);
    }
    sendStringToWidget(widget,s);
}
