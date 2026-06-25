#include "LuaCodeEditor.h"

#include <QCodeEditor>
#include <QLuaHighlighter>
#include <QLuaCompleter>
#include <QLineNumberArea>
#include <QCoreApplication>
#include <QPainter>
#include <QFile>
#include <QKeyEvent>
#include <QScrollBar>
#include <QVector>

#include "Misc/TimerEvents.h"
#include "Misc/CommonFonts.h"

LuaCodeEditor::~LuaCodeEditor()
{
    delete m_editor;
    m_editor = nullptr;
}

LuaCodeEditor::LuaCodeEditor(QQuickItem *parent)
    : QQuickPaintedItem(parent)
    , m_editor(new QCodeEditor())
{
    setMipmap(false);
    setAntialiasing(false);
    setOpaquePainting(true);
    setFlag(ItemHasContents, true);
    setFlag(ItemIsFocusScope, true);
    setFlag(ItemAcceptsInputMethod, true);
    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);
    setCursor(Qt::IBeamCursor);

    // 配置编辑器
    m_editor->setTabReplace(true);
    m_editor->setTabReplaceSize(4);
    m_editor->setAutoIndentation(true);
    m_editor->setHighlighter(new QLuaHighlighter(m_editor->document()));
    m_editor->setCompleter(new QLuaCompleter(m_editor));
    m_editor->setAutoParentheses(true);
    m_editor->setFont(Misc::CommonFonts::instance().customMonoFont(0.85));

    // 字体变更时更新
    connect(&Misc::CommonFonts::instance(), &Misc::CommonFonts::fontsChanged,
            this, [this]() {
        m_editor->setFont(Misc::CommonFonts::instance().customMonoFont(0.85));
        renderEditor();
    });

    // 安装事件过滤器，拦截快捷键
    m_editor->installEventFilter(this);
    m_editor->viewport()->installEventFilter(this);

    // 加载暗色主题
    Q_INIT_RESOURCE(qcodeeditor_resources);
    QFile file(":/dark_style.xml");
    if (file.open(QFile::ReadOnly)) {
        m_syntaxStyle.load(QString::fromUtf8(file.readAll()));
        m_editor->setSyntaxStyle(&m_syntaxStyle);
        file.close();
    }

    // 连接信号
    connect(m_editor, &QCodeEditor::textChanged, this, [this]() {
        if (!m_updatingText) {
            emit textChanged();
        }
    });

    // 尺寸变化
    connect(this, &QQuickPaintedItem::widthChanged, this, &LuaCodeEditor::resizeEditor);
    connect(this, &QQuickPaintedItem::heightChanged, this, &LuaCodeEditor::resizeEditor);

    // 使用统一定时器渲染
    connect(&Misc::TimerEvents::instance(), &Misc::TimerEvents::uiTimeout,
            this, &LuaCodeEditor::renderEditor);

    // 初始尺寸
    if (width() > 0 && height() > 0) {
        resizeEditor();
    }
}

QString LuaCodeEditor::text() const
{
    return m_editor->toPlainText();
}

void LuaCodeEditor::setText(const QString &text)
{
    if (text == this->text())
        return;

    m_updatingText = true;
    m_editor->setPlainText(text);
    m_editor->document()->clearUndoRedoStacks();
    m_editor->document()->setModified(false);
    m_updatingText = false;
    emit textChanged();
}

bool LuaCodeEditor::running() const
{
    return m_running;
}

void LuaCodeEditor::setRunning(bool running)
{
    if (m_running == running)
        return;
    m_running = running;
    emit runningChanged();
}

void LuaCodeEditor::undo()
{
    m_editor->undo();
}

void LuaCodeEditor::redo()
{
    m_editor->redo();
}

void LuaCodeEditor::cut()
{
    m_editor->cut();
}

void LuaCodeEditor::copy()
{
    m_editor->copy();
}

void LuaCodeEditor::paste()
{
    m_editor->paste();
}

void LuaCodeEditor::selectAll()
{
    m_editor->selectAll();
}

void LuaCodeEditor::paint(QPainter *painter)
{
    if (painter && isVisible()) {
        painter->drawPixmap(0, 0, m_pixmap);
    }
}

void LuaCodeEditor::renderEditor()
{
    if (isVisible() && width() > 0 && height() > 0) {
        m_pixmap = m_editor->grab();
        update();
    }
}

void LuaCodeEditor::resizeEditor()
{
    if (width() > 0 && height() > 0) {
        m_editor->setFixedSize(static_cast<int>(width()), static_cast<int>(height()));
        renderEditor();
    }
}

// ---------- 事件过滤器：拦截快捷键 ----------

bool LuaCodeEditor::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->modifiers() == Qt::ControlModifier) {
            if (ke->key() == Qt::Key_S) {
                emit saveRequested();
                return true;
            }
            if (ke->key() == Qt::Key_Slash) {
                toggleComment();
                return true;
            }
        }
    }
    return QQuickPaintedItem::eventFilter(obj, event);
}

// ---------- 注释/取消注释 ----------

void LuaCodeEditor::toggleComment()
{
    // 参考 VSCode lineComment: trackSelection 方式
    // 核心规则：anchor 和 active 都按「修改位置 <= 自身位置」的累积 delta 偏移
    QTextCursor cursor = m_editor->textCursor();
    int anchorPos = cursor.anchor();
    int activePos = cursor.position();

    cursor.setPosition(anchorPos);
    int anchorBlock = cursor.blockNumber();

    cursor.setPosition(activePos);
    int activeBlock = cursor.blockNumber();

    int startBlock = qMin(anchorBlock, activeBlock);
    int endBlock = qMax(anchorBlock, activeBlock);

    QTextDocument *doc = m_editor->document();
    int totalBlocks = doc->blockCount();
    if (startBlock >= totalBlocks) return;
    if (endBlock >= totalBlocks) endBlock = totalBlocks - 1;

    // 第一遍：判断是否全部已注释（忽略空行）
    bool allCommented = true;
    for (int i = startBlock; i <= endBlock; i++) {
        QString text = doc->findBlockByNumber(i).text();
        QString trimmed = text.trimmed();
        if (!trimmed.isEmpty() && !trimmed.startsWith("--")) {
            allCommented = false;
            break;
        }
    }

    // 保存滚动位置
    QScrollBar *vbar = m_editor->verticalScrollBar();
    int scrollPos = vbar->value();

    // 保存修改前每行的起始位置
    QVector<int> origBlockStarts(endBlock - startBlock + 1);
    for (int i = startBlock; i <= endBlock; i++)
        origBlockStarts[i - startBlock] = doc->findBlockByNumber(i).position();

    // 第二遍：从后往前逐行修改 + 记录每行 delta
    cursor.beginEditBlock();

    QVector<int> deltas(endBlock - startBlock + 1);

    for (int i = endBlock; i >= startBlock; i--) {
        QTextBlock block = doc->findBlockByNumber(i);
        if (!block.isValid()) continue;

        QTextCursor lc(block);
        lc.movePosition(QTextCursor::StartOfBlock);
        lc.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        QString line = lc.selectedText();

        if (allCommented) {
            int idx = line.indexOf("--");
            if (idx >= 0) {
                int len = (line.length() > idx + 2 && line.at(idx + 2) == QLatin1Char(' ')) ? 3 : 2;
                line.remove(idx, len);
                deltas[i - startBlock] = -len;
            }
        } else {
            line.prepend("-- ");
            deltas[i - startBlock] = 3;
        }

        lc.insertText(line);
    }

    cursor.endEditBlock();

    // 恢复滚动位置
    vbar->setValue(scrollPos);

    // 恢复选区：遍历修改行，累积 delta，当修改位置 <= 原始位置时应用
    int runningDelta = 0;
    int newAnchor = anchorPos;
    int newActive = activePos;

    for (int i = startBlock; i <= endBlock; i++) {
        runningDelta += deltas[i - startBlock];
        int origStart = origBlockStarts[i - startBlock];

        if (anchorPos >= origStart)
            newAnchor = anchorPos + runningDelta;
        if (activePos >= origStart)
            newActive = activePos + runningDelta;
    }

    newAnchor = qMax(0, newAnchor);
    newActive = qMax(0, newActive);

    QTextCursor restoreCursor = m_editor->textCursor();
    restoreCursor.setPosition(newAnchor);
    restoreCursor.setPosition(newActive, QTextCursor::KeepAnchor);
    m_editor->setTextCursor(restoreCursor);

    // 触发重绘
    renderEditor();
}

// ---------- 事件转发 ----------

void LuaCodeEditor::keyPressEvent(QKeyEvent *event)
{
    QCoreApplication::sendEvent(m_editor, event);
}

void LuaCodeEditor::keyReleaseEvent(QKeyEvent *event)
{
    QCoreApplication::sendEvent(m_editor, event);
}

void LuaCodeEditor::inputMethodEvent(QInputMethodEvent *event)
{
    QCoreApplication::sendEvent(m_editor, event);
}

void LuaCodeEditor::focusInEvent(QFocusEvent *event)
{
    QCoreApplication::sendEvent(m_editor, event);
}

void LuaCodeEditor::focusOutEvent(QFocusEvent *event)
{
    QCoreApplication::sendEvent(m_editor, event);
}

void LuaCodeEditor::mousePressEvent(QMouseEvent *event)
{
    const auto gutterWidth = m_editor->lineNumberArea()->sizeHint().width();
    QPointF pos = event->position() - QPointF(gutterWidth, 0);
    // 限制在编辑区范围内，防止移出时选中到末尾
    QRectF vr = m_editor->viewport()->rect();
    pos.setX(qBound(0.0, pos.x(), vr.width() - 1.0));
    pos.setY(qBound(0.0, pos.y(), vr.height() - 1.0));
    QMouseEvent copy(event->type(), pos, event->globalPosition(),
                     event->button(), event->buttons(), event->modifiers());
    QCoreApplication::sendEvent(m_editor->viewport(), &copy);
    forceActiveFocus();
}

void LuaCodeEditor::mouseMoveEvent(QMouseEvent *event)
{
    const auto gutterWidth = m_editor->lineNumberArea()->sizeHint().width();
    QPointF pos = event->position() - QPointF(gutterWidth, 0);
    QRectF vr = m_editor->viewport()->rect();
    pos.setX(qBound(0.0, pos.x(), vr.width() - 1.0));
    pos.setY(qBound(0.0, pos.y(), vr.height() - 1.0));
    QMouseEvent copy(event->type(), pos, event->globalPosition(),
                     event->button(), event->buttons(), event->modifiers());
    QCoreApplication::sendEvent(m_editor->viewport(), &copy);
}

void LuaCodeEditor::mouseReleaseEvent(QMouseEvent *event)
{
    const auto gutterWidth = m_editor->lineNumberArea()->sizeHint().width();
    QPointF pos = event->position() - QPointF(gutterWidth, 0);
    QRectF vr = m_editor->viewport()->rect();
    pos.setX(qBound(0.0, pos.x(), vr.width() - 1.0));
    pos.setY(qBound(0.0, pos.y(), vr.height() - 1.0));
    QMouseEvent copy(event->type(), pos, event->globalPosition(),
                     event->button(), event->buttons(), event->modifiers());
    QCoreApplication::sendEvent(m_editor->viewport(), &copy);
}

void LuaCodeEditor::mouseDoubleClickEvent(QMouseEvent *event)
{
    const auto gutterWidth = m_editor->lineNumberArea()->sizeHint().width();
    QPointF pos = event->position() - QPointF(gutterWidth, 0);
    QRectF vr = m_editor->viewport()->rect();
    pos.setX(qBound(0.0, pos.x(), vr.width() - 1.0));
    pos.setY(qBound(0.0, pos.y(), vr.height() - 1.0));
    QMouseEvent copy(event->type(), pos, event->globalPosition(),
                     event->button(), event->buttons(), event->modifiers());
    QCoreApplication::sendEvent(m_editor->viewport(), &copy);
}

void LuaCodeEditor::wheelEvent(QWheelEvent *event)
{
    QCoreApplication::sendEvent(m_editor->viewport(), event);
}

void LuaCodeEditor::hoverEnterEvent(QHoverEvent *event)
{
    setCursor(Qt::IBeamCursor);
    QQuickPaintedItem::hoverEnterEvent(event);
}

void LuaCodeEditor::hoverLeaveEvent(QHoverEvent *event)
{
    setCursor(Qt::ArrowCursor);
    QQuickPaintedItem::hoverLeaveEvent(event);
}
