#ifndef GENERICFUNCS_H
#define GENERICFUNCS_H

#include <QString>
#include <QByteArray>
#include <QList>
#include <QStringList>
#include <QWidget>
#include <QFileDialog>
#include "translator.h"

extern QStringList debugMessages;

void stdConsoleOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg);

QString detectMIME(const QString& filename);
QString detectMIME(const QByteArray& buf);
QTextCodec* detectEncoding(const QByteArray &content);
QString detectEncodingName(const QByteArray &content);
QString detectDecodeToUnicode(const QByteArray &content);
QString makeSimpleHtml(const QString& title, const QString& content, bool integratedTitle = false);
QString getClipboardContent(bool noFormatting = false, bool plainpre = false);
QString fixMetaEncoding(const QString& data_utf8);
QString wordWrap(const QString &str, int wrapLength);
QList<QStringList> encodingsByScript();
QString bool2str(bool value);
QString bool2str2(bool value);
QString formatBytes(qint64 sz);
QString getTmpDir();
bool checkAndUnpackUrl(QUrl& url);
int getRandomTCPPort();
bool runnedFromQtCreator();
int compareStringLists(const QStringList& left, const QStringList& right);
void generateHTML(const CHTMLNode &src, QString &html, bool reformat = false, int depth = 0);
QString extractFileTitle(const QString& fileContents);
QString convertPatternToRegExp(const QString &wildcardPattern);
void sendKeyboardInputToView(QWidget *widget, const QString& s);

QString getOpenFileNameD ( QWidget * parent = 0, const QString & caption = QString(),
                           const QString & dir = QString(), const QString & filter = QString(),
                           QString * selectedFilter = 0);

QStringList getOpenFileNamesD ( QWidget * parent = 0, const QString & caption = QString(),
                               const QString & dir = QString(), const QString & filter = QString(),
                               QString * selectedFilter = 0);

QString getSaveFileNameD (QWidget * parent = 0, const QString & caption = QString(),
                          const QString & dir = QString(), const QString & filter = QString(),
                          QString * selectedFilter = 0, QString preselectFileName = QString());

QString	getExistingDirectoryD ( QWidget * parent = 0, const QString & caption = QString(),
                                const QString & dir = QString(),
                                QFileDialog::Options options = QFileDialog::ShowDirsOnly);

#endif // GENERICFUNCS_H
