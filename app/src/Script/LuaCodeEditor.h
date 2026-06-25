#pragma once

#include <QQuickPaintedItem>
#include <QSyntaxStyle>

class QCodeEditor;

class LuaCodeEditor : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(bool running READ running WRITE setRunning NOTIFY runningChanged)

public:
    explicit LuaCodeEditor(QQuickItem *parent = nullptr);
    ~LuaCodeEditor() override;

    QString text() const;
    void setText(const QString &text);

    bool running() const;
    void setRunning(bool running);

signals:
    void textChanged();
    void runningChanged();
    void saveRequested();

public slots:
    void undo();
    void redo();
    void cut();
    void copy();
    void paste();
    void selectAll();

protected:
    void paint(QPainter *painter) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void hoverEnterEvent(QHoverEvent *event) override;
    void hoverLeaveEvent(QHoverEvent *event) override;

    void toggleComment();
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void renderEditor();
    void resizeEditor();

private:
    QCodeEditor *m_editor;
    QSyntaxStyle m_syntaxStyle;
    QPixmap m_pixmap;
    bool m_running = false;
    bool m_updatingText = false;
};
