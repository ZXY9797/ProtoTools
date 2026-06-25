#include "Terminal.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QDir>
#include <QStandardPaths>

#include "DataModel/SettingsManager.h"

#include <cctype>

Terminal::Terminal(QObject *parent)
    : QObject(parent)
{
    // 缓冲定时器：单次触发，用于合并 OS 分包到达的数据
    m_flushRxTimer = new QTimer(this);
    m_flushRxTimer->setSingleShot(true);
    m_flushRxTimer->setInterval(FLUSH_INTERVAL_MS);
    connect(m_flushRxTimer, &QTimer::timeout, this, &Terminal::flushRxBuffer);

    m_flushTxTimer = new QTimer(this);
    m_flushTxTimer->setSingleShot(true);
    m_flushTxTimer->setInterval(FLUSH_INTERVAL_MS);
    connect(m_flushTxTimer, &QTimer::timeout, this, &Terminal::flushTxBuffer);

    // 恢复保存的设置（只读，默认值由 ModuleManager 统一写入）
    m_displayMode = SettingsManager::instance()->loadValue("terminal.displayMode", 0).toInt();
    m_lineEnding = SettingsManager::instance()->loadValue("terminal.lineEnding", 1).toInt();
    m_dataMode = SettingsManager::instance()->loadValue("terminal.dataMode", 0).toInt();
    m_echo = SettingsManager::instance()->loadValue("terminal.echo", true).toBool();
    m_showTimestamp = SettingsManager::instance()->loadValue("terminal.showTimestamp", false).toBool();
    m_hexMode = SettingsManager::instance()->loadValue("terminal.hexMode", false).toBool();
}

// ============================================================================
// 属性访问
// ============================================================================

int Terminal::lineCount() const
{
    QMutexLocker lock(&m_mutex);
    return m_lines.size();
}

bool Terminal::hexMode() const      { return m_hexMode; }
bool Terminal::showTimestamp() const { return m_showTimestamp; }
bool Terminal::autoScroll() const    { return m_autoScroll; }
bool Terminal::echo() const          { return m_echo; }
int Terminal::dataMode() const       { return m_dataMode; }
int Terminal::lineEnding() const     { return m_lineEnding; }
int Terminal::displayMode() const    { return m_displayMode; }

QStringList Terminal::lineEndings() const
{
    return {QStringLiteral("No Line Ending"),
            QStringLiteral("New Line"),
            QStringLiteral("Carriage Return"),
            QStringLiteral("Both NL & CR")};
}

QStringList Terminal::dataModes() const
{
    return {QStringLiteral("ASCII"), QStringLiteral("HEX")};
}

QStringList Terminal::displayModes() const
{
    return {QStringLiteral("Plain Text"), QStringLiteral("Hexadecimal")};
}

QString Terminal::currentHistoryString() const
{
    if (m_historyItem >= 0 && m_historyItem < m_historyItems.count())
        return m_historyItems.at(m_historyItem);
    return {};
}

QString Terminal::displayText() const
{
    QMutexLocker lock(&m_mutex);
    return m_displayText;
}

QString Terminal::getLine(int index) const
{
    QMutexLocker lock(&m_mutex);
    if (index < 0 || index >= m_lines.size())
        return {};
    return m_lines.at(index);
}

// ============================================================================
// 操作
// ============================================================================

void Terminal::clear()
{
    {
        QMutexLocker lock(&m_mutex);
        m_lines.clear();
        m_displayText.clear();
    }
    m_rxBuffer.clear();
    m_txBuffer.clear();
    m_flushRxTimer->stop();
    m_flushTxTimer->stop();
    emit displayString(QString());
    emit lineCountChanged();
    emit displayTextChanged();
}

void Terminal::send(const QString &data)
{
    if (data.isEmpty() && m_lineEnding == static_cast<int>(LineEnding::NoLineEnding))
        return;

    if (!data.isEmpty())
        addToHistory(data);

    QByteArray bin = parseSendData(data);

    switch (static_cast<LineEnding>(m_lineEnding)) {
    case LineEnding::NoLineEnding: break;
    case LineEnding::NewLine:      bin.append('\n'); break;
    case LineEnding::CarriageReturn: bin.append('\r'); break;
    case LineEnding::Both:         bin.append('\r'); bin.append('\n'); break;
    }

    if (!bin.isEmpty())
        emit dataSendRequested(bin);
}

void Terminal::historyUp()
{
    if (m_historyItem > 0) { --m_historyItem; emit historyItemChanged(); }
}

void Terminal::historyDown()
{
    if (m_historyItem < m_historyItems.count() - 1) {
        ++m_historyItem; emit historyItemChanged();
    } else if (m_historyItem == m_historyItems.count() - 1) {
        m_historyItem = m_historyItems.count(); emit historyItemChanged();
    }
}

// ============================================================================
// HEX 辅助
// ============================================================================

bool Terminal::validateUserHex(const QString &text)
{
    QString clean = text.simplified().remove(' ');
    static QRegularExpression hexPattern("^[0-9A-Fa-f]*$");
    return hexPattern.match(clean).hasMatch() && clean.length() % 2 == 0;
}

QString Terminal::formatUserHex(const QString &text)
{
    static QRegularExpression nonHex("[^0-9A-Fa-f]");
    QString data = text.simplified().remove(nonHex);

    QString str;
    for (int i = 0; i < data.length(); ++i) {
        str.append(data.at(i));
        if ((i + 1) % 2 == 0 && i > 0) str.append(' ');
    }
    while (str.endsWith(' ')) str.chop(1);
    return str;
}

// ============================================================================
// 显示设置
// ============================================================================

void Terminal::setHexMode(bool hex)
{
    if (m_hexMode != hex) { m_hexMode = hex; SettingsManager::instance()->saveValue("terminal.hexMode", hex); emit hexModeChanged(); }
}

void Terminal::setEcho(bool enabled)
{
    if (m_echo != enabled) { m_echo = enabled; SettingsManager::instance()->saveValue("terminal.echo", enabled); emit echoChanged(); }
}

void Terminal::setShowTimestamp(bool show)
{
    if (m_showTimestamp != show) { m_showTimestamp = show; SettingsManager::instance()->saveValue("terminal.showTimestamp", show); emit showTimestampChanged(); }
}

void Terminal::setAutoScroll(bool scroll)
{
    if (m_autoScroll != scroll) { m_autoScroll = scroll; SettingsManager::instance()->saveValue("terminal.autoScroll", scroll); emit autoScrollChanged(); }
}

void Terminal::setDataMode(int mode)
{
    if (m_dataMode != mode) { m_dataMode = mode; SettingsManager::instance()->saveValue("terminal.dataMode", mode); emit dataModeChanged(); }
}

void Terminal::setLineEnding(int mode)
{
    if (m_lineEnding != mode) { m_lineEnding = mode; SettingsManager::instance()->saveValue("terminal.lineEnding", mode); emit lineEndingChanged(); }
}

void Terminal::setDisplayMode(int mode)
{
    if (m_displayMode != mode) {
        m_displayMode = mode;
        bool hex = (mode == 1);
        if (m_hexMode != hex) { m_hexMode = hex; emit hexModeChanged(); }
        SettingsManager::instance()->saveValue("terminal.displayMode", mode);
        emit displayModeChanged();
    }
}

// ============================================================================
// 数据接收（带缓冲合并 — OS 分包的数据在 50ms 窗口内合并后一次显示）
// ============================================================================

void Terminal::appendRxData(const QByteArray &data)
{
    if (data.isEmpty()) return;
    m_rxBuffer.append(data);
    m_flushRxTimer->start(); // 重置倒计时，合并后续快速到达的数据
}

void Terminal::appendTxData(const QByteArray &data)
{
    if (data.isEmpty()) return;
    if (!m_echo) return;
    m_txBuffer.append(data);
    m_flushTxTimer->start();
}

void Terminal::flushRxBuffer()
{
    if (m_rxBuffer.isEmpty()) return;
    appendLine(m_rxBuffer);
    m_rxBuffer.clear();
}

void Terminal::flushTxBuffer()
{
    if (m_txBuffer.isEmpty()) return;
    appendLine(m_txBuffer);
    m_txBuffer.clear();
}

void Terminal::appendLine(const QByteArray &data)
{
    QString ts;
    if (m_showTimestamp)
        ts = QDateTime::currentDateTime().toString("hh:mm:ss.zzz") + " ";

    QString content = formatDisplayText(data);

    // 存储到内部 m_lines（带时间戳，供 displayText 属性查询）
    {
        QMutexLocker lock(&m_mutex);
        QStringList contentLines = content.split('\n');
        for (int i = 0; i < contentLines.size(); ++i) {
            QString line;
            if (i == 0)
                line = ts + contentLines[i];
            else
                line = QString(ts.length(), ' ') + contentLines[i];
            m_lines.append(line);
            while (m_lines.size() > MAX_LINES)
                m_lines.removeFirst();
        }
    }

    rebuildDisplayText();
    emit lineCountChanged();
    emit displayTextChanged();

    // Emit formatted string for TerminalWidget.
    // 空行分隔 + 时间戳行 + hex dump 行（不含 RX/TX 标识）
    emit displayString(QStringLiteral("\n") + ts + content);
}

// ============================================================================
// 内部辅助
// ============================================================================

QString Terminal::formatDisplayText(const QByteArray &data) const
{
    if (m_hexMode || m_displayMode == 1) {
        // Hex dump 模式
        constexpr int rowSize = 16;
        QString out;

        for (int i = 0; i < data.size(); i += rowSize) {
            out += QStringLiteral("%1 | ").arg(i, 6, 16, QLatin1Char('0'));
            for (int j = 0; j < rowSize; ++j) {
                if (i + j < data.size())
                    out += QStringLiteral("%1 ").arg(static_cast<unsigned char>(data[i + j]), 2, 16, QLatin1Char('0'));
                else out += QStringLiteral("   ");
                if (j == 7) out += ' ';
            }
            out += QStringLiteral("| ");
            for (int j = 0; j < rowSize; ++j) {
                if (i + j >= data.size()) { out += ' '; continue; }
                const char c = data[i + j];
                out += std::isprint(static_cast<unsigned char>(c)) ? c : '.';
            }
            out += QStringLiteral(" |");
            if (i + rowSize < data.size()) out += '\n';
        }
        return out;
    } else {
        // Plain text: 替换不可打印字符
        QString content = QString::fromUtf8(data);
        for (int i = 0; i < content.size(); ++i) {
            QChar c = content[i];
            if (c.unicode() < 0x20 && c != '\n' && c != '\r' && c != '\t')
                content[i] = QLatin1Char('.');
        }
        return content;
    }
}

QByteArray Terminal::parseSendData(const QString &data) const
{
    if (m_dataMode == DataHEX) {
        QString hex = QString(data).remove(' ').remove('\n').remove('\r');
        if (hex.length() % 2 != 0) hex.prepend('0');
        return QByteArray::fromHex(hex.toLatin1());
    }
    return data.toUtf8();
}

void Terminal::addToHistory(const QString &command)
{
    while (m_historyItems.count() > 100) m_historyItems.removeFirst();
    m_historyItems.append(command);
    m_historyItem = m_historyItems.count();
    emit historyItemChanged();
}

void Terminal::rebuildDisplayText()
{
    QMutexLocker lock(&m_mutex);
    m_displayText = m_lines.join('\n');
}

