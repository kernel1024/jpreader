#ifndef CGLOBALPYTHON_H
#define CGLOBALPYTHON_H

#include <QObject>
#include <QString>
#include <QScopedPointer>

class CGlobalPythonPrivate;

class CGlobalPython : public QObject
{
    Q_OBJECT

public:
    explicit CGlobalPython(QObject *parent = nullptr);
    ~CGlobalPython() override;

    int tiktokenCountTokens(const QString& text) const;
    int fallbackCountTokens(const QString& text) const;

private:
    QScopedPointer<CGlobalPythonPrivate> dptr;

    Q_DECLARE_PRIVATE_D(dptr,CGlobalPython)
};

#endif // CGLOBALPYTHON_H
