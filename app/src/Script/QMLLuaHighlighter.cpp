#include "QMLLuaHighlighter.h"
#include <QTextDocument>
#include <QQuickTextDocument>
#include <QDebug>
#include <QColor>
#include <QStack>

SimpleLuaHighlighter::SimpleLuaHighlighter(QTextDocument *doc)
    : QSyntaxHighlighter(doc)
{
    // 1. 字符串 "..." (先于注释处理)
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression("\"[^\n\"]*\"");
        rule.format.setForeground(QColor("#CE9178"));
        m_rules.append(rule);
    }
    // 字符串 '...'
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression("'[^'\n]*'");
        rule.format.setForeground(QColor("#CE9178"));
        m_rules.append(rule);
    }

    // 2. 数字
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression("\\b\\d+\\.?\\d*\\b");
        rule.format.setForeground(QColor("#B5CEA8"));
        m_rules.append(rule);
    }

    // 3. 关键字
    QStringList keywords = {"function","end","if","then","else","elseif",
        "for","while","do","return","break","local","and","or","not",
        "in","repeat","until"};
    for (const auto &kw : keywords) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QString("\\b%1\\b").arg(kw));
        rule.format.setForeground(QColor("#C586C0"));
        rule.format.setFontWeight(QFont::Bold);
        m_rules.append(rule);
    }

    // 4. true/false/nil
    for (const auto &kw : {"true","false","nil"}) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QString("\\b%1\\b").arg(kw));
        rule.format.setForeground(QColor("#569CD6"));
        m_rules.append(rule);
    }

    // 5. 函数调用
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression("\\b([A-Za-z_][A-Za-z0-9_]*)\\s*(?=\\()");
        rule.format.setForeground(QColor("#DCDCAA"));
        m_rules.append(rule);
    }

    // 6. 单行注释 — 最后执行，覆盖上面所有格式
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression("--[^\n]*");
        rule.format.setForeground(QColor("#6A9955"));
        rule.format.setFontItalic(true);
        m_commentRule = rule;
    }

    // 括号颜色（6个层级循环）
    m_bracketColors = {
        QColor("#FFD700"),  // 金色
        QColor("#B5CEA8"),  // 绿色
        QColor("#C586C0"),  // 紫色
        QColor("#569CD6"),  // 蓝色
        QColor("#CE9178"),  // 橙色
        QColor("#4EC9B0"),  // 青色
    };
}

void SimpleLuaHighlighter::highlightBlock(const QString &text)
{
    // 1. 先应用普通规则
    for (const auto &rule : m_rules) {
        auto it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            auto match = it.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    // 2. 括号配对高亮（不同层级不同颜色）
    int depth = qMax(0, previousBlockState());
    for (int i = 0; i < text.length(); i++) {
        QChar ch = text[i];
        if (ch == '(' || ch == '{' || ch == '[') {
            int colorIdx = depth % m_bracketColors.size();
            QTextCharFormat fmt;
            fmt.setForeground(m_bracketColors[colorIdx]);
            fmt.setFontWeight(QFont::Bold);
            setFormat(i, 1, fmt);
            depth++;
        } else if (ch == ')' || ch == '}' || ch == ']') {
            depth = qMax(0, depth - 1);
            int colorIdx = depth % m_bracketColors.size();
            QTextCharFormat fmt;
            fmt.setForeground(m_bracketColors[colorIdx]);
            fmt.setFontWeight(QFont::Bold);
            setFormat(i, 1, fmt);
        }
    }
    setCurrentBlockState(depth);

    // 3. 注释最后执行 — 覆盖所有格式（包括括号颜色）
    {
        auto it = m_commentRule.pattern.globalMatch(text);
        while (it.hasNext()) {
            auto match = it.next();
            setFormat(match.capturedStart(), match.capturedLength(), m_commentRule.format);
        }
    }
}

// QMLLuaHighlighter implementation
QMLLuaHighlighter::QMLLuaHighlighter(QObject *parent)
    : QObject(parent)
{
}

QMLLuaHighlighter::~QMLLuaHighlighter()
{
    // 确保在文档析构之前清理高亮器
    if (m_highlighter && m_doc && m_doc->textDocument()) {
        m_highlighter->setDocument(nullptr);
    }
    delete m_highlighter;
    m_highlighter = nullptr;
}

QQuickTextDocument* QMLLuaHighlighter::document() const
{
    return m_doc;
}

void QMLLuaHighlighter::setDocument(QQuickTextDocument *doc)
{
    if (m_doc == doc)
        return;

    delete m_highlighter;
    m_highlighter = nullptr;

    m_doc = doc;

    if (m_doc) {
        QTextDocument *textDoc = m_doc->textDocument();
        if (textDoc) {
            m_highlighter = new SimpleLuaHighlighter(textDoc);
            qDebug() << "QMLLuaHighlighter: Highlighter created";
        }
    }

    emit documentChanged();
}
