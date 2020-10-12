/*
  This file is part of the Ofi Labs X2 project.

  Copyright (C) 2011 Ariya Hidayat <ariya.hidayat@gmail.com>
  Copyright (C) 2010 Ariya Hidayat <ariya.hidayat@gmail.com>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the <organization> nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <QPainter>
#include <QRegularExpression>
#include <QDebug>

#include "jsedit.h"
#include "jsedit_p.h"
#include "structures.h"
#include "genericfuncs.h"

JSHighlighter::JSHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    static const QColor identifierColor(0,0,32);

    // default color scheme
    m_colors[JSEdit::Normal]     = QColor(Qt::black);
    m_colors[JSEdit::Comment]    = QColor(Qt::darkGray);
    m_colors[JSEdit::Number]     = QColor(Qt::darkGreen);
    m_colors[JSEdit::String]     = QColor(Qt::darkRed);
    m_colors[JSEdit::Operator]   = QColor(Qt::darkYellow);
    m_colors[JSEdit::Identifier] = identifierColor;
    m_colors[JSEdit::Keyword]    = QColor(Qt::darkBlue);
    m_colors[JSEdit::BuiltIn]    = QColor(Qt::darkCyan);
    m_colors[JSEdit::Marker]     = QColor(Qt::yellow);

    // https://developer.mozilla.org/en/JavaScript/Reference/Reserved_Words
    m_keywords = { QSL("break"),
                   QSL("case"),
                   QSL("catch"),
                   QSL("continue"),
                   QSL("default"),
                   QSL("delete"),
                   QSL("do"),
                   QSL("else"),
                   QSL("finally"),
                   QSL("for"),
                   QSL("function"),
                   QSL("if"),
                   QSL("in"),
                   QSL("instanceof"),
                   QSL("new"),
                   QSL("return"),
                   QSL("switch"),
                   QSL("this"),
                   QSL("throw"),
                   QSL("try"),
                   QSL("typeof"),
                   QSL("var"),
                   QSL("void"),
                   QSL("while"),
                   QSL("with"),

                   QSL("true"),
                   QSL("false"),
                   QSL("null")
                 };

    // built-in and other popular objects + properties
    m_knownIds = { QSL("Object"),
                   QSL("prototype"),
                   QSL("create"),
                   QSL("defineProperty"),
                   QSL("defineProperties"),
                   QSL("getOwnPropertyDescriptor"),
                   QSL("keys"),
                   QSL("getOwnPropertyNames"),
                   QSL("constructor"),
                   QSL("__parent__"),
                   QSL("__proto__"),
                   QSL("__defineGetter__"),
                   QSL("__defineSetter__"),
                   QSL("eval"),
                   QSL("hasOwnProperty"),
                   QSL("isPrototypeOf"),
                   QSL("__lookupGetter__"),
                   QSL("__lookupSetter__"),
                   QSL("__noSuchMethod__"),
                   QSL("propertyIsEnumerable"),
                   QSL("toSource"),
                   QSL("toLocaleString"),
                   QSL("toString"),
                   QSL("unwatch"),
                   QSL("valueOf"),
                   QSL("watch"),

                   QSL("Function"),
                   QSL("arguments"),
                   QSL("arity"),
                   QSL("caller"),
                   QSL("constructor"),
                   QSL("length"),
                   QSL("name"),
                   QSL("apply"),
                   QSL("bind"),
                   QSL("call"),

                   QSL("String"),
                   QSL("fromCharCode"),
                   QSL("length"),
                   QSL("charAt"),
                   QSL("charCodeAt"),
                   QSL("concat"),
                   QSL("indexOf"),
                   QSL("lastIndexOf"),
                   QSL("localCompare"),
                   QSL("match"),
                   QSL("quote"),
                   QSL("replace"),
                   QSL("search"),
                   QSL("slice"),
                   QSL("split"),
                   QSL("substr"),
                   QSL("substring"),
                   QSL("toLocaleLowerCase"),
                   QSL("toLocaleUpperCase"),
                   QSL("toLowerCase"),
                   QSL("toUpperCase"),
                   QSL("trim"),
                   QSL("trimLeft"),
                   QSL("trimRight"),

                   QSL("Array"),
                   QSL("isArray"),
                   QSL("index"),
                   QSL("input"),
                   QSL("pop"),
                   QSL("push"),
                   QSL("reverse"),
                   QSL("shift"),
                   QSL("sort"),
                   QSL("splice"),
                   QSL("unshift"),
                   QSL("concat"),
                   QSL("join"),
                   QSL("filter"),
                   QSL("forEach"),
                   QSL("every"),
                   QSL("map"),
                   QSL("some"),
                   QSL("reduce"),
                   QSL("reduceRight"),

                   QSL("RegExp"),
                   QSL("global"),
                   QSL("ignoreCase"),
                   QSL("lastIndex"),
                   QSL("multiline"),
                   QSL("source"),
                   QSL("exec"),
                   QSL("test"),

                   QSL("JSON"),
                   QSL("parse"),
                   QSL("stringify"),

                   QSL("decodeURI"),
                   QSL("decodeURIComponent"),
                   QSL("encodeURI"),
                   QSL("encodeURIComponent"),
                   QSL("eval"),
                   QSL("isFinite"),
                   QSL("isNaN"),
                   QSL("parseFloat"),
                   QSL("parseInt"),
                   QSL("Infinity"),
                   QSL("NaN"),
                   QSL("undefined"),

                   QSL("Math"),
                   QSL("E"),
                   QSL("LN2"),
                   QSL("LN10"),
                   QSL("LOG2E"),
                   QSL("LOG10E"),
                   QSL("PI"),
                   QSL("SQRT1_2"),
                   QSL("SQRT2"),
                   QSL("abs"),
                   QSL("acos"),
                   QSL("asin"),
                   QSL("atan"),
                   QSL("atan2"),
                   QSL("ceil"),
                   QSL("cos"),
                   QSL("exp"),
                   QSL("floor"),
                   QSL("log"),
                   QSL("max"),
                   QSL("min"),
                   QSL("pow"),
                   QSL("random"),
                   QSL("round"),
                   QSL("sin"),
                   QSL("sqrt"),
                   QSL("tan"),

                   QSL("document"),
                   QSL("window"),
                   QSL("navigator"),
                   QSL("userAgent")
                 };
}

void JSHighlighter::setColor(JSEdit::ColorComponent component, const QColor &color)
{
    m_colors[component] = color;
    rehighlight();
}

void JSHighlighter::highlightBlock(const QString &text)
{
    // parsing state
    enum {
        Start = 0,
        Number = 1,
        Identifier = 2,
        String = 3,
        Comment = 4,
        Regex = 5
    };

    QList<int> bracketPositions;

    const uint stateMask = 0xf;
    uint blockState = 0;
    uint state = Start;
    int bracketLevel = 0;
    if (previousBlockState()>=0) {
        blockState = static_cast<uint>(previousBlockState());
        bracketLevel = static_cast<int>(blockState >> 4U);
        state = blockState & stateMask;
    }

    int start = 0;
    int i = 0;
    while (i <= text.length()) {
        QChar ch = (i < text.length()) ? text.at(i) : QChar();
        QChar next = (i < text.length() - 1) ? text.at(i + 1) : QChar();

        switch (state) {

        case Start:
            start = i;
            if (ch.isSpace()) {
                ++i;
            } else if (ch.isDigit()) {
                ++i;
                state = Number;
            } else if (ch.isLetter() || ch == '_') {
                ++i;
                state = Identifier;
            } else if (ch == '\'' || ch == '\"') {
                ++i;
                state = String;
            } else if (ch == '/' && next == '*') {
                ++i;
                ++i;
                state = Comment;
            } else if (ch == '/' && next == '/') {
                i = text.length();
                setFormat(start, text.length(), m_colors[JSEdit::Comment]);
            } else if (ch == '/' && next != '*') {
                ++i;
                state = Regex;
            } else {
                if (!QSL("(){}[]").contains(ch))
                    setFormat(start, 1, m_colors[JSEdit::Operator]);
                if (ch =='{' || ch == '}') {
                    bracketPositions += i;
                    if (ch == '{') {
                        bracketLevel++;
                    } else {
                        bracketLevel--;
                    }
                }
                ++i;
                state = Start;
            }
            break;

        case Number:
            if (ch.isSpace() || !ch.isDigit()) {
                setFormat(start, i - start, m_colors[JSEdit::Number]);
                state = Start;
            } else {
                ++i;
            }
            break;

        case Identifier:
            if (ch.isSpace() || !(ch.isDigit() || ch.isLetter() || ch == '_')) {
                QString token = text.mid(start, i - start).trimmed();
                if (m_keywords.contains(token)) {
                    setFormat(start, i - start, m_colors[JSEdit::Keyword]);
                } else if (m_knownIds.contains(token)) {
                    setFormat(start, i - start, m_colors[JSEdit::BuiltIn]);
                }
                state = Start;
            } else {
                ++i;
            }
            break;

        case String:
            if (ch == text.at(start)) {
                QChar prev = (i > 0) ? text.at(i - 1) : QChar();
                if (prev != '\\') {
                    ++i;
                    setFormat(start, i - start, m_colors[JSEdit::String]);
                    state = Start;
                } else {
                    ++i;
                }
            } else {
                ++i;
            }
            break;

        case Comment:
            if (ch == '*' && next == '/') {
                ++i;
                ++i;
                setFormat(start, i - start, m_colors[JSEdit::Comment]);
                state = Start;
            } else {
                ++i;
            }
            break;

        case Regex:
            if (ch == '/') {
                QChar prev = (i > 0) ? text.at(i - 1) : QChar();
                if (prev != '\\') {
                    ++i;
                    setFormat(start, i - start, m_colors[JSEdit::String]);
                    state = Start;
                } else {
                    ++i;
                }
            } else {
                ++i;
            }
            break;

        default:
            state = Start;
            break;
        }
    }

    if (state == Comment) {
        setFormat(start, text.length(), m_colors[JSEdit::Comment]);
    } else {
        state = Start;
    }

    if (!m_markString.isEmpty()) {
        int pos = 0;
        int len = m_markString.length();
        QTextCharFormat markerFormat;
        markerFormat.setBackground(m_colors[JSEdit::Marker]);
        markerFormat.setForeground(m_colors[JSEdit::Normal]);
        for (;;) {
            pos = text.indexOf(m_markString, pos, m_markCaseSensitivity);
            if (pos < 0)
                break;
            setFormat(pos, len, markerFormat);
            ++pos;
        }
    }

    if (!bracketPositions.isEmpty()) {
        JSBlockData *blockData = reinterpret_cast<JSBlockData*>(currentBlock().userData());
        if (!blockData) {
            blockData = new JSBlockData;
            currentBlock().setUserData(blockData);
        }
        blockData->bracketPositions = bracketPositions;
    }

    blockState = (state & stateMask) | (static_cast<uint>(bracketLevel) << 4U);
    setCurrentBlockState(static_cast<int>(blockState));
}

void JSHighlighter::mark(const QString &str, Qt::CaseSensitivity caseSensitivity)
{
    m_markString = str;
    m_markCaseSensitivity = caseSensitivity;
    rehighlight();
}

QStringList JSHighlighter::keywords() const
{
    return QStringList(m_keywords.constBegin(),m_keywords.constEnd());
}

void JSHighlighter::setKeywords(const QStringList &keywords)
{
    m_keywords = QSet<QString>(keywords.constBegin(),keywords.constEnd());
    rehighlight();
}

SidebarWidget::SidebarWidget(JSEdit *editor)
    : QWidget(editor)
    , foldIndicatorWidth(0)
{
    backgroundColor = Qt::lightGray;
    lineNumberColor = Qt::black;
    indicatorColor = Qt::white;
    foldIndicatorColor = Qt::lightGray;
}

void SidebarWidget::mousePressEvent(QMouseEvent *event)
{
    if (foldIndicatorWidth > 0) {
        int xofs = width() - foldIndicatorWidth;
        int lineNo = -1;
        int fh = fontMetrics().lineSpacing();
        int ys = event->pos().y();
        if (event->pos().x() > xofs) {
            for (const auto &ln : qAsConst(lineNumbers)) {
                if (ln.position < ys && (ln.position + fh) > ys) {
                    if (ln.foldable)
                        lineNo = ln.number;
                    break;
                }
            }
        }
        if (lineNo >= 0) {
            auto *editor = qobject_cast<JSEdit*>(parent());
            if (editor)
                editor->toggleFold(lineNo);
        }
    }
}

void SidebarWidget::paintEvent(QPaintEvent *event)
{
    QPainter p(this);
    p.fillRect(event->rect(), backgroundColor);
    p.setPen(lineNumberColor);
    p.setFont(font);
    int fh = QFontMetrics(font).height();
    for (const auto &ln : qAsConst(lineNumbers))
        p.drawText(0, ln.position, width() - 4 - foldIndicatorWidth, fh, Qt::AlignRight, QString::number(ln.number));

    if (foldIndicatorWidth > 0) {
        int xofs = width() - foldIndicatorWidth;
        p.fillRect(xofs, 0, foldIndicatorWidth, height(), indicatorColor);

        // initialize (or recreate) the arrow icons whenever necessary
        if (foldIndicatorWidth != rightArrowIcon.width()) {
            QPainter iconPainter;
            QPolygonF polygon;

            int dim = foldIndicatorWidth;
            rightArrowIcon = QPixmap(dim, dim);
            rightArrowIcon.fill(Qt::transparent);
            downArrowIcon = rightArrowIcon;

            const qreal leftF = 0.4;
            const qreal rightF = 0.8;
            const qreal upF = 0.25;
            const qreal downF = 0.75;
            const qreal vmidF = 0.5;
            polygon << QPointF(dim * leftF, dim * upF);
            polygon << QPointF(dim * leftF, dim * downF);
            polygon << QPointF(dim * rightF, dim * vmidF);
            iconPainter.begin(&rightArrowIcon);
            iconPainter.setRenderHint(QPainter::Antialiasing);
            iconPainter.setPen(Qt::NoPen);
            iconPainter.setBrush(foldIndicatorColor);
            iconPainter.drawPolygon(polygon);
            iconPainter.end();

            polygon.clear();
            polygon << QPointF(dim * upF, dim * leftF);
            polygon << QPointF(dim * downF, dim * leftF);
            polygon << QPointF(dim * vmidF, dim * rightF);
            iconPainter.begin(&downArrowIcon);
            iconPainter.setRenderHint(QPainter::Antialiasing);
            iconPainter.setPen(Qt::NoPen);
            iconPainter.setBrush(foldIndicatorColor);
            iconPainter.drawPolygon(polygon);
            iconPainter.end();
        }

        for (const auto &ln : qAsConst(lineNumbers)) {
            if (ln.foldable) {
                if (ln.folded) {
                    p.drawPixmap(xofs, ln.position, rightArrowIcon);
                } else {
                    p.drawPixmap(xofs, ln.position, downArrowIcon);
                }
            }
        }
    }
}

static int findClosingMatch(const QTextDocument *doc, int cursorPosition)
{
    QTextBlock block = doc->findBlock(cursorPosition);
    auto *blockData = reinterpret_cast<JSBlockData*>(block.userData());
    if (!blockData->bracketPositions.isEmpty()) {
        int depth = 1;
        while (block.isValid()) {
            blockData = reinterpret_cast<JSBlockData*>(block.userData());
            if (blockData && !blockData->bracketPositions.isEmpty()) {
                for (int c = 0; c < blockData->bracketPositions.count(); ++c) {
                    int absPos = block.position() + blockData->bracketPositions.at(c);
                    if (absPos <= cursorPosition)
                        continue;
                    if (doc->characterAt(absPos) == '{') {
                        depth++;
                    } else {
                        depth--;
                    }
                    if (depth == 0)
                        return absPos;
                }
            }
            block = block.next();
        }
    }
    return -1;
}

static int findOpeningMatch(const QTextDocument *doc, int cursorPosition)
{
    QTextBlock block = doc->findBlock(cursorPosition);
    auto *blockData = reinterpret_cast<JSBlockData*>(block.userData());
    if (!blockData->bracketPositions.isEmpty()) {
        int depth = 1;
        while (block.isValid()) {
            blockData = reinterpret_cast<JSBlockData*>(block.userData());
            if (blockData && !blockData->bracketPositions.isEmpty()) {
                for (int c = blockData->bracketPositions.count() - 1; c >= 0; --c) {
                    int absPos = block.position() + blockData->bracketPositions.at(c);
                    if (absPos >= cursorPosition - 1)
                        continue;
                    if (doc->characterAt(absPos) == '}') {
                        depth++;
                    } else {
                        depth--;
                    }
                    if (depth == 0)
                        return absPos;
                }
            }
            block = block.previous();
        }
    }
    return -1;
}

JSDocLayout::JSDocLayout(QTextDocument *doc)
    : QPlainTextDocumentLayout(doc)
{
}

void JSDocLayout::forceUpdate()
{
    Q_EMIT documentSizeChanged(documentSize());
}

JSEdit::JSEdit(QWidget *parent)
    : QPlainTextEdit(parent)
    , dptr(new JSEditPrivate)
{
    static const QColor cursorColor(255,255,192);
    static const QColor bracketMatchColor(180,238,180);
    static const QColor bracketErrorColor(224,128,128);

    dptr->editor = this;
    dptr->layout = new JSDocLayout(document());
    dptr->highlighter = new JSHighlighter(document());
    dptr->sidebar = new SidebarWidget(this);
    dptr->showLineNumbers = true;
    dptr->textWrap = true;
    dptr->bracketsMatching = true;
    dptr->cursorColor = cursorColor;
    dptr->bracketMatchColor = bracketMatchColor;
    dptr->bracketErrorColor = bracketErrorColor;
    dptr->codeFolding = true;

    document()->setDocumentLayout(dptr->layout);

    connect(this, &JSEdit::cursorPositionChanged, this, &JSEdit::updateCursor);
    connect(this, &JSEdit::blockCountChanged, this, [this](int newBlockCount){
        Q_UNUSED(newBlockCount)
        updateSidebar();
    });
    connect(this, &JSEdit::updateRequest, this, [this](const QRect &rect, int dy){
        Q_UNUSED(rect)
        Q_UNUSED(dy)
        updateSidebar();
    });

#if defined(Q_OS_MAC)
    QFont textFont = font();
    textFont.setPointSize(12);
    textFont.setFamily(QSL("Monaco"));
    setFont(textFont);
#elif defined(Q_OS_UNIX)
    QFont textFont = font();
    textFont.setFamily(QSL("Monospace"));
    setFont(textFont);
#endif

    installEventFilter(this);
}

JSEdit::~JSEdit()
{
    delete dptr->layout;
}

void JSEdit::setColor(ColorComponent component, const QColor &color)
{
    Q_D(JSEdit);

    if (component == Background) {
        QPalette pal = palette();
        pal.setColor(QPalette::Base, color);
        setPalette(pal);
        d->sidebar->indicatorColor = color;
        updateSidebar();
    } else if (component == Normal) {
        QPalette pal = palette();
        pal.setColor(QPalette::Text, color);
        setPalette(pal);
    } else if (component == Sidebar) {
        d->sidebar->backgroundColor = color;
        updateSidebar();
    } else if (component == LineNumber) {
        d->sidebar->lineNumberColor = color;
        updateSidebar();
    } else if (component == Cursor) {
        d->cursorColor = color;
        updateCursor();
    } else if (component == BracketMatch) {
        d->bracketMatchColor = color;
        updateCursor();
    } else if (component == BracketError) {
        d->bracketErrorColor = color;
        updateCursor();
    } else if (component == FoldIndicator) {
        d->sidebar->foldIndicatorColor = color;
        updateSidebar();
    } else {
        d->highlighter->setColor(component, color);
        updateCursor();
    }
}

QStringList JSEdit::keywords() const
{
    return dptr->highlighter->keywords();
}

void JSEdit::setKeywords(const QStringList &keywords)
{
    dptr->highlighter->setKeywords(keywords);
}

bool JSEdit::isLineNumbersVisible() const
{
    return dptr->showLineNumbers;
}

void JSEdit::setLineNumbersVisible(bool visible)
{
    dptr->showLineNumbers = visible;
    updateSidebar();
    Q_EMIT lineNumbersVisibilityChanged();
}

bool JSEdit::isTextWrapEnabled() const
{
    return dptr->textWrap;
}

void JSEdit::setTextWrapEnabled(bool enable)
{
    dptr->textWrap = enable;
    setLineWrapMode(enable ? WidgetWidth : NoWrap);
    Q_EMIT textWrapChanged();
}

bool JSEdit::isBracketsMatchingEnabled() const
{
    return dptr->bracketsMatching;
}

void JSEdit::setBracketsMatchingEnabled(bool enable)
{
    dptr->bracketsMatching = enable;
    updateCursor();
    Q_EMIT bracketsMatchingChanged();
}

bool JSEdit::isCodeFoldingEnabled() const
{
    return dptr->codeFolding;
}

void JSEdit::setCodeFoldingEnabled(bool enable)
{
    dptr->codeFolding = enable;
    updateSidebar();
    Q_EMIT codeFoldingChanged();
}

static int findClosingConstruct(const QTextBlock &block)
{
    if (!block.isValid())
        return -1;
    auto *blockData = reinterpret_cast<JSBlockData*>(block.userData());
    if (!blockData)
        return -1;
    if (blockData->bracketPositions.isEmpty())
        return -1;
    const QTextDocument *doc = block.document();
    int offset = block.position();
    for (const int pos : qAsConst(blockData->bracketPositions)) {
        int absPos = offset + pos;
        if (doc->characterAt(absPos) == '{') {
            int matchPos = findClosingMatch(doc, absPos);
            if (matchPos >= 0)
                return matchPos;
        }
    }
    return -1;
}

bool JSEdit::isFoldable(int line) const
{
    int matchPos = findClosingConstruct(document()->findBlockByNumber(line - 1));
    if (matchPos >= 0) {
        QTextBlock matchBlock = document()->findBlock(matchPos);
        if (matchBlock.isValid() && matchBlock.blockNumber() > line)
            return true;
    }
    return false;
}

bool JSEdit::isFolded(int line) const
{
    QTextBlock block = document()->findBlockByNumber(line - 1);
    if (!block.isValid())
        return false;
    block = block.next();
    if (!block.isValid())
        return false;
    return !block.isVisible();
}

void JSEdit::fold(int line)
{
    QTextBlock startBlock = document()->findBlockByNumber(line - 1);
    int endPos = findClosingConstruct(startBlock);
    if (endPos < 0)
        return;
    QTextBlock endBlock = document()->findBlock(endPos);

    QTextBlock block = startBlock.next();
    while (block.isValid() && block != endBlock) {
        block.setVisible(false);
        block.setLineCount(0);
        block = block.next();
    }

    document()->markContentsDirty(startBlock.position(), endPos - startBlock.position() + 1);
    updateSidebar();
    update();

    auto *layout = reinterpret_cast<JSDocLayout*>(document()->documentLayout());
    layout->forceUpdate();
}

void JSEdit::unfold(int line)
{
    QTextBlock startBlock = document()->findBlockByNumber(line - 1);
    int endPos = findClosingConstruct(startBlock);

    QTextBlock block = startBlock.next();
    while (block.isValid() && !block.isVisible()) {
        block.setVisible(true);
        block.setLineCount(block.layout()->lineCount());
        endPos = block.position() + block.length();
        block = block.next();
    }

    document()->markContentsDirty(startBlock.position(), endPos - startBlock.position() + 1);
    updateSidebar();
    update();

    auto *layout = reinterpret_cast<JSDocLayout*>(document()->documentLayout());
    layout->forceUpdate();
}

void JSEdit::toggleFold(int line)
{
    if (isFolded(line)) {
        unfold(line);
    } else {
        fold(line);
    }
}

void JSEdit::resizeEvent(QResizeEvent *e)
{
    QPlainTextEdit::resizeEvent(e);
    updateSidebar();
}

void JSEdit::wheelEvent(QWheelEvent *e)
{
    const int stepsMin = -3;
    const int stepsMax = 3;
    const int pointSizeMin = 10;
    const int pointSizeMax = 40;
    const int deltasInStep = 20;

    if (e->modifiers() == Qt::ControlModifier) {
        int steps = e->angleDelta().y() / deltasInStep;
        steps = qBound(stepsMin, steps, stepsMax);
        QFont textFont = font();
        int pointSize = textFont.pointSize() + steps;
        pointSize = qBound(pointSizeMin, pointSize, pointSizeMax);
        textFont.setPointSize(pointSize);
        setFont(textFont);
        updateSidebar();
        e->accept();
        return;
    }
    QPlainTextEdit::wheelEvent(e);
}

bool JSEdit::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        auto * const key = dynamic_cast<QKeyEvent*>(event);
        const QRegularExpression rxIndent(QSL("^(\\s+)"));

        if (key->key() == Qt::Key_Enter || key->key() == Qt::Key_Return)
        {
            QString currentLine = textCursor().block().text();
            int pos = textCursor().positionInBlock();

            textCursor().insertBlock();

            // Auto indent
            QRegularExpressionMatch match = rxIndent.match(currentLine);
            if (match.hasMatch())
                textCursor().insertText(match.captured(1));

            // Add indent after opening brace
            if (pos>0 && ((pos-1) < currentLine.length()) &&
                    (currentLine.at(pos-1) == QChar('{'))) {
                textCursor().insertText(QSL("    "));
            }
            ensureCursorVisible();
            return true;
        }

        // Remove additional indent before closing brace
        static const QString closingBrace = QSL("}");
        if (key->text() == closingBrace) {
            bool onlySpaces = textCursor().block().text().trimmed().isEmpty();
            textCursor().insertText(closingBrace);
            if (onlySpaces) {
                int pos = findOpeningMatch(document(),textCursor().position());
                if (pos>=0) {
                    QTextBlock block = document()->findBlock(pos);
                    QString indent;
                    QRegularExpressionMatch match = rxIndent.match(block.text());
                    if (match.hasMatch())
                        indent = match.captured(1);

                    textCursor().beginEditBlock();
                    while (textCursor().positionInBlock()>0)
                        textCursor().deletePreviousChar();
                    textCursor().insertText(indent);
                    textCursor().insertText(closingBrace);
                    textCursor().endEditBlock();
                }
            }
            ensureCursorVisible();
            return true;
        }
    }

    return QPlainTextEdit::eventFilter(obj, event);
}


void JSEdit::updateCursor()
{
    Q_D(JSEdit);

    if (isReadOnly()) {
        setExtraSelections(QList<QTextEdit::ExtraSelection>());
    } else {

        d->matchPositions.clear();
        d->errorPositions.clear();

        if (d->bracketsMatching && (textCursor().block().userData() != nullptr)) {
            QTextCursor cursor = textCursor();
            int cursorPosition = cursor.position();

            if (document()->characterAt(cursorPosition) == '{') {
                int matchPos = findClosingMatch(document(), cursorPosition);
                if (matchPos < 0) {
                    d->errorPositions += cursorPosition;
                } else {
                    d->matchPositions += cursorPosition;
                    d->matchPositions += matchPos;
                }
            }

            if (document()->characterAt(cursorPosition - 1) == '}') {
                int matchPos = findOpeningMatch(document(), cursorPosition);
                if (matchPos < 0) {
                    d->errorPositions += cursorPosition - 1;
                } else {
                    d->matchPositions += cursorPosition - 1;
                    d->matchPositions += matchPos;
                }
            }
        }

        QTextEdit::ExtraSelection highlight;
        highlight.format.setBackground(d->cursorColor);
        highlight.format.setProperty(QTextFormat::FullWidthSelection, true);
        highlight.cursor = textCursor();
        highlight.cursor.clearSelection();

        QList<QTextEdit::ExtraSelection> extraSelections;
        extraSelections.reserve(d->matchPositions.count() + d->errorPositions.count() + 1);
        extraSelections.append(highlight);

        for (int i = 0; i < d->matchPositions.count(); ++i) {
            int pos = d->matchPositions.at(i);
            QTextEdit::ExtraSelection matchHighlight;
            matchHighlight.format.setBackground(d->bracketMatchColor);
            matchHighlight.cursor = textCursor();
            matchHighlight.cursor.setPosition(pos);
            matchHighlight.cursor.setPosition(pos + 1, QTextCursor::KeepAnchor);
            extraSelections.append(matchHighlight);
        }

        for (int i = 0; i < d->errorPositions.count(); ++i) {
            int pos = d->errorPositions.at(i);
            QTextEdit::ExtraSelection errorHighlight;
            errorHighlight.format.setBackground(d->bracketErrorColor);
            errorHighlight.cursor = textCursor();
            errorHighlight.cursor.setPosition(pos);
            errorHighlight.cursor.setPosition(pos + 1, QTextCursor::KeepAnchor);
            extraSelections.append(errorHighlight);
        }

        setExtraSelections(extraSelections);
    }
}

void JSEdit::updateSidebar(const QRect &rect, int d)
{
    Q_UNUSED(rect)
    if (d != 0)
        updateSidebar();
}

void JSEdit::updateSidebar()
{
    Q_D(JSEdit);

    if (!d->showLineNumbers && !d->codeFolding) {
        d->sidebar->hide();
        setViewportMargins(0, 0, 0, 0);
        d->sidebar->setGeometry(3, 0, 0, height());
        return;
    }

    d->sidebar->foldIndicatorWidth = 0;
    d->sidebar->font = this->font();
    d->sidebar->show();

    int sw = 0;
    if (d->showLineNumbers) {
        int digits = qMax(2,CGenericFuncs::numDigits(blockCount()));
        sw += fontMetrics().boundingRect('w').width() * digits;
    }
    if (d->codeFolding) {
        int fh = fontMetrics().lineSpacing();
        int fw = fontMetrics().boundingRect('w').width();
        d->sidebar->foldIndicatorWidth = qMax(fw, fh);
        sw += d->sidebar->foldIndicatorWidth;
    }
    setViewportMargins(sw, 0, 0, 0);

    d->sidebar->setGeometry(0, 0, sw, height());
    QRectF sidebarRect(0, 0, sw, height());

    QTextBlock block = firstVisibleBlock();
    int index = 0;
    while (block.isValid()) {
        if (block.isVisible()) {
            QRectF rect = blockBoundingGeometry(block).translated(contentOffset());
            if (sidebarRect.intersects(rect)) {
                if (d->sidebar->lineNumbers.count() >= index)
                    d->sidebar->lineNumbers.resize(index + 1);
                d->sidebar->lineNumbers[index].position = qRound(rect.top());
                d->sidebar->lineNumbers[index].number = block.blockNumber() + 1;
                d->sidebar->lineNumbers[index].foldable = d->codeFolding ? isFoldable(block.blockNumber() + 1) : false;
                d->sidebar->lineNumbers[index].folded = d->codeFolding ? isFolded(block.blockNumber() + 1) : false;
                ++index;
            }
            if (rect.top() > sidebarRect.bottom())
                break;
        }
        block = block.next();
    }
    d->sidebar->lineNumbers.resize(index);
    d->sidebar->update();
}

void JSEdit::mark(const QString &str, Qt::CaseSensitivity sens)
{
    dptr->highlighter->mark(str, sens);
}

JSBlockData::~JSBlockData() = default;
