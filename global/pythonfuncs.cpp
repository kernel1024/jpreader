#include "pythonfuncs_p.h"
#include "pythonfuncs.h"
#include "global/structures.h"

#include <unicode/utypes.h>
#include <unicode/brkiter.h>
#include <unicode/unistr.h>
#include <unicode/ustream.h>

#include <QDebug>

CGlobalPython::CGlobalPython(QObject *parent)
    : QObject(parent),
      dptr(new CGlobalPythonPrivate())
{
}

CGlobalPython::~CGlobalPython() = default;

int CGlobalPython::tiktokenCountTokens(const QString &text, const QString &model)
{
    Q_D(CGlobalPython);

#ifdef WITH_PYTHON3
    if (!d->changeEncoding(model))
        return fallbackCountTokens(text);

    const QByteArray text_utf8 = text.toUtf8();

    // Call the encode method with the text argument
    const QScopedPointer<PyObject,CScopedPointerPyObjectDeleter>
            encode_method(PyObject_GetAttrString(d->m_encoding.data(), "encode"));
    if (encode_method.isNull()) {
        d->setFailed();
        return fallbackCountTokens(text);
    }

    const QScopedPointer<PyObject,CScopedPointerPyObjectDeleter>
            encode_args(PyTuple_Pack(1, PyUnicode_FromString(text_utf8.data())));
    if (encode_args.isNull()) {
        d->setFailed();
        return fallbackCountTokens(text);
    }

    const QScopedPointer<PyObject,CScopedPointerPyObjectDeleter>
            encoded_list(PyObject_CallObject(encode_method.data(), encode_args.data()));
    if (encoded_list.isNull()) {
        d->setFailed();
        return fallbackCountTokens(text);
    }

    // Get the length of the encoded list
    const int num = static_cast<int>(PyList_GET_SIZE(encoded_list.data()));

    return num;
#else
    return fallbackCountTokens(text);
#endif
}

int CGlobalPython::fallbackCountTokens(const QString &text) const
{
    qWarning() << "Python tiktoken fallback with ICU.";

    UErrorCode status = U_ZERO_ERROR;
    int wordCount = 0;

    QScopedPointer<icu::BreakIterator>
            wordIterator(icu::BreakIterator::createWordInstance(icu::Locale::getDefault(), status));

    if (wordIterator.isNull() || U_FAILURE(status)) {
        // Emergency fallback
        qCritical() << "Python tiktoken fallback failed.";
        return text.length() / 2;
    }

    icu::UnicodeString utext = icu::UnicodeString::fromUTF8(text.toStdString());
    wordIterator->setText(utext);

    int32_t start = wordIterator->first();
    for (int32_t end = wordIterator->next(); end != icu::BreakIterator::DONE;
         start = end, end = wordIterator->next()) {
        ++wordCount;
    }

    return wordCount;
}

bool CGlobalPython::isTiktokenLoaded() const
{
    Q_D(const CGlobalPython);

    return d->isTiktokenLoaded();
}

CGlobalPythonPrivate::CGlobalPythonPrivate(QObject *parent)
    : QObject(parent)
{
    initialize();
}

void CGlobalPythonPrivate::initialize()
{
#ifdef WITH_PYTHON3
    Py_Initialize();

    m_tiktokenModule.reset(PyImport_ImportModule("tiktoken"));
    if (m_tiktokenModule.isNull()) return;

    m_encoding.reset(nullptr);
    m_encodingModel.clear();
#endif
}

CGlobalPythonPrivate::~CGlobalPythonPrivate() = default;

CGlobalPythonPrivateCleanup::~CGlobalPythonPrivateCleanup()
{
#ifdef WITH_PYTHON3
    Py_Finalize();
#endif
}

bool CGlobalPythonPrivate::isTiktokenLoaded() const
{
#ifdef WITH_PYTHON3
    return (!m_tiktokenModule.isNull() && !m_tiktokenFailed);
#else
    return false;
#endif
}

bool CGlobalPythonPrivate::changeEncoding(const QString &modelName)
{
    if (m_tiktokenFailed || m_tiktokenModule.isNull())
        return false;

    if (m_encodingModel == modelName)
        return true;

    // Attempt to change model
    QString model = modelName;
    if (model.isEmpty())
        model = QSL("cl100k_base");
    const QByteArray model_utf8 = model.toUtf8();

    const QScopedPointer<PyObject,CScopedPointerPyObjectDeleter>
            encoding_for_model(PyObject_GetAttrString(m_tiktokenModule.data(), "encoding_for_model"));
    if (encoding_for_model.isNull()) {
        setFailed();
        return false;
    }

    const QScopedPointer<PyObject,CScopedPointerPyObjectDeleter>
            args(PyTuple_Pack(1, PyUnicode_FromString(model_utf8.data())));
    m_encoding.reset(PyObject_CallObject(encoding_for_model.data(), args.data()));

    if (m_encoding.isNull()) {
        setFailed();
        return false;
    }

    m_encodingModel = modelName;
    return true;
}

void CGlobalPythonPrivate::setFailed()
{
    m_tiktokenFailed = true;
    m_encodingModel.clear();
}
