#ifndef UITRANSLATOR_H
#define UITRANSLATOR_H

#include <QTranslator>
#include <QObject>

class CUITranslator : public QTranslator
{
    Q_OBJECT
    Q_DISABLE_COPY(CUITranslator)
private:
    QString m_fileDialogNewFolderName;
public:
    explicit CUITranslator(QObject *parent = nullptr);
    ~CUITranslator() override;
    QString translate(const char *context, const char *sourceText,
                      const char *disambiguation = nullptr, int n = -1) const override;

    void setFileDialogNewFolderName(const QString &fileDialogNewFolderName);
};

#endif // UITRANSLATOR_H
