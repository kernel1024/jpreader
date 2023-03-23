#include "pythonfuncs_p.h"
#include "pythonfuncs.h"

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

int CGlobalPython::tiktokenCountTokens(const QString &text) const
{
    Q_D(const CGlobalPython);

    if (!d->isLoaded())
        return fallbackCountTokens(text);

#ifdef WITH_PYTHON3
    const QByteArray textUtf8 = text.toUtf8();

    // Call the tokenize method on the text
    QScopedPointer<PyObject,CScopedPointerPyObjectDeleter>
            tokenize_method(PyObject_GetAttrString(d->m_tokenizer.data(), "tokenize"));
    QScopedPointer<PyObject,CScopedPointerPyObjectDeleter>
            args(PyTuple_Pack(1, PyUnicode_FromString(textUtf8.data())));
    QScopedPointer<PyObject,CScopedPointerPyObjectDeleter>
            tokens(PyObject_CallObject(tokenize_method.data(), args.data()));

    // Iterate through tokens and add them to the token count
    QScopedPointer<PyObject,CScopedPointerPyObjectDeleter>
            iterator(PyObject_GetIter(tokens.data()));
    PyObject *token;
    while ((token = PyIter_Next(iterator.data()))) {
        PyObject *add_method = PyObject_GetAttrString(d->m_tokenCounter.data(), "add");
        PyObject *add_args = PyTuple_Pack(1, token);
        PyObject_CallObject(add_method, add_args);
        Py_DECREF(token);
    }

    // Get the length of the token count
    QScopedPointer<PyObject,CScopedPointerPyObjectDeleter>
            len_function(PyObject_GetAttrString(d->m_tokenCounter.data(), "__len__"));
    QScopedPointer<PyObject,CScopedPointerPyObjectDeleter>
            length(PyObject_CallObject(len_function.data(), nullptr));
    int token_count_length = PyLong_AsLong(length.data());

    return token_count_length;
#else
    return fallbackCountTokens(text);
#endif
}

int CGlobalPython::fallbackCountTokens(const QString &text) const
{
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

CGlobalPythonPrivate::CGlobalPythonPrivate(QObject *parent)
    : QObject(parent)
{
    initialize();
}

void CGlobalPythonPrivate::initialize()
{
#ifdef WITH_PYTHON3
    Py_Initialize();

    // Import tiktoken.Tokenizer and tiktoken.TokenCount
    m_module.reset(PyImport_ImportModule("tiktoken"));
    if (m_module.isNull()) return;

    m_tokenizerClass.reset(PyObject_GetAttrString(m_module.data(), "Tokenizer"));
    if (m_tokenizerClass.isNull()) return;

    m_tokenCounterClass.reset(PyObject_GetAttrString(m_module.data(), "TokenCount"));
    if (m_tokenCounterClass.isNull()) return;

    // Create instances of tiktoken.Tokenizer and tiktoken.TokenCount
    m_tokenizer.reset(PyObject_CallObject(m_tokenizerClass.data(), nullptr));
    if (m_tokenizer.isNull()) return;

    m_tokenCounter.reset(PyObject_CallObject(m_tokenCounterClass.data(), nullptr));
    if (m_tokenCounter.isNull()) return;
#endif

}

CGlobalPythonPrivate::~CGlobalPythonPrivate() = default;

CGlobalPythonPrivateCleanup::~CGlobalPythonPrivateCleanup()
{
#ifdef WITH_PYTHON3
    Py_Finalize();
#endif
}

bool CGlobalPythonPrivate::isLoaded() const
{
#ifdef WITH_PYTHON3
    return !m_tokenCounter.isNull();
#else
    return false;
#endif
}
