#ifndef DATAMONITORMODEL_H
#define DATAMONITORMODEL_H

#include <QAbstractListModel>
#include <QVariantList>
#include <QVariantMap>
#include <QMutex>
#include <QQueue>

/**
 * @brief 协议监控数据模型
 *
 * 采用定时刷新架构 (60fps)，链路数据通过 FrameProcessor 队列缓存，
 * 定时器驱动 UI 消费，避免高频数据导致界面卡顿。
 *
 * 支持过滤规则: filterText 对协议帧所有字段做模糊匹配。
 * 结构化过滤支持 field:value、field=value、* 通配、&&/|| 逻辑组合。
 */
class DataMonitorModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(bool paused READ isPaused WRITE setPaused NOTIFY pausedChanged)
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(int refreshIntervalMs READ refreshIntervalMs WRITE setRefreshIntervalMs NOTIFY refreshIntervalChanged)
    Q_PROPERTY(bool commandEchoEnabled READ commandEchoEnabled WRITE setCommandEchoEnabled NOTIFY commandEchoEnabledChanged)

public:
    enum Roles {
        TimestampRole = Qt::UserRole + 1,
        DirectionRole,
        SenderRole,
        ReceiverRole,
        LenRole,
        TypeRole,
        SeqRole,
        CmdIdRole,
        CmdSetRole,
        DataRole,
        CrcRole,
        Crc8Role,
        RawHexRole,
        RowColorRole,
        TextColorRole
    };
    Q_ENUM(Roles)

    explicit DataMonitorModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE QVariantMap frameAt(int row) const;
    Q_INVOKABLE QVariantList frames(int maxCount = 1000) const;

    bool isPaused() const { return m_paused; }
    void setPaused(bool paused);

    QString filterText() const { return m_filterText; }
    void setFilterText(const QString &text);

    int refreshIntervalMs() const { return m_refreshInterval; }
    void setRefreshIntervalMs(int ms);

    bool commandEchoEnabled() const { return m_commandEchoEnabled; }
    void setCommandEchoEnabled(bool enabled);

public slots:
    void addFrame(const QVariantMap &frame);
    void enqueueFrame(const QVariantMap &frame);  // 外部入队（Lua 过滤后调用）
    void clear();
    void refreshFromQueue();  // 从待显示队列拉取帧并刷新 UI

signals:
    void countChanged();
    void pausedChanged();
    void filterTextChanged();
    void refreshIntervalChanged();
    void commandEchoEnabledChanged();

private:
    bool matchesFilter(const QVariantMap &frame) const;
    void rebuildFilterIndices();
    void appendFramesSorted(const QList<QVariantMap> &frames);

    // 全部帧（未过滤）
    QVariantList m_allFrames;
    // 过滤后的帧索引
    QList<int> m_filteredIndices;

    bool m_paused = false;
    bool m_commandEchoEnabled = false;
    QString m_filterText;
    int m_refreshInterval = 16;  // ~60fps（保留属性，刷新由 TimerEvents 统一驱动）
    qint64 m_nextSequence = 0;

    QMutex m_mutex;
    QQueue<QVariantMap> m_displayQueue;  // 待显示帧队列

    static const int MAX_FRAMES = 20000;
};

#endif // DATAMONITORMODEL_H
