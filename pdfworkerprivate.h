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

class CPDFWorkerPrivate : public QObject
{
    Q_OBJECT

private:
    QString m_text;
    QString m_pageSeparator;
    CIntList m_outLengths;
    bool m_prevblock { false };
    static int loggedPopplerErrors;

    Q_DISABLE_COPY(CPDFWorkerPrivate)

    void metaDate(QString &out, Dict *infoDict, const char *key, const QString &fmt);
    void metaString(QString &out, Dict *infoDict, const char *key, const QString &fmt);
    QString formatPdfText(const QString &text);
    int zlibInflate(const char* src, int srcSize, uchar *dst, int dstSize);

    static void outputToString(void *stream, const char *text, int len);

public:
    explicit CPDFWorkerPrivate(QObject *parent = nullptr);
    ~CPDFWorkerPrivate() override = default;

    static void popplerError(void *data, ErrorCategory category, Goffset pos, const char *msg);

    QString pdfToText(bool *error, const QString &filename);

};

#endif // WITH_POPPLER

#endif // PDFWORKERPRIVATE_H
