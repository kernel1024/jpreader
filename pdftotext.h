#ifndef PDFTOTEXT_H
#define PDFTOTEXT_H

#include <QObject>
#include <QUrl>
#include <QString>
#include "structures.h"

class CPDFWorker : public QObject
{
    Q_OBJECT
public:
    explicit CPDFWorker(QObject* parent = nullptr);
    ~CPDFWorker() override = default;

private:
    QString m_text;
    CIntList m_outLengths;
    bool m_prevblock { false };

    Q_DISABLE_COPY(CPDFWorker)

    QString formatPdfText(const QString &text);
    int zlibInflate(const char* src, int srcSize, uchar *dst, int dstSize);
    static void outputToString(void *stream, const char *text, int len);

public Q_SLOTS:
    void pdfToText(const QString &filename);

Q_SIGNALS:
    void gotText(const QString& result);
    void error(const QString& message);

};

void initPdfToText();
void freePdfToText();

#endif // PDFTOTEXT_H

