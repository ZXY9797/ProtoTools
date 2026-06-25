#pragma once

#include <QColor>
#include <QKeyEvent>
#include <QPalette>
#include <QQuickPaintedItem>
#include <QTimer>

/**
 * @brief QML 终端渲染组件 — 移植自 SerialStudio Widgets::Terminal
 *
 * 纯渲染引擎：字符格栅、VT-100 模拟、ANSI 颜色、文本选择。
 * 不包含数据管理（发送、历史、HEX 格式化等 — 这些在 Terminal (ConsoleHandler) 中）。
 *
 * 参考 Serial-Studio 架构：
 *   - Misc::TimerEvents::uiTimeout 驱动 24Hz 刷新（替代自建 QTimer）
 *   - Misc::CommonFonts::monoFont() 驱动默认字体
 *   - keyPressEvent 处理完整 VT-100 键盘映射
 */
class TerminalWidget : public QQuickPaintedItem
{
    Q_OBJECT

    // ── 显示属性 ──
    Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged)
    Q_PROPERTY(QPalette colorPalette READ colorPalette WRITE setColorPalette NOTIFY colorPaletteChanged)
    Q_PROPERTY(int charWidth READ charWidth NOTIFY fontChanged)
    Q_PROPERTY(int charHeight READ charHeight NOTIFY fontChanged)
    Q_PROPERTY(bool autoscroll READ autoscroll WRITE setAutoscroll NOTIFY autoscrollChanged)
    Q_PROPERTY(bool copyAvailable READ copyAvailable NOTIFY selectionChanged)
    Q_PROPERTY(int scrollOffsetY READ scrollOffsetY WRITE setScrollOffsetY NOTIFY scrollOffsetYChanged)
    Q_PROPERTY(int lineCount READ lineCount NOTIFY lineCountChanged)
    Q_PROPERTY(int terminalColumns READ terminalColumns NOTIFY terminalSizeChanged)
    Q_PROPERTY(int terminalRows READ terminalRows NOTIFY terminalSizeChanged)

    // ── 模拟 ──
    Q_PROPERTY(bool vt100emulation READ vt100emulation WRITE setVt100Emulation NOTIFY vt100EmulationChanged)
    Q_PROPERTY(bool ansiColors READ ansiColors WRITE setAnsiColors NOTIFY ansiColorsChanged)

public:
    struct CharColor {
        QColor foreground;
        QColor background;
        CharColor() : foreground(), background() {}
        CharColor(const QColor &fg, const QColor &bg = QColor()) : foreground(fg), background(bg) {}
    };

    enum Direction { LeftDirection, RightDirection };
    Q_ENUM(Direction)

    enum State { Text, Escape, Format, ResetFont, OSC, IgnoreSeq };
    Q_ENUM(State)

    explicit TerminalWidget(QQuickItem *parent = nullptr);
    void paint(QPainter *painter) override;

    // ── 度量 ──
    int charWidth() const;
    int charHeight() const;
    int lineCount() const;
    int linesPerPage() const;
    int scrollOffsetY() const;
    int maxCharsPerLine() const;
    int terminalColumns() const;
    int terminalRows() const;

    // ── 属性 ──
    const QFont &font() const;
    const QPalette &colorPalette() const;
    bool autoscroll() const;
    bool ansiColors() const;
    bool copyAvailable() const;
    bool vt100emulation() const;
    const QPoint &cursorPosition() const;

    // ── 坐标映射 ──
    QPoint positionToCursor(const QPoint &pos) const;

public slots:
    void copy();
    void clear();
    void selectAll();
    void setFont(const QFont &font);
    void setAutoscroll(bool enabled);
    void setScrollOffsetY(int offset);
    void setColorPalette(const QPalette &palette);
    void setAnsiColors(bool enabled);
    void setVt100Emulation(bool enabled);

    // ── 数据追加（被 ConsoleHandler 调用）──
    Q_INVOKABLE void append(const QString &data);

signals:
    void fontChanged();
    void cursorMoved();
    void selectionChanged();
    void autoscrollChanged();
    void colorPaletteChanged();
    void scrollOffsetYChanged();
    void vt100EmulationChanged();
    void ansiColorsChanged();
    void lineCountChanged();
    void terminalSizeChanged();
    void dataSendRequested(const QByteArray &data);

private slots:
    void toggleCursor();
    void loadWelcomeGuide();

private:
    // ── 渲染 ──
    void drawCursor(QPainter *painter, int firstLine, int lastVLine, int lineHeight);
    void renderFastSegment(QPainter *painter, const QString &segment, const QColor &textColor, int x, int y);
    void renderAnsiSegment(QPainter *painter, const QString &segment, int segStart,
                           const QList<CharColor> *colorLine, const QColor &defaultFg, int x, int y);
    void drawSegmentSelection(QPainter *painter, const QString &line, int lineIndex,
                              int segStart, int segEnd, int y);

    // ── 像素↔字符映射 ──
    int findCharAtPixelX(const QString &line, int segStart, int segEnd, int pixelX) const;
    int calcCursorPixelX(QPainter *painter, const QString &line, int segStart, int cursorCol, int segEnd) const;

    // ── 缓冲区 ──
    void initBuffer();
    void appendString(QStringView string);
    void replaceData(int x, int y, const QChar &byte);
    void setCursorPosition(int x, int y);
    void setCursorPosition(const QPoint &pos);
    void removeStringFromCursor(Direction direction = RightDirection, int len = INT_MAX);

    // ── VT-100 ──
    void processText(const QChar &byte, QString &text);
    void processEscape(const QChar &byte, QString &text);
    void processFormat(const QChar &byte, QString &text);
    void processResetFont(const QChar &byte, QString &text);
    void applyAnsiColor(const QList<int> &codes);
    void updateAnsiColorPalette();
    QColor getColor256(int index) const;

    // ── 输入 ──
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    bool shouldEndSelection(const QChar &c);

    // ── 主题 ──
    QPalette m_palette;

    // ── 字体 ──
    QFont m_font;
    int m_cWidth  = 0;
    int m_cHeight = 0;
    int m_borderX = 0;
    int m_borderY = 0;

    // ── 显示 ──
    QStringList m_data;
    QList<QList<CharColor>> m_colorData;

    int m_scrollOffsetY = 0;
    bool m_autoscroll   = true;
    bool m_ansiColors   = false;
    bool m_emulateVt100 = false;
    bool m_cursorVisible  = true;
    bool m_mouseTracking  = false;
    bool m_stateChanged   = false;

    // ── 光标 ──
    QTimer m_cursorTimer;
    QPoint m_cursorPosition;
    QPoint m_selectionEnd;
    QPoint m_selectionStart;
    QPoint m_selectionStartCursor;

    // ── VT-100 状态 ──
    State m_state = Text;
    QList<int> m_formatValues;
    int m_currentFormatValue = 0;
    bool m_privateMode       = false;
    bool m_cursorHidden      = false;
    QPoint m_savedCursorPosition;
    QColor m_currentColor;
    QColor m_currentBgColor;
    QColor m_ansiStandardColors[8];
    QColor m_ansiBrightColors[8];

    static constexpr int MAX_LINES = 1000;
};
