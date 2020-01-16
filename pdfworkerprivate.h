#ifndef PDFWORKERPRIVATE_H
#define PDFWORKERPRIVATE_H

#include <QObject>
#include "structures.h"

#ifdef WITH_POPPLER
#include <poppler-config.h>
#include <poppler-version.h>
#include <GlobalParams.h>
#include <Object.h>
#include <Dict.h>
#include <Error.h>
#endif // WITH_POPPLER

class CPDFWorkerPrivate : public QObject
{
    Q_OBJECT
private:
    QString m_pageSeparator;

public:
    explicit CPDFWorkerPrivate(QObject *parent = nullptr);
    ~CPDFWorkerPrivate() override = default;

#ifdef WITH_POPPLER
    static void initPdfToText();
    static void freePdfToText();
    QString pdfToText(bool *error, const QString &filename);

private:
    QString m_text;
    CIntList m_outLengths;
    bool m_prevblock { false };
    static int loggedPopplerErrors;


    void metaDate(QString &out, Dict *infoDict, const char *key, const QString &fmt);
    void metaString(QString &out, Dict *infoDict, const char *key, const QString &fmt);
    QString formatPdfText(const QString &text);
    int zlibInflate(const char* src, int srcSize, uchar *dst, int dstSize);

    static void outputToString(void *stream, const char *text, int len);
    static void popplerError(void *data, ErrorCategory category, Goffset pos, const char *msg);

#endif
private:
    Q_DISABLE_COPY(CPDFWorkerPrivate)

};

#endif // PDFWORKERPRIVATE_H
