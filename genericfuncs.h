#ifndef GENERICFUNCS_H
#define GENERICFUNCS_H

#include <QString>
#include <QByteArray>
#include <QList>
#include <QStringList>
#include <QWidget>
#include <QFileDialog>

extern QStringList debugMessages;

void stdConsoleOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg);

QString detectMIME(const QString& filename);
QString detectMIME(const QByteArray& buf);
QTextCodec* detectEncoding(const QByteArray &content);
QString detectEncodingName(const QByteArray &content);
QString detectDecodeToUnicode(const QByteArray &content);
QString makeSimpleHtml(const QString& title, const QString& content);
QString getClipboardContent(bool noFormatting = false, bool plainpre = false);
QString fixMetaEncoding(const QString& data_utf8);
QString wordWrap(const QString &str, int wrapLength);
QList<QStringList> encodingsByScript();
QString bool2str(bool value);
QString bool2str2(bool value);
QString formatBytes(qint64 sz);
QString getTmpDir();
bool runnedFromQtCreator();
int compareStringLists(const QStringList& left, const QStringList& right);

QString getOpenFileNameD ( QWidget * parent = 0, const QString & caption = QString(),
                           const QString & dir = QString(), const QString & filter = QString(),
                           QString * selectedFilter = 0, QFileDialog::Options options = 0);

QStringList getOpenFileNamesD ( QWidget * parent = 0, const QString & caption = QString(),
                               const QString & dir = QString(), const QString & filter = QString(),
                               QString * selectedFilter = 0, QFileDialog::Options options = 0);

QString getSaveFileNameD (QWidget * parent = 0, const QString & caption = QString(),
                          const QString & dir = QString(), const QString & filter = QString(),
                          QString * selectedFilter = 0, QFileDialog::Options options = 0,
                          QString preselectFileName = QString());

QString	getExistingDirectoryD ( QWidget * parent = 0, const QString & caption = QString(),
                                const QString & dir = QString(),
                                QFileDialog::Options options = QFileDialog::ShowDirsOnly);

#endif // GENERICFUNCS_H
