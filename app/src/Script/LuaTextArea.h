#pragma once

#include <QQuickTextArea>
#include <QSyntaxHighlighter>

class SimpleLuaHighlighter;

/**
 * 带 Lua 语法高亮的 TextArea
 * 内部管理高亮器生命周期，确保析构顺序正确
 */
class LuaTextArea : public QQuickTextArea
{
    Q_OBJECT

public:
    explicit LuaTextArea(QQuickItem *parent = nullptr);
    ~LuaTextArea() override;

private:
    SimpleLuaHighlighter *m_highlighter = nullptr;
};
