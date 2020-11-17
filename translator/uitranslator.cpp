#include "uitranslator.h"

CUITranslator::CUITranslator(QObject *parent)
    : QTranslator(parent)
{
}

CUITranslator::~CUITranslator() = default;

QString CUITranslator::translate(const char *context, const char *sourceText, const char *disambiguation, int n) const
{
    QString ctx = QString::fromUtf8(context);
    QString text = QString::fromUtf8(sourceText);

    if (!m_fileDialogNewFolderName.isEmpty()
            && (ctx == QSL("QFileDialog"))
            && (text == QSL("New Folder"))) {
        return m_fileDialogNewFolderName;
    }

    return QTranslator::translate(context,sourceText,disambiguation,n);
}

void CUITranslator::setFileDialogNewFolderName(const QString &fileDialogNewFolderName)
{
    m_fileDialogNewFolderName = fileDialogNewFolderName;
}
