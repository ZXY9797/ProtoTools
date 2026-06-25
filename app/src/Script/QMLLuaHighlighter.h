#pragma once

#include <QObject>
#include <QQuickTextDocument>
#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QTextCharFormat>

/**
 * 简单的 Lua 语法高亮器
 */
class SimpleLuaHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
public:
    explicit SimpleLuaHighlighter(QTextDocument *doc);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QList<HighlightingRule> m_rules;
    HighlightingRule m_commentRule;
    QList<QColor> m_bracketColors;
};

/**
 * QML Lua 语法高亮器
 * 用法：QMLLuaHighlighter { document: textArea.textDocument }
 */
class QMLLuaHighlighter : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQuickTextDocument* document READ document WRITE setDocument NOTIFY documentChanged)

public:
    explicit QMLLuaHighlighter(QObject *parent = nullptr);
    ~QMLLuaHighlighter() override;

    QQuickTextDocument* document() const;
    void setDocument(QQuickTextDocument *doc);

signals:
    void documentChanged();

private:
    QQuickTextDocument *m_doc = nullptr;
    SimpleLuaHighlighter *m_highlighter = nullptr;
};
