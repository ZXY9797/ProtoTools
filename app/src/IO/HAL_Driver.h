#ifndef HAL_DRIVER_H
#define HAL_DRIVER_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QVariant>
#include <QList>
#include <QJsonObject>

namespace IO {

/**
 * @brief 驱动配置属性描述符
 *
 * 每个驱动通过 driverProperties() 返回此结构的列表，
 * QML Setup 面板据此动态生成配置表单，无需知道具体总线类型。
 */
struct DriverProperty {
    enum Type {
        Text,        // 普通文本
        HexText,     // 十六进制文本
        IntField,    // 整数
        FloatField,  // 浮点数
        CheckBox,    // 复选框
        ComboBox,    // 下拉框
    };

    QString key;         // 属性键（setProperty/getProperty 使用）
    QString label;       // 显示标签
    QString description; // 提示文字
    Type type = Text;
    QVariant value;
    QStringList options;  // ComboBox 可用选项
    QVariant min;         // IntField/FloatField 最小值
    QVariant max;         // IntField/FloatField 最大值
};

/**
 * @brief 链路驱动抽象基类
 *
 * 所有链路类型（UART、USB、CAN、BLE）都继承此基类。
 * ConnectionManager 通过统一接口管理不同驱动。
 *
 * 设计借鉴 Serial-Studio 的 HAL_Driver：
 * - DriverProperty 模式：驱动自描述配置，UI 自动生成表单
 * - UI-config / live driver 分离：配置用 UI 驱动，连接用 live 驱动
 * - processData() 缓冲：高频数据聚合后 emit，减少信号开销
 */
class HAL_Driver : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isOpen READ isOpen NOTIFY connectionChanged)
    Q_PROPERTY(QString driverName READ driverName CONSTANT)

public:
    explicit HAL_Driver(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~HAL_Driver() = default;

    /// 驱动名称（用于 UI 显示）
    virtual QString driverName() const = 0;

    // 连接管理
    virtual bool open() = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    /// 设备是否可读
    virtual bool isReadable() const { return isOpen(); }

    /// 设备是否可写
    virtual bool isWritable() const { return isOpen(); }

    /// 配置是否合法（可安全调用 open()）
    virtual bool configurationOk() const noexcept { return true; }

    // 数据收发
    virtual qint64 write(const QByteArray &data) = 0;

    // 通用属性接口（驱动配置）
    virtual void setProperty(const QString &key, const QVariant &value) = 0;
    virtual QVariant getProperty(const QString &key) const = 0;

    /// 返回驱动配置属性列表（UI 据此生成配置表单）
    virtual QList<DriverProperty> driverProperties() const = 0;

    /// 返回设备硬件标识（用于跨平台匹配，JSON 格式）
    virtual QJsonObject deviceIdentifier() const { return {}; }

    /// 根据之前保存的硬件标识查找并选中设备
    virtual bool selectByIdentifier(const QJsonObject &id) {
        Q_UNUSED(id);
        return false;
    }

    // ========== 缓冲数据管线 ==========

    /**
     * @brief 设置缓冲区阈值（字节）
     *
     * 收到数据先入缓冲区，达到阈值后 emit dataReceived()。
     * 设为 1 表示逐字节发送（默认），适合低吞吐设备。
     * 设为更大值可减少高频数据时的信号开销。
     */
    void setBufferSize(int bytes) { m_bufferSize = qMax(1, bytes); }

    /// 强制刷新缓冲区（立即 emit 当前缓冲数据）
    Q_INVOKABLE void flushBuffer() {
        if (!m_buffer.isEmpty()) {
            emit dataReceived(m_buffer);
            m_buffer.clear();
        }
    }

signals:
    void connectionChanged();
    void configurationChanged();
    void dataReceived(const QByteArray &data);
    void dataSent(const QByteArray &data);
    void errorOccurred(const QString &message);

protected:
    /**
     * @brief 子类通过此方法喂入接收到的数据
     *
     * 内部累积到缓冲区，达到阈值后自动 emit dataReceived()。
     * 如果缓冲区阈值为 1，直接 emit（无缓冲延迟）。
     */
    void processReceivedData(const QByteArray &data) {
        if (m_bufferSize <= 1) {
            emit dataReceived(data);
            return;
        }
        m_buffer.append(data);
        if (m_buffer.size() >= m_bufferSize) {
            emit dataReceived(m_buffer);
            m_buffer.clear();
        }
    }

    /// 通知数据已发送
    void notifyDataSent(const QByteArray &data) { emit dataSent(data); }

private:
    QByteArray m_buffer;
    int m_bufferSize = 1;
};

} // namespace IO

#endif // HAL_DRIVER_H
