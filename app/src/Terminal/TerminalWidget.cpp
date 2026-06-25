#include "TerminalWidget.h"

#include <QApplication>
#include <QClipboard>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QPainter>
#include <QStringView>
#include <QWheelEvent>

#include "Misc/TimerEvents.h"
#include "Misc/CommonFonts.h"
#include "Misc/ThemeManager.h"
#include "DataModel/SettingsManager.h"

// ============================================================================
// Constructor (ported from SerialStudio Terminal)
// ============================================================================

TerminalWidget::TerminalWidget(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    initBuffer();

    // QML item flags
    setFlag(ItemHasContents, true);
    setFlag(ItemIsFocusScope, true);
    setFlag(ItemAcceptsInputMethod, true);
    setAcceptedMouseButtons(Qt::AllButtons);
    setActiveFocusOnTab(true);
    setMipmap(true);
    setOpaquePainting(true);

    // 恢复终端渲染设置（必须在 loadWelcomeGuide/clear 之前，clear 会写默认值）
    m_emulateVt100 = SettingsManager::instance()->loadValue("terminal.vt100", false).toBool();
    m_ansiColors = SettingsManager::instance()->loadValue("terminal.ansi", false).toBool();
    m_autoscroll = SettingsManager::instance()->loadValue("terminal.autoscroll", true).toBool();

    // Default font from CommonFonts
    loadWelcomeGuide();
    setFont(Misc::CommonFonts::instance().monoFont());
    connect(&Misc::CommonFonts::instance(), &Misc::CommonFonts::fontsChanged,
            this, [this] { setFont(Misc::CommonFonts::instance().monoFont()); });

    // Theme from ThemeManager
    m_palette = Misc::ThemeManager::instance().palette();
    setFillColor(m_palette.color(QPalette::Base));
    updateAnsiColorPalette();

    // Cursor blink
    m_cursorTimer.start(200);
    m_cursorTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_cursorTimer, &QTimer::timeout, this, &TerminalWidget::toggleCursor);

    // Redraw via TimerEvents (SS architecture)
    connect(&Misc::TimerEvents::instance(), &Misc::TimerEvents::uiTimeout,
            this, [this] {
                if (isVisible() && m_stateChanged) {
                    m_stateChanged = false;
                    update();
                }
            });

    // Auto-scroll to cursor when visible
    connect(this, &TerminalWidget::visibleChanged, this, [this] {
        if (isVisible()) {
            if (autoscroll() && linesPerPage() > 0) {
                int cursorLine   = m_cursorPosition.y();
                int wrappedLines = 1;
                if (cursorLine < m_data.size())
                    wrappedLines = (m_data[cursorLine].length() + maxCharsPerLine() - 1) / maxCharsPerLine();
                int visualBottom = cursorLine + wrappedLines - 1;
                setScrollOffsetY(qMax(0, visualBottom - linesPerPage() + 1));
            }
            update();
        }
    });

}

// ============================================================================
// Metrics
// ============================================================================

int TerminalWidget::charWidth() const       { return m_cWidth; }
int TerminalWidget::charHeight() const      { return m_cHeight; }
int TerminalWidget::lineCount() const       { return m_data.size(); }
int TerminalWidget::scrollOffsetY() const   { return m_scrollOffsetY; }
int TerminalWidget::terminalColumns() const { return maxCharsPerLine(); }
int TerminalWidget::terminalRows() const    { return linesPerPage(); }

int TerminalWidget::linesPerPage() const
{
    if (m_cHeight <= 0) return 0;
    return static_cast<int>(qFloor((height() - 2 * m_borderY) / m_cHeight));
}

int TerminalWidget::maxCharsPerLine() const
{
    if (m_cWidth <= 0) return 84;
    return qMax<int>(84, (width() - 2 * m_borderX) / m_cWidth);
}

// ============================================================================
// Properties
// ============================================================================

const QFont &TerminalWidget::font() const           { return m_font; }
const QPalette &TerminalWidget::colorPalette() const { return m_palette; }
bool TerminalWidget::autoscroll() const             { return m_autoscroll; }
bool TerminalWidget::ansiColors() const             { return m_ansiColors && m_emulateVt100; }
bool TerminalWidget::vt100emulation() const         { return m_emulateVt100; }
const QPoint &TerminalWidget::cursorPosition() const { return m_cursorPosition; }
bool TerminalWidget::copyAvailable() const {
    return (!m_selectionEnd.isNull() || !m_selectionStart.isNull()) && !m_data.isEmpty();
}

// ============================================================================
// Style setters
// ============================================================================

void TerminalWidget::setFont(const QFont &font)
{
    m_font = font;
    m_font.setStyleStrategy(QFont::PreferAntialias);
    QFontMetrics fm(m_font);
    m_cHeight = fm.height();
    m_cWidth  = fm.horizontalAdvance("M");
    m_borderX = qMax(m_cWidth, m_cHeight) / 2;
    m_borderY = qMax(m_cWidth, m_cHeight) / 2;
    emit fontChanged();
}

void TerminalWidget::setColorPalette(const QPalette &palette)
{
    m_palette = palette;
    setFillColor(m_palette.color(QPalette::Base));
    emit colorPaletteChanged();
}

void TerminalWidget::setAutoscroll(bool enabled)        { m_autoscroll = enabled; emit autoscrollChanged(); SettingsManager::instance()->saveValue("terminal.autoscroll", enabled); }
void TerminalWidget::setScrollOffsetY(int offset)       { if (m_scrollOffsetY != offset) { m_scrollOffsetY = offset; emit scrollOffsetYChanged(); } }
void TerminalWidget::setVt100Emulation(bool enabled)    { m_emulateVt100 = enabled; emit vt100EmulationChanged(); SettingsManager::instance()->saveValue("terminal.vt100", enabled); }

void TerminalWidget::setAnsiColors(bool enabled)
{
    m_ansiColors = enabled;
    if (enabled) {
        m_currentColor = QColor();
        m_colorData.reserve(MAX_LINES);
    }
    emit ansiColorsChanged();
    SettingsManager::instance()->saveValue("terminal.ansi", enabled);
}

// ============================================================================
// Geometry
// ============================================================================

void TerminalWidget::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size())
        emit terminalSizeChanged();
}

// ============================================================================
// Coordinate mapping (ported from SerialStudio)
// ============================================================================

int TerminalWidget::findCharAtPixelX(const QString &line, int segStart, int segEnd, int pixelX) const
{
    QFontMetrics fm(m_font);
    int widthSum = 0;
    for (int j = segStart; j < segEnd; ++j) {
        int cw = fm.horizontalAdvance(line[j]);
        if (widthSum + cw > pixelX) return j;
        widthSum += cw;
    }
    return segEnd;
}

int TerminalWidget::calcCursorPixelX(QPainter *painter, const QString &line,
                                      int segStart, int cursorCol, int segEnd) const
{
    int cx = m_borderX;
    for (int j = segStart; j < qMin(cursorCol, segEnd); ++j)
        cx += painter->fontMetrics().horizontalAdvance(line[j]);
    return cx;
}

QPoint TerminalWidget::positionToCursor(const QPoint &pos) const
{
    int localY = (pos.y() - m_borderY) / m_cHeight;
    if (localY < 0) return m_scrollOffsetY < m_data.size() ? QPoint(0, m_scrollOffsetY) : QPoint(0, 0);

    int remainingY = localY;
    for (int i = m_scrollOffsetY; i < m_data.size(); ++i) {
        const QString &line = m_data[i];
        if (line.isEmpty()) { if (remainingY == 0) return QPoint(0, i); remainingY--; continue; }
        int lines = (line.length() + maxCharsPerLine() - 1) / maxCharsPerLine();
        if (remainingY < lines) {
            int segStart = qMax(0, remainingY) * maxCharsPerLine();
            int segEnd   = qMin(segStart + maxCharsPerLine(), line.length());
            return QPoint(findCharAtPixelX(line, segStart, segEnd, pos.x() - m_borderX), i);
        }
        remainingY -= lines;
    }

    if (!m_data.isEmpty()) return QPoint(m_data.last().length(), m_data.size() - 1);
    return QPoint(0, 0);
}

// ============================================================================
// Paint (ported from SerialStudio)
// ============================================================================

void TerminalWidget::paint(QPainter *painter)
{
    if (!isVisible() || !painter) return;

    painter->setFont(m_font);
    int lineHeight      = m_cHeight;
    const int firstLine = m_scrollOffsetY;
    const int lastVLine = qMin(firstLine + linesPerPage(), lineCount() - 1);

    // ── Selection highlights ──
    int y = m_borderY;
    for (int i = firstLine; i <= lastVLine && y < height() - m_borderY; ++i) {
        const QString &line = m_data[i];
        bool lineFullySelected = !m_selectionEnd.isNull() && i >= m_selectionStart.y() && i < m_selectionEnd.y();

        if (line.isEmpty()) {
            if (lineFullySelected)
                painter->fillRect(QRect(m_borderX, y, width() - 2 * m_borderX, m_cHeight),
                                  m_palette.color(QPalette::Highlight));
            y += lineHeight;
            continue;
        }
        if (lineFullySelected) {
            int wrappedLines = qMax(1, (line.length() + maxCharsPerLine() - 1) / maxCharsPerLine());
            for (int w = 0; w < wrappedLines && y < height() - m_borderY; ++w) {
                painter->fillRect(QRect(m_borderX, y, width() - 2 * m_borderX, m_cHeight),
                                  m_palette.color(QPalette::Highlight));
                y += lineHeight;
            }
            continue;
        }

        int start = 0;
        while (start < line.length()) {
            int lineEnd = qMin<int>(start + maxCharsPerLine(), line.length());
            drawSegmentSelection(painter, line, i, start, lineEnd, y);
            y += lineHeight;
            start = lineEnd;
        }
    }

    // ── Text content ──
    y = m_borderY;
    const QColor defaultTextColor = m_palette.color(QPalette::Text);
    for (int i = firstLine; i <= lastVLine && y < height() - m_borderY; ++i) {
        const QString &line = m_data[i];
        if (line.isEmpty()) { y += lineHeight; continue; }

        const QList<CharColor> *colorLine = nullptr;
        if (ansiColors() && i < m_colorData.size()) colorLine = &m_colorData[i];

        int start = 0;
        while (start < line.length()) {
            int end = qMin<int>(start + maxCharsPerLine(), line.length());
            QString segment = line.mid(start, end - start);
            int x = m_borderX;

            if (!colorLine)
                renderFastSegment(painter, segment, defaultTextColor, x, y);
            else
                renderAnsiSegment(painter, segment, start, colorLine, defaultTextColor, x, y);

            y += lineHeight;
            start = end;
        }
    }

    // ── Cursor ──
    if (m_cursorVisible && !m_cursorHidden)
        drawCursor(painter, firstLine, lastVLine, lineHeight);

    // ── Scrollbar ──
    if (!autoscroll() && lineCount() > linesPerPage()) {
        int availH = height() - 2 * m_borderY;
        int sbw = 6;
        int sbh = qMax(20.0, qPow(availH, 2) / lineCount());
        if (sbh > availH / 2) sbh = availH / 2;

        int sx = width() - sbw - m_borderX;
        int sy = (m_scrollOffsetY / static_cast<float>(lineCount() - linesPerPage())) * (availH - sbh) - m_borderY;
        sy = qMax(m_borderY, sy);

        painter->setRenderHint(QPainter::Antialiasing);
        painter->setBrush(m_palette.color(QPalette::Window));
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(QRect(sx, sy, sbw, sbh), sbw / 2, sbw / 2);
    }
}

// ============================================================================
// Segment rendering (ported from SerialStudio)
// ============================================================================

void TerminalWidget::renderFastSegment(QPainter *painter, const QString &segment,
                                        const QColor &textColor, int x, int y)
{
    painter->setPen(textColor);
    int segWidth = painter->fontMetrics().horizontalAdvance(segment);
    painter->drawText(x, y, segWidth, m_cHeight, Qt::AlignLeft | Qt::AlignVCenter, segment);
}

void TerminalWidget::renderAnsiSegment(QPainter *painter, const QString &segment, int segStart,
                                        const QList<CharColor> *colorLine, const QColor &defaultFg,
                                        int x, int y)
{
    const auto &fm = painter->fontMetrics();
    int xPos = x, j = 0;

    while (j < segment.length()) {
        int charIndex = segStart + j;
        QColor runFg = defaultFg;
        QColor runBg;

        if (colorLine && charIndex < colorLine->size()) {
            const CharColor &cc = (*colorLine)[charIndex];
            runFg = cc.foreground.isValid() ? cc.foreground : defaultFg;
            runBg = cc.background;
        }

        int runStart = j;
        ++j;
        while (j < segment.length()) {
            int ci = segStart + j;
            QColor fg = defaultFg;
            QColor bg;
            if (colorLine && ci < colorLine->size()) {
                const CharColor &cc = (*colorLine)[ci];
                fg = cc.foreground.isValid() ? cc.foreground : defaultFg;
                bg = cc.background;
            }
            if (fg != runFg || bg != runBg) break;
            ++j;
        }

        auto runText = QStringView(segment).mid(runStart, j - runStart);
        int runWidth = fm.horizontalAdvance(runText.toString());

        if (runBg.isValid())
            painter->fillRect(xPos, y, runWidth, m_cHeight, runBg);

        painter->setPen(runFg);
        painter->drawText(xPos, y, runWidth, m_cHeight, Qt::AlignVCenter, runText.toString());
        xPos += runWidth;
    }
}

void TerminalWidget::drawSegmentSelection(QPainter *painter, const QString &line, int lineIndex,
                                           int segStart, int segEnd, int y)
{
    if (m_selectionEnd.isNull()) return;
    if (lineIndex < m_selectionStart.y() || lineIndex > m_selectionEnd.y()) return;

    int selStartX, selEndX;
    if (lineIndex == m_selectionStart.y() && lineIndex == m_selectionEnd.y()) {
        selStartX = qMax(m_selectionStart.x(), segStart);
        selEndX   = qMin(m_selectionEnd.x(), segEnd);
    } else if (lineIndex == m_selectionStart.y()) {
        selStartX = qMax(m_selectionStart.x(), segStart); selEndX = segEnd;
    } else if (lineIndex == m_selectionEnd.y()) {
        selStartX = segStart; selEndX = qMin(m_selectionEnd.x(), segEnd);
    } else {
        selStartX = segStart; selEndX = segEnd;
    }

    if (selStartX >= selEndX) return;

    int startX = m_borderX;
    int selectionWidth = 0;
    const int maxWidth = width() - 2 * m_borderX;

    for (int j = segStart; j < selEndX; ++j) {
        int cw = painter->fontMetrics().horizontalAdvance(line[j]);
        if (j < selStartX) startX += cw;
        else selectionWidth += cw;
    }

    selectionWidth = qMin(selectionWidth, maxWidth - (startX - m_borderX));
    painter->fillRect(QRect(startX, y, selectionWidth, m_cHeight),
                      m_palette.color(QPalette::Highlight));
}

// ============================================================================
// Cursor (ported from SerialStudio)
// ============================================================================

void TerminalWidget::toggleCursor()
{
    m_stateChanged  = true;
    m_cursorVisible = !m_cursorVisible;
}

void TerminalWidget::drawCursor(QPainter *painter, int firstLine, int lastVLine, int lineHeight)
{
    int cursorLine = m_cursorPosition.y();
    int cursorCol  = m_cursorPosition.x();
    int vy = m_borderY;
    bool drawn = false;

    for (int i = firstLine; i <= lastVLine && i < m_data.size(); ++i) {
        const QString &line = m_data[i];

        if (line.isEmpty()) {
            if (i == cursorLine) {
                painter->setPen(m_palette.color(QPalette::Text));
                painter->drawText(m_borderX, vy + m_cHeight, QStringLiteral("█"));
                drawn = true; break;
            }
            vy += lineHeight;
            continue;
        }

        int start = 0;
        while (start < line.length()) {
            int end = qMin<int>(start + maxCharsPerLine(), line.length());
            if (i == cursorLine && cursorCol >= start && cursorCol <= end) {
                int cx = calcCursorPixelX(painter, line, start, cursorCol, end);
                painter->setPen(m_palette.color(QPalette::Text));
                painter->drawText(cx, vy + m_cHeight, QStringLiteral("█"));
                drawn = true; break;
            }
            vy += lineHeight;
            start = end;
        }
        if (drawn) break;
    }

    if (!drawn && cursorLine >= m_data.size()) {
        painter->setPen(m_palette.color(QPalette::Text));
        painter->drawText(m_borderX, vy + m_cHeight, QStringLiteral("█"));
    }
}

void TerminalWidget::setCursorPosition(int x, int y)
{
    QPoint clamped(x, qBound(0, y, MAX_LINES));
    if (m_cursorPosition != clamped) { m_cursorPosition = clamped; emit cursorMoved(); }
}

void TerminalWidget::setCursorPosition(const QPoint &pos) { setCursorPosition(pos.x(), pos.y()); }

// ============================================================================
// Data append (called by ConsoleHandler)
// ============================================================================

void TerminalWidget::append(const QString &data)
{
    if (data.isEmpty()) return;

    QString text;
    text.reserve(data.size());
    int pos = 0;
    int len = data.size();

    while (pos < len) {
        if (m_state == Text) {
            int runStart = pos;
            while (pos < len) {
                auto ch = data[pos].unicode();
                if (ch == 0x1b || ch == '\n' || ch == '\r' || ch == '\b' || ch == 0x7F || ch == '\t')
                    break;
                if (ch >= 0x20) { ++pos; continue; }
                break;
            }
            if (pos > runStart) text.append(QStringView(data).mid(runStart, pos - runStart));
            if (pos < len && m_state == Text) { processText(data[pos], text); ++pos; }
            continue;
        }

        auto byte = data[pos];
        switch (m_state) {
        case Escape:    processEscape(byte, text); break;
        case Format:    processFormat(byte, text); break;
        case ResetFont: processResetFont(byte, text); break;
        case OSC:
            if (byte.toLatin1() == 0x07) m_state = Text;
            else if (byte.toLatin1() == 0x1b) m_state = Escape;
            break;
        case IgnoreSeq:
            if ((byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z')) m_state = Text;
            break;
        default: break;
        }
        ++pos;
    }

    appendString(text);
    m_stateChanged = true;
    emit lineCountChanged();
}

// ============================================================================
// Buffer (ported from SerialStudio)
// ============================================================================

void TerminalWidget::initBuffer()
{
    m_data.clear(); m_data.squeeze();
    m_scrollOffsetY = 0;
    m_data.reserve(MAX_LINES);
    m_colorData.clear(); m_colorData.squeeze();
    if (ansiColors()) { m_colorData.reserve(MAX_LINES); m_currentColor = QColor(); }
}

void TerminalWidget::clear()
{
    bool savedAutoscroll = m_autoscroll;
    initBuffer();
    setCursorPosition(0, 0);
    m_autoscroll = savedAutoscroll;   // preserve saved setting
    m_selectionStart = QPoint(); m_selectionEnd = QPoint();
    m_stateChanged = true;
    emit selectionChanged();
    emit lineCountChanged();
}

void TerminalWidget::loadWelcomeGuide()
{
    clear();
    // Empty welcome — just clears the terminal
}

void TerminalWidget::appendString(QStringView string)
{
    // Drop oldest lines when full
    int linesToDrop = m_data.size() - MAX_LINES + 1;
    if (m_data.size() >= MAX_LINES && linesToDrop > 0) {
        m_data.erase(m_data.begin(), m_data.begin() + linesToDrop);
        if (ansiColors() && m_colorData.size() >= linesToDrop)
            m_colorData.erase(m_colorData.begin(), m_colorData.begin() + linesToDrop);
        if (m_cursorPosition.y() >= linesToDrop)
            m_cursorPosition.setY(m_cursorPosition.y() - linesToDrop);
        else m_cursorPosition.setY(0);
        m_scrollOffsetY = qMax(0, m_scrollOffsetY - linesToDrop);
    }

    for (const auto &ch : string) {
        int cx = m_cursorPosition.x(), cy = m_cursorPosition.y();
        replaceData(cx, cy, ch);
        setCursorPosition(cx + 1, cy);
        if (m_cursorPosition.x() >= maxCharsPerLine())
            setCursorPosition(0, m_cursorPosition.y() + 1);
    }

    if (autoscroll()) {
        int cursorLine   = m_cursorPosition.y();
        int wrappedLines = 1;
        if (cursorLine < m_data.size())
            wrappedLines = (m_data[cursorLine].length() + maxCharsPerLine() - 1) / maxCharsPerLine();
        int visualBottom = cursorLine + wrappedLines - 1;
        m_scrollOffsetY  = qMax(0, visualBottom - linesPerPage() + 1);
        if (isVisible()) emit scrollOffsetYChanged();
    }
}

void TerminalWidget::replaceData(int x, int y, const QChar &byte)
{
    if (y >= m_data.size()) m_data.resize(y + 1);
    QString &line = m_data[y];

    if (ansiColors()) {
        if (y >= m_colorData.size()) m_colorData.resize(y + 1);
        QList<CharColor> &colorLine = m_colorData[y];
        if (x > line.size()) {
            for (int i = line.size(); i < x; ++i) colorLine.append(CharColor());
        }
        while (colorLine.size() < line.size()) colorLine.append(CharColor());
        if (x >= 0 && x < colorLine.size()) colorLine[x] = CharColor(m_currentColor, m_currentBgColor);
        else if (x >= 0) colorLine.append(CharColor(m_currentColor, m_currentBgColor));
    }

    if (x > line.size()) line = line.leftJustified(x, ' ');
    if (x >= 0 && x < line.size()) line[x] = byte.isPrint() ? byte : ' ';
    else if (x >= 0) line.append(byte.isPrint() ? byte : ' ');
}

void TerminalWidget::removeStringFromCursor(Direction direction, int len)
{
    int posX = m_cursorPosition.x(), posY = m_cursorPosition.y();
    if (len < 0) len = INT_MAX;

    int removeSize;
    if (direction == RightDirection)
        removeSize = qMin<int>(m_data[posY].size() - posX, len);
    else
        removeSize = qMin(len, m_cursorPosition.x());

    const QChar clearChar('\x7F');
    for (int i = 0; i < removeSize; ++i) {
        int offset = (direction == LeftDirection) ? -1 : i;
        replaceData(m_cursorPosition.x() + offset, posY, clearChar);
    }
}

// ============================================================================
// VT-100 (ported from SerialStudio)
// ============================================================================

void TerminalWidget::processText(const QChar &byte, QString &text)
{
    if (byte.toLatin1() == 0x1b && vt100emulation()) {
        appendString(text); text.clear(); m_state = Escape;
    } else if (byte == '\r' && vt100emulation()) {
        appendString(text); text.clear(); setCursorPosition(0, m_cursorPosition.y());
    } else if (byte == '\n') {
        appendString(text); text.clear(); setCursorPosition(0, m_cursorPosition.y() + 1);
    } else if (byte == '\b' && vt100emulation()) {
        if (m_cursorPosition.x()) {
            appendString(text); text.clear(); setCursorPosition(m_cursorPosition.x() - 1, m_cursorPosition.y());
        }
    } else if (byte.toLatin1() == 0x7F && vt100emulation()) {
        appendString(text); text.clear(); removeStringFromCursor(RightDirection, 1);
    } else if (byte == '\t' && vt100emulation()) {
        appendString(text); text.clear();
        int nextTab = (m_cursorPosition.x() / 8 + 1) * 8;
        text.fill(' ', nextTab - m_cursorPosition.x());
        appendString(text); text.clear();
    } else if (byte.isPrint())
        text.append(byte);
}

void TerminalWidget::processEscape(const QChar &byte, QString &text)
{
    (void)text;
    m_formatValues.clear(); m_currentFormatValue = 0; m_privateMode = false;

    if (byte == '[') m_state = Format;
    else if (byte == '(') m_state = ResetFont;
    else if (byte == ']') m_state = OSC;
    else if (byte == '7') { m_savedCursorPosition = m_cursorPosition; m_state = Text; }
    else if (byte == '8') { setCursorPosition(m_savedCursorPosition); m_state = Text; }
    else if (byte == 'M') { setCursorPosition(m_cursorPosition.x(), qMax(0, m_cursorPosition.y() - 1)); m_state = Text; }
    else m_state = Text;
}

void TerminalWidget::processFormat(const QChar &byte, QString &text)
{
    (void)text;

    if (byte >= '0' && byte <= '9') {
        m_currentFormatValue = m_currentFormatValue * 10 + (byte.cell() - '0');
    } else if (byte == '?' || byte == '>' || byte == '=') {
        m_privateMode = true;
    } else {
        if (byte == ';') { m_formatValues.append(m_currentFormatValue); m_currentFormatValue = 0; m_state = Format; return; }
        if (byte == 'm') { if (!m_privateMode) { m_formatValues.append(m_currentFormatValue); if (ansiColors()) applyAnsiColor(m_formatValues); } m_state = Text; return; }
        if (byte == 's') { if (!m_privateMode) m_savedCursorPosition = m_cursorPosition; m_state = Text; return; }
        if (byte == 'u') { if (!m_privateMode) setCursorPosition(m_savedCursorPosition); m_state = Text; return; }
        if (byte >= 'A' && byte <= 'F') {
            if (!m_privateMode) {
                int v = m_currentFormatValue ? m_currentFormatValue : 1;
                switch (byte.toLatin1()) {
                case 'A': setCursorPosition(m_cursorPosition.x(), qMax(0, m_cursorPosition.y() - v)); break;
                case 'B': setCursorPosition(m_cursorPosition.x(), m_cursorPosition.y() + v); break;
                case 'C': setCursorPosition(m_cursorPosition.x() + v, m_cursorPosition.y()); break;
                case 'D': setCursorPosition(qMax(0, m_cursorPosition.x() - v), m_cursorPosition.y()); break;
                case 'E': setCursorPosition(0, m_cursorPosition.y() + v); break;
                case 'F': setCursorPosition(0, qMax(0, m_cursorPosition.y() - v)); break;
                default: break;
                }
            }
            m_state = Text; return;
        }
        if (byte == 'H' || byte == 'f') {
            if (!m_privateMode) {
                int row = qMax(0, m_formatValues.value(0, 1) - 1);
                int col = qMax(0, m_currentFormatValue > 0 ? m_currentFormatValue - 1 : m_formatValues.value(1, 1) - 1);
                setCursorPosition(col, row);
            }
            m_state = Text; return;
        }
        if (byte == 'G') { if (!m_privateMode) setCursorPosition(qMax(0, m_currentFormatValue > 0 ? m_currentFormatValue - 1 : 0), m_cursorPosition.y()); m_state = Text; return; }
        if (byte == 'd') { if (!m_privateMode) setCursorPosition(m_cursorPosition.x(), qMax(0, m_currentFormatValue > 0 ? m_currentFormatValue - 1 : 0)); m_state = Text; return; }
        if (byte == 'J') {
            if (!m_privateMode) {
                int cy = m_cursorPosition.y();
                switch (m_currentFormatValue) {
                case 0: removeStringFromCursor(RightDirection); if (cy + 1 < m_data.size()) { m_data.erase(m_data.begin() + cy + 1, m_data.end()); if (ansiColors() && cy + 1 < m_colorData.size()) m_colorData.erase(m_colorData.begin() + cy + 1, m_colorData.end()); } break;
                case 1: removeStringFromCursor(LeftDirection); if (cy > 0) { m_data.erase(m_data.begin(), m_data.begin() + cy); if (ansiColors() && cy < m_colorData.size()) m_colorData.erase(m_colorData.begin(), m_colorData.begin() + cy); } setCursorPosition(m_cursorPosition.x(), 0); break;
                case 2: case 3: clear(); break;
                default: break;
                }
            }
            m_state = Text; return;
        }
        if (byte == 'K') {
            if (!m_privateMode) {
                switch (m_currentFormatValue) {
                case 0: removeStringFromCursor(RightDirection); break;
                case 1: removeStringFromCursor(LeftDirection); break;
                case 2: removeStringFromCursor(RightDirection); removeStringFromCursor(LeftDirection); break;
                default: break;
                }
            }
            m_state = Text; return;
        }
        if (byte == 'P') { if (!m_privateMode) { removeStringFromCursor(LeftDirection, m_currentFormatValue); removeStringFromCursor(RightDirection); } m_state = Text; return; }
        if (byte == 'h' || byte == 'l') { if (m_privateMode && m_currentFormatValue == 25) { m_cursorHidden = (byte == 'l'); m_stateChanged = true; } m_state = Text; return; }
        if ((byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z')) { m_state = Text; return; }
        m_state = Text;
    }
}

void TerminalWidget::processResetFont(const QChar &byte, QString &text)
{
    (void)byte; (void)text; m_state = Text;
}

// ============================================================================
// ANSI Color (ported from SerialStudio)
// ============================================================================

void TerminalWidget::applyAnsiColor(const QList<int> &codes)
{
    for (int i = 0; i < codes.size(); ++i) {
        int code = codes[i];
        if (code == 0) { m_currentColor = QColor(); m_currentBgColor = QColor(); }
        else if (code == 1) { if (!m_currentColor.isValid()) m_currentColor = m_palette.color(QPalette::Text); m_currentColor = m_currentColor.lighter(130); }
        else if (code >= 30 && code <= 37) m_currentColor = m_ansiStandardColors[code - 30];
        else if (code >= 40 && code <= 47) m_currentBgColor = m_ansiStandardColors[code - 40];
        else if (code >= 90 && code <= 97) m_currentColor = m_ansiBrightColors[code - 90];
        else if (code >= 100 && code <= 107) m_currentBgColor = m_ansiBrightColors[code - 100];
        else if (code == 38 && i + 2 < codes.size() && codes[i + 1] == 5) { m_currentColor = getColor256(codes[i + 2]); i += 2; }
        else if (code == 48 && i + 2 < codes.size() && codes[i + 1] == 5) { m_currentBgColor = getColor256(codes[i + 2]); i += 2; }
        else if (code == 38 && i + 4 < codes.size() && codes[i + 1] == 2) { m_currentColor = QColor(codes[i + 2], codes[i + 3], codes[i + 4]); i += 4; }
        else if (code == 48 && i + 4 < codes.size() && codes[i + 1] == 2) { m_currentBgColor = QColor(codes[i + 2], codes[i + 3], codes[i + 4]); i += 4; }
    }
}

void TerminalWidget::updateAnsiColorPalette()
{
    m_ansiStandardColors[0] = QColor(0, 0, 0);
    m_ansiStandardColors[1] = QColor(205, 49, 49);
    m_ansiStandardColors[2] = QColor(13, 188, 121);
    m_ansiStandardColors[3] = QColor(229, 229, 16);
    m_ansiStandardColors[4] = QColor(36, 114, 200);
    m_ansiStandardColors[5] = QColor(188, 63, 188);
    m_ansiStandardColors[6] = QColor(17, 168, 205);
    m_ansiStandardColors[7] = QColor(229, 229, 229);

    m_ansiBrightColors[0] = QColor(102, 102, 102);
    m_ansiBrightColors[1] = QColor(241, 76, 76);
    m_ansiBrightColors[2] = QColor(35, 209, 139);
    m_ansiBrightColors[3] = QColor(245, 245, 67);
    m_ansiBrightColors[4] = QColor(59, 142, 234);
    m_ansiBrightColors[5] = QColor(214, 112, 214);
    m_ansiBrightColors[6] = QColor(41, 184, 219);
    m_ansiBrightColors[7] = QColor(255, 255, 255);
}

QColor TerminalWidget::getColor256(int index) const
{
    if (index < 8) {
        static const QColor c[8] = {{0,0,0},{170,0,0},{0,170,0},{170,85,0},{0,0,170},{170,0,170},{0,170,170},{170,170,170}};
        return c[index];
    }
    if (index < 16) {
        static const QColor c[8] = {{85,85,85},{255,85,85},{85,255,85},{255,255,85},{85,85,255},{255,85,255},{85,255,255},{255,255,255}};
        return c[index - 8];
    }
    if (index < 232) {
        int a = index - 16;
        return QColor((a/36)%6 ? ((a/36)%6)*40+55 : 0, (a/6)%6 ? ((a/6)%6)*40+55 : 0, a%6 ? (a%6)*40+55 : 0);
    }
    int g = 8 + (index - 232) * 10;
    return QColor(g, g, g);
}

// ============================================================================
// Mouse (ported from SerialStudio)
// ============================================================================

void TerminalWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_mouseTracking        = true;
        m_selectionStartCursor = positionToCursor(event->pos());
        m_selectionStart       = m_selectionStartCursor;
        m_selectionEnd         = m_selectionStartCursor;
        m_stateChanged         = true;
        forceActiveFocus();
        emit selectionChanged();
    }
}

void TerminalWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_mouseTracking) return;
    QPoint cur = positionToCursor(event->pos());

    if ((m_selectionStartCursor.y() > cur.y()) || (m_selectionStartCursor.y() == cur.y() && m_selectionStartCursor.x() > cur.x())) {
        m_selectionStart = cur; m_selectionEnd = m_selectionStartCursor;
    } else {
        m_selectionStart = m_selectionStartCursor; m_selectionEnd = cur;
    }
    m_stateChanged = true;
    emit selectionChanged();
}

void TerminalWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_selectionStart == m_selectionEnd) { m_selectionStart = QPoint(); m_selectionEnd = QPoint(); }
        m_selectionStartCursor = QPoint(); m_mouseTracking = false;
        m_stateChanged = true; emit selectionChanged();
    }
}

void TerminalWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    auto pos = positionToCursor(event->pos());
    if (pos.y() >= 0 && pos.y() < m_data.size()) {
        const QString &line = m_data[pos.y()];
        int ws = pos.x(), we = pos.x();
        while (ws > 0 && !shouldEndSelection(line[ws - 1])) ws--;
        while (we < line.size() && !shouldEndSelection(line[we])) we++;
        m_selectionStart = QPoint(ws, pos.y()); m_selectionEnd = QPoint(we, pos.y());
        m_stateChanged = true; emit selectionChanged();
    }
}

void TerminalWidget::wheelEvent(QWheelEvent *event)
{
    int steps = event->angleDelta().y();
    if (steps > 0) steps = qMax(1, steps / m_cHeight);
    else if (steps < 0) steps = qMin(-1, steps / m_cHeight);

    if (steps > 0 && autoscroll() && linesPerPage() < lineCount()) setAutoscroll(false);

    if (steps != 0) {
        int maxScroll = qMax(0, lineCount() - linesPerPage() + 2);
        int offset = m_scrollOffsetY - steps;
        offset = qBound(0, offset, maxScroll);
        if (offset == maxScroll && !autoscroll()) setAutoscroll(true);
        setScrollOffsetY(offset);
    }
    m_stateChanged = true;
    event->accept();
}

// ============================================================================
// Keyboard (ported from SerialStudio)
// ============================================================================

void TerminalWidget::keyPressEvent(QKeyEvent *event)
{
    if (!vt100emulation()) {
        if (event->matches(QKeySequence::Copy)) { copy(); event->accept(); return; }
        QQuickPaintedItem::keyPressEvent(event);
        return;
    }

    QByteArray seq;
    const Qt::KeyboardModifiers mods = event->modifiers();
    int key = event->key();

    // Ctrl+letter → control code
    if ((mods & Qt::ControlModifier) && key >= Qt::Key_A && key <= Qt::Key_Z) {
        seq.append(char(key - Qt::Key_A + 1));
        emit dataSendRequested(seq);
        event->accept();
        return;
    }

    // Ctrl+[ → ESC
    if ((mods & Qt::ControlModifier) && key == Qt::Key_BracketLeft) {
        seq.append('\x1b');
        emit dataSendRequested(seq);
        event->accept();
        return;
    }

    switch (key) {
    case Qt::Key_Return: case Qt::Key_Enter: seq.append('\r'); break;
    case Qt::Key_Backspace: seq.append('\x7f'); break;
    case Qt::Key_Delete: seq.append("\x1b[3~"); break;
    case Qt::Key_Tab: seq.append('\t'); break;
    case Qt::Key_Backtab: seq.append("\x1b[Z"); break;
    case Qt::Key_Escape: seq.append('\x1b'); break;
    case Qt::Key_Up: seq.append("\x1b[A"); break;
    case Qt::Key_Down: seq.append("\x1b[B"); break;
    case Qt::Key_Right: seq.append("\x1b[C"); break;
    case Qt::Key_Left: seq.append("\x1b[D"); break;
    case Qt::Key_Home: seq.append("\x1b[H"); break;
    case Qt::Key_End: seq.append("\x1b[F"); break;
    case Qt::Key_PageUp: seq.append("\x1b[5~"); break;
    case Qt::Key_PageDown: seq.append("\x1b[6~"); break;
    case Qt::Key_Insert: seq.append("\x1b[2~"); break;
    case Qt::Key_F1: seq.append("\x1bOP"); break;
    case Qt::Key_F2: seq.append("\x1bOQ"); break;
    case Qt::Key_F3: seq.append("\x1bOR"); break;
    case Qt::Key_F4: seq.append("\x1bOS"); break;
    case Qt::Key_F5: seq.append("\x1b[15~"); break;
    case Qt::Key_F6: seq.append("\x1b[17~"); break;
    case Qt::Key_F7: seq.append("\x1b[18~"); break;
    case Qt::Key_F8: seq.append("\x1b[19~"); break;
    case Qt::Key_F9: seq.append("\x1b[20~"); break;
    case Qt::Key_F10: seq.append("\x1b[21~"); break;
    case Qt::Key_F11: seq.append("\x1b[23~"); break;
    case Qt::Key_F12: seq.append("\x1b[24~"); break;
    default:
        if (!event->text().isEmpty()) seq = event->text().toUtf8();
        break;
    }

    if (!seq.isEmpty()) {
        emit dataSendRequested(seq);
        event->accept();
        return;
    }

    QQuickPaintedItem::keyPressEvent(event);
}

bool TerminalWidget::shouldEndSelection(const QChar &c)
{
    return c.isSpace() || c.isNonCharacter() || (!c.isLetter() && !c.isNumber());
}

// ============================================================================
// Clipboard (ported from SerialStudio)
// ============================================================================

void TerminalWidget::copy()
{
    if (!copyAvailable()) return;
    QString text;
    QPoint start = m_selectionStart, end = m_selectionEnd;
    if (start.y() > end.y() || (start.y() == end.y() && start.x() > end.x()))
        std::swap(start, end);

    for (int li = start.y(); li <= end.y(); ++li) {
        const QString &line = m_data[li];
        int sx = (li == start.y()) ? start.x() : 0;
        int ex = (li == end.y()) ? end.x() : line.size();
        if (start.y() == end.y() && start.x() > end.x()) std::swap(sx, ex);
        if (sx < ex) text.append(line.mid(sx, ex - sx));
        if (li != end.y() || (sx == 0 && ex == line.size())) text.append('\n');
    }
    QGuiApplication::clipboard()->setText(text);
}

void TerminalWidget::selectAll()
{
    if (m_data.isEmpty()) return;
    m_selectionStart = QPoint(0, 0);
    m_selectionEnd   = QPoint(m_data.last().size(), m_data.size() - 1);
    m_selectionStartCursor = m_selectionStart;
    m_stateChanged = true;
    emit selectionChanged();
}
