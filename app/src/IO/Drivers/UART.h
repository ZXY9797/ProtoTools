#ifndef UART_H
#define UART_H

#include "IO/HAL_Driver.h"

#ifdef HAS_SERIALPORT
#include <QSerialPort>
#include <QSerialPortInfo>
#endif

namespace IO {
namespace Drivers {

/**
 * @brief 串口驱动
 *
 * 实现 HAL_Driver 接口，支持设备枚举、连接管理、DriverProperty 配置描述。
 * 配置变化 emit configurationChanged()，供 ConnectionManager 和 QML 面板响应。
 */
class UART : public HAL_Driver
{
    Q_OBJECT
    Q_PROPERTY(QString portName READ portName WRITE setPortName NOTIFY configChanged)
    Q_PROPERTY(int baudRate READ baudRate WRITE setBaudRate NOTIFY configChanged)
    Q_PROPERTY(int dataBits READ dataBits WRITE setDataBits NOTIFY configChanged)
    Q_PROPERTY(int stopBits READ stopBits WRITE setStopBits NOTIFY configChanged)
    Q_PROPERTY(QString parity READ parity WRITE setParity NOTIFY configChanged)
    Q_PROPERTY(QStringList availablePorts READ availablePorts NOTIFY portsChanged)

public:
    explicit UART(QObject *parent = nullptr);
    ~UART() override;

    QString driverName() const override { return QStringLiteral("UART"); }

    bool open() override;
    void close() override;
    bool isOpen() const override;
    bool isReadable() const override { return isOpen(); }
    bool isWritable() const override { return isOpen(); }
    bool configurationOk() const noexcept override;
    qint64 write(const QByteArray &data) override;

    void setProperty(const QString &key, const QVariant &value) override;
    QVariant getProperty(const QString &key) const override;

    /// 返回驱动配置属性列表（供 Setup 面板生成表单）
    QList<IO::DriverProperty> driverProperties() const override;

    /// 返回设备硬件标识（端口名 + 系统路径）
    QJsonObject deviceIdentifier() const override;
    bool selectByIdentifier(const QJsonObject &id) override;

    QString portName() const { return m_portName; }
    void setPortName(const QString &name);
    int baudRate() const { return m_baudRate; }
    void setBaudRate(int rate);
    int dataBits() const { return m_dataBits; }
    void setDataBits(int bits);
    int stopBits() const { return m_stopBits; }
    void setStopBits(int bits);
    QString parity() const { return m_parity; }
    void setParity(const QString &p);
    QStringList availablePorts() const;

signals:
    void configChanged();
    void portsChanged();

private slots:
    void onReadyRead();

private:
    void applyConfig();

#ifdef HAS_SERIALPORT
    QSerialPort *m_port = nullptr;
#endif
    QString m_portName;
    int m_baudRate = 115200;
    int m_dataBits = 8;
    int m_stopBits = 1;
    QString m_parity = "N";
};

} // namespace Drivers
} // namespace IO

#endif // UART_H
