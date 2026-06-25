#include "LuaTextArea.h"
#include "QMLLuaHighlighter.h"
#include <QTextDocument>
#include <QQuickTextDocument>
#include <QDebug>

LuaTextArea::LuaTextArea(QQuickItem *parent)
    : QQuickTextArea(parent)
{
    // 当 textDocument 可用时创建高亮器
    connect(this, &QQuickTextArea::textDocumentChanged, this, [this]() {
        // 清理旧的
        if (m_highlighter) {
            m_highlighter->setDocument(nullptr);
            delete m_highlighter;
            m_highlighter = nullptr;
        }

        QQuickTextDocument *qd = textDocument();
        if (qd && qd->textDocument()) {
            m_highlighter = new SimpleLuaHighlighter(qd->textDocument());
            qDebug() << "LuaTextArea: Highlighter created";
        }
    });
}

LuaTextArea::~LuaTextArea()
{
    // 关键：在 TextArea 析构时清理高亮器，
    // 此时 QTextDocument 还未被销毁
    if (m_highlighter) {
        m_highlighter->setDocument(nullptr);
        delete m_highlighter;
        m_highlighter = nullptr;
    }
}
