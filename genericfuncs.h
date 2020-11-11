#ifndef GENERICFUNCS_H
#define GENERICFUNCS_H

#include <QString>
#include <QByteArray>
#include <QVector>
#include <QStringList>
#include <QWidget>
#include <QFileDialog>
#include <QElapsedTimer>
#include <QNetworkReply>

namespace CDefaults {
const int httpCodeFound = 200;
const int httpCodeRedirect = 300;
const int httpCodeClientError = 400;
const int httpCodeServerError = 500;
const int httpCodeClientUnknownError = 499;
const int httpCodeTooManyRequests = 429;
const int oneMB = 1024*1024;
const int oneHour = 60*60;
const int oneMinute = 60;
const int intConversionBase = 10;
const int httpMaxRedirects = 10;
}

class CGenericFuncs : public QObject
{
    Q_OBJECT
public:
    explicit CGenericFuncs(QObject* parent = nullptr);
    ~CGenericFuncs() override = default;

    static QString detectMIME(const QString& filename);
    static QString detectMIME(const QByteArray& buf);
    static QTextCodec* detectEncoding(const QByteArray &content);
    static QString detectEncodingName(const QByteArray &content);
    static QString detectDecodeToUnicode(const QByteArray &content);
    static QString makeSimpleHtml(const QString& title, const QString& content, bool integratedTitle = false,
                                  const QUrl &origin = QUrl());
    static QString makeSpecialUrlsHtml();
    static QString getClipboardContent(bool noFormatting = false, bool plainpre = false);
    static QString wordWrap(const QString &str, int wrapLength);
    static QString formatFileSize(qint64 size);
    static QString formatFileSize(const QString& size);
    static QString formatSize(double size, int precision);
    static QString elideString(const QString& text, int maxlen, Qt::TextElideMode mode = Qt::ElideRight);
    static QString highlightSnippet(const QString& snippet, const QStringList& terms);
    static const QVector<QStringList> &encodingsByScript();
    static const QStringList &getSupportedImageExtensions();
    static QString getTmpDir();
    static bool checkAndUnpackUrl(QUrl& url);
    static int compareStringLists(const QStringList& left, const QStringList& right);
    static QString extractFileTitle(const QString& fileContents);
    static QString convertPatternToRegExp(const QString &wildcardPattern);
    static void sendKeyboardInputToView(QWidget *widget, const QString& s);
    static QString bool2str(bool value);
    static QString bool2str2(bool value);
    static QString paddedNumber(int value, int maxValue);
    static int numDigits(int n);
    static QString secsToString(qint64 seconds);
    static void processedSleep(unsigned long secs);
    static void processedMSleep(unsigned long msecs);
    static bool writeBytesToZip(const QString& zipFile, const QString& fileName, const QByteArray& data);
    static QString unsplitMobileText(const QString &text);
    static QString makeSafeFilename(const QString &text);
    static QString encodeHtmlEntities(const QString &text);
    static QString decodeHtmlEntities(const QString &text);
    static int getHttpStatusFromReply(QNetworkReply *reply);

    static QString getOpenFileNameD ( QWidget * parent = nullptr, const QString & caption = QString(),
                                      const QString & dir = QString(), const QStringList & filters = QStringList(),
                                      QString * selectedFilter = nullptr);

    static QStringList getOpenFileNamesD (QWidget * parent = nullptr, const QString & caption = QString(),
                                           const QString & dir = QString(), const QStringList &filters = QStringList(),
                                           QString * selectedFilter = nullptr);

    static QString getSaveFileNameD (QWidget * parent = nullptr, const QString & caption = QString(),
                                     const QString & dir = QString(), const QStringList &filters = QStringList(),
                                     QString * selectedFilter = nullptr, const QString & preselectFileName = QString());

    static QString	getExistingDirectoryD (QWidget * parent = nullptr, const QString & caption = QString(),
                                           const QString & dir = QString(),
                                           QFileDialog::Options options = QFileDialog::ShowDirsOnly,
                                           const QString &suggestedName = QString());

    static QByteArray signSHA256withRSA(const QByteArray& data, const QByteArray& privateKey);

    static QByteArray hmacSha1(const QByteArray &key, const QByteArray &baseString);

    static void profiler(int line, const QElapsedTimer &time);


private:
    Q_DISABLE_COPY(CGenericFuncs)

    static QStringList getSuffixesFromFilter(const QString &filter);
    static void sendStringToWidget(QWidget *widget, const QString &s);
};

#endif // GENERICFUNCS_H
