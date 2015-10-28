#ifndef PDFTOTEXT_H
#define PDFTOTEXT_H

#include <QUrl>
#include <QString>

bool pdfToText(const QUrl& pdf, QString& result);
void initPdfToText();
void freePdfToText();

#endif // PDFTOTEXT_H

