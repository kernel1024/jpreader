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
    QScopedPointer<PyObject,CScopedPointerPyObjectDeleter> m_tiktokenModule;
    QScopedPointer<PyObject,CScopedPointerPyObjectDeleter> m_encoding;
    QString m_encodingModel;
    bool m_tiktokenFailed { false };
#endif

    explicit CGlobalPythonPrivate(QObject *parent = nullptr);
    ~CGlobalPythonPrivate() override;

    bool isTiktokenLoaded() const;
    bool changeEncoding(const QString& modelName);
    void setFailed();

private:
    Q_DISABLE_COPY(CGlobalPythonPrivate)

    void initialize();

};

#endif // PYTHONFUNCS_P_H
