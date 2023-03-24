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

    int tiktokenCountTokens(const QString& text, const QString &model);
    int fallbackCountTokens(const QString& text) const;

    bool isTiktokenLoaded() const;

private:
    QScopedPointer<CGlobalPythonPrivate> dptr;

    Q_DECLARE_PRIVATE_D(dptr,CGlobalPython)
    Q_DISABLE_COPY(CGlobalPython)
};

#endif // CGLOBALPYTHON_H
