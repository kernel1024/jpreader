#ifndef JSEDIT_P_H
#define JSEDIT_P_H

#include <QSyntaxHighlighter>
#include "jsedit.h"

class JSHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
public:
    explicit JSHighlighter(QTextDocument *parent = nullptr);
    ~JSHighlighter() override = default;
    void setColor(JSEdit::ColorComponent component, const QColor &color);
    void mark(const QString &str, Qt::CaseSensitivity caseSensitivity);

    QStringList keywords() const;
    void setKeywords(const QStringList &keywords);

protected:
    void highlightBlock(const QString &text) override;

private:
    QString m_markString;
    QSet<QString> m_keywords;
    QSet<QString> m_knownIds;
    QHash<JSEdit::ColorComponent, QColor> m_colors;
    Qt::CaseSensitivity m_markCaseSensitivity { Qt::CaseInsensitive };
};

class JSBlockData: public QTextBlockUserData
{
public:
    QList<int> bracketPositions;

    JSBlockData() {}
    JSBlockData(const JSBlockData& other) = delete;
    ~JSBlockData() override;
    JSBlockData &operator=(const JSBlockData& other) = delete;

};

struct BlockInfo {
    int position;
    int number;
    bool foldable;
    bool folded;
    char _reserved[2];
};
Q_DECLARE_TYPEINFO(BlockInfo, Q_PRIMITIVE_TYPE);

class SidebarWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SidebarWidget(JSEdit *editor);
    QVector<BlockInfo> lineNumbers;
    QColor backgroundColor;
    QColor lineNumberColor;
    QColor indicatorColor;
    QColor foldIndicatorColor;
    QFont font;
    QPixmap rightArrowIcon;
    QPixmap downArrowIcon;
    int foldIndicatorWidth;
protected:
    void mousePressEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
};

class JSDocLayout: public QPlainTextDocumentLayout
{
    Q_OBJECT
public:
    explicit JSDocLayout(QTextDocument *doc);
    void forceUpdate();
};

class JSEditPrivate
{
public:
    QList<int> matchPositions;
    QList<int> errorPositions;
    QColor cursorColor;
    QColor bracketMatchColor;
    QColor bracketErrorColor;
    JSEdit *editor {nullptr};
    JSDocLayout *layout {nullptr};
    JSHighlighter *highlighter {nullptr};
    SidebarWidget *sidebar {nullptr};
    bool showLineNumbers {false};
    bool textWrap {false};
    bool bracketsMatching {false};
    bool codeFolding {false};
};

#endif // JSEDIT_P_H
