#pragma once

#include <QObject>
#include <QStringList>
#include <QMutex>
#include <QByteArray>
#include <QTimer>

/**
 * @brief 终端 ConsoleHandler — 参照 SerialStudio Console::Handler
 *
 * 管理数据收发、显示格式、命令历史。
 * 通过 Q_PROPERTY 直接暴露给 QML，发送 displayString 信号驱动 TerminalWidget 渲染。
 */
class Terminal : public QObject
{
    Q_OBJECT

    // ── 显示属性 ──
    Q_PROPERTY(int lineCount READ lineCount NOTIFY lineCountChanged)
    Q_PROPERTY(bool hexMode READ hexMode WRITE setHexMode NOTIFY hexModeChanged)
    Q_PROPERTY(bool showTimestamp READ showTimestamp WRITE setShowTimestamp NOTIFY showTimestampChanged)
    Q_PROPERTY(bool autoScroll READ autoScroll WRITE setAutoScroll NOTIFY autoScrollChanged)
    Q_PROPERTY(bool echo READ echo WRITE setEcho NOTIFY echoChanged)
    Q_PROPERTY(QString displayText READ displayText NOTIFY displayTextChanged)

    // ── 发送属性 (参照 SerialStudio ConsoleHandler) ──
    Q_PROPERTY(int dataMode READ dataMode WRITE setDataMode NOTIFY dataModeChanged)
    Q_PROPERTY(int lineEnding READ lineEnding WRITE setLineEnding NOTIFY lineEndingChanged)
    Q_PROPERTY(int displayMode READ displayMode WRITE setDisplayMode NOTIFY displayModeChanged)
    Q_PROPERTY(QStringList lineEndings READ lineEndings CONSTANT)
    Q_PROPERTY(QStringList dataModes READ dataModes CONSTANT)
    Q_PROPERTY(QStringList displayModes READ displayModes CONSTANT)
    Q_PROPERTY(QString currentHistoryString READ currentHistoryString NOTIFY historyItemChanged)

public:
    enum DataMode { DataASCII = 0, DataHEX = 1 };
    Q_ENUM(DataMode)

    enum class LineEnding {
        NoLineEnding = 0,
        NewLine      = 1,
        CarriageReturn = 2,
        Both         = 3
    };
    Q_ENUM(LineEnding)

    explicit Terminal(QObject *parent = nullptr);

    // ── 属性访问 ──
    int lineCount() const;
    bool hexMode() const;
    bool showTimestamp() const;
    bool autoScroll() const;
    bool echo() const;
    QString displayText() const;
    int dataMode() const;
    int lineEnding() const;
    int displayMode() const;
    QStringList lineEndings() const;
    QStringList dataModes() const;
    QStringList displayModes() const;
    QString currentHistoryString() const;

    Q_INVOKABLE QString getLine(int index) const;

    // ── 操作 ──
    Q_INVOKABLE void clear();
    Q_INVOKABLE void send(const QString &data);
    Q_INVOKABLE void historyUp();
    Q_INVOKABLE void historyDown();

    // ── HEX 辅助 ──
    Q_INVOKABLE bool validateUserHex(const QString &text);
    Q_INVOKABLE QString formatUserHex(const QString &text);

public slots:
    void setHexMode(bool hex);
    void setEcho(bool enabled);
    void setShowTimestamp(bool show);
    void setAutoScroll(bool scroll);
    void setDataMode(int mode);
    void setLineEnding(int mode);
    void setDisplayMode(int mode);

    void appendRxData(const QByteArray &data);
    void appendTxData(const QByteArray &data);

signals:
    void lineCountChanged();
    void hexModeChanged();
    void showTimestampChanged();
    void autoScrollChanged();
    void echoChanged();
    void dataModeChanged();
    void lineEndingChanged();
    void displayModeChanged();
    void historyItemChanged();
    void displayTextChanged();
    void dataSendRequested(const QByteArray &data);

    /// 格式化后的显示字符串，供 QML 转发到 TerminalWidget::append()
    void displayString(const QString &text);

private:
    void appendLine(const QByteArray &data);
    void flushRxBuffer();
    void flushTxBuffer();
    QString formatDisplayText(const QByteArray &data) const;
    QByteArray parseSendData(const QString &data) const;
    void addToHistory(const QString &command);
    void rebuildDisplayText();
    QString configFilePath() const;

    // 缓冲合并：串口数据可能被 OS 分包到达，缓冲 50ms 内到达的所有数据再统一显示
    QByteArray m_rxBuffer;
    QByteArray m_txBuffer;
    QTimer *m_flushRxTimer = nullptr;
    QTimer *m_flushTxTimer = nullptr;
    static constexpr int FLUSH_INTERVAL_MS = 50;

    mutable QMutex m_mutex;
    QStringList m_lines;
    QString m_displayText;

    bool m_hexMode = false;
    bool m_showTimestamp = false;
    bool m_autoScroll = true;
    bool m_echo = true;

    int m_dataMode = DataASCII;
    int m_lineEnding = static_cast<int>(LineEnding::NewLine);
    int m_displayMode = 0; // 0=Plain Text, 1=Hex Dump

    int m_historyItem = 0;
    QStringList m_historyItems;

    static const int MAX_LINES = 10000;
};
