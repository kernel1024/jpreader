#ifndef PDFTOTEXT_H
#define PDFTOTEXT_H

#include <QObject>
#include <QString>
#include "global/structures.h"

class CPDFWorkerPrivate;

class CPDFWorker : public QObject
{
    Q_OBJECT
public:
    explicit CPDFWorker(QObject* parent = nullptr);
    ~CPDFWorker() override;
    static void initPdfToText();
    static void freePdfToText();

private:
    QScopedPointer<CPDFWorkerPrivate> dptr;

    Q_DISABLE_COPY(CPDFWorker)
    Q_DECLARE_PRIVATE_D(dptr,CPDFWorker)

public Q_SLOTS:
    void pdfToText(const QString &filename);

Q_SIGNALS:
    void gotText(const QString& result);
    void error(const QString& message);
    void finished();

};


#endif // PDFTOTEXT_H

