/**
 * ========================================================================
 *
 * pdftotext.cc
 *
 *  Copyright 1997-2003 Glyph & Cog, LLC
 *
 *  Modified for Debian by Hamish Moffatt, 22 May 2002.
 *
 * ========================================================================
 *
 * ========================================================================
 *
 *  Modified under the Poppler project - http://poppler.freedesktop.org
 *
 *  All changes made under the Poppler project to this file are licensed
 *  under GPL version 2 or later
 *
 * Copyright (C) 2006 Dominic Lachowicz <cinamod@hotmail.com>
 * Copyright (C) 2007-2008, 2010, 2011 Albert Astals Cid <aacid@kde.org>
 * Copyright (C) 2009 Jan Jockusch <jan@jockusch.de>
 * Copyright (C) 2010, 2013 Hib Eris <hib@hiberis.nl>
 * Copyright (C) 2010 Kenneth Berland <ken@hero.com>
 * Copyright (C) 2011 Tom Gleason <tom@buildadam.com>
 * Copyright (C) 2011 Steven Murdoch <Steven.Murdoch@cl.cam.ac.uk>
 * Copyright (C) 2013 Yury G. Kudryashov <urkud.urkud@gmail.com>
 * Copyright (C) 2013 Suzuki Toshiya <mpsuzuki@hiroshima-u.ac.jp>
 * Copyright (C) 2015 Jeremy Echols <jechols@uoregon.edu>
 *
 * To see a description of the changes please see the Changelog file that
 * came with your tarball or type make ChangeLog if you are building from git
 *
 * Adaptation for JPReader by kernel1024
 *
 * ========================================================================*/

#include <QDebug>
#include "pdftotext.h"
#include "pdfworkerprivate.h"

#ifdef WITH_POPPLER
#include <GlobalParams.h>
#endif

CPDFWorker::CPDFWorker(QObject *parent)
    : QObject(parent),
      dptr(new CPDFWorkerPrivate(this))
{

}

CPDFWorker::~CPDFWorker()
{

}

void CPDFWorker::pdfToText(const QString &filename)
{
#ifndef WITH_POPPLER
    Q_UNUSED(filename)

    m_text = tr("pdfToText unavailable, JPReader compiled without poppler support.");
    qCritical() << m_text;
    Q_EMIT error(m_text);
#else
    Q_D(CPDFWorker);

    bool err;
    QString result = d->pdfToText(&err,filename);

    if (err) {
        Q_EMIT error(result);
    } else {
        Q_EMIT gotText(result);
    }
#endif
    Q_EMIT finished();
}

void CPDFWorker::initPdfToText()
{
#ifdef WITH_POPPLER
    const auto textEncoding = "UTF-8";
    globalParams = new GlobalParams();
    setErrorCallback(&(CPDFWorkerPrivate::popplerError),nullptr);
    globalParams->setTextEncoding(const_cast<char *>(textEncoding));
#endif
}

void CPDFWorker::freePdfToText()
{
#ifdef WITH_POPPLER
    delete globalParams;
#endif
}
