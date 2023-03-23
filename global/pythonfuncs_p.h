#ifndef PYTHONFUNCS_P_H
#define PYTHONFUNCS_P_H

#ifdef WITH_PYTHON3
#include <Python.h>
#endif

#include <QObject>
#include <QScopedPointer>

class CGlobalPython;

#ifdef WITH_PYTHON3
struct CScopedPointerPyObjectDeleter
{
    static inline void cleanup(PyObject *pointer)
    {
        Py_DecRef(pointer);
    }
};
#endif

class CGlobalPythonPrivateCleanup {
public:
    ~CGlobalPythonPrivateCleanup();
};

class CGlobalPythonPrivate : public QObject, private CGlobalPythonPrivateCleanup {
    Q_OBJECT
    friend class CGlobalPython;

public:
#ifdef WITH_PYTHON3
    QScopedPointer<PyObject,CScopedPointerPyObjectDeleter> m_module;
    QScopedPointer<PyObject,CScopedPointerPyObjectDeleter> m_tokenizerClass;
    QScopedPointer<PyObject,CScopedPointerPyObjectDeleter> m_tokenCounterClass;
    QScopedPointer<PyObject,CScopedPointerPyObjectDeleter> m_tokenizer;
    QScopedPointer<PyObject,CScopedPointerPyObjectDeleter> m_tokenCounter;
#endif

    explicit CGlobalPythonPrivate(QObject *parent = nullptr);
    ~CGlobalPythonPrivate() override;

    bool isLoaded() const;

private:
    Q_DISABLE_COPY(CGlobalPythonPrivate)

    void initialize();

};

#endif // PYTHONFUNCS_P_H
