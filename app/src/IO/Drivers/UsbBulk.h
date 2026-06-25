#ifndef USBBULK_H
#define USBBULK_H

#include "IO/HAL_Driver.h"

#include <QTimer>

namespace IO {
namespace Drivers {

class UsbBulk : public HAL_Driver
{
    Q_OBJECT
    Q_PROPERTY(int vendorId READ vendorId WRITE setVendorId NOTIFY configChanged)
    Q_PROPERTY(int productId READ productId WRITE setProductId NOTIFY configChanged)
    Q_PROPERTY(int interfaceNumber READ interfaceNumber WRITE setInterfaceNumber NOTIFY configChanged)
    Q_PROPERTY(int bulkInEndpoint READ bulkInEndpoint WRITE setBulkInEndpoint NOTIFY configChanged)
    Q_PROPERTY(int bulkOutEndpoint READ bulkOutEndpoint WRITE setBulkOutEndpoint NOTIFY configChanged)
    Q_PROPERTY(int readPacketSize READ readPacketSize WRITE setReadPacketSize NOTIFY configChanged)
    Q_PROPERTY(int timeoutMs READ timeoutMs WRITE setTimeoutMs NOTIFY configChanged)
    Q_PROPERTY(int readIntervalMs READ readIntervalMs WRITE setReadIntervalMs NOTIFY configChanged)

public:
    explicit UsbBulk(QObject *parent = nullptr);
    ~UsbBulk() override;

    QString driverName() const override { return QStringLiteral("USB"); }

    bool open() override;
    void close() override;
    bool isOpen() const override;
    bool isReadable() const override { return isOpen(); }
    bool isWritable() const override { return isOpen(); }
    bool configurationOk() const noexcept override;
    qint64 write(const QByteArray &data) override;

    void setProperty(const QString &key, const QVariant &value) override;
    QVariant getProperty(const QString &key) const override;
    QList<IO::DriverProperty> driverProperties() const override;
    QJsonObject deviceIdentifier() const override;
    bool selectByIdentifier(const QJsonObject &id) override;

    int vendorId() const { return m_vendorId; }
    void setVendorId(int value);
    int productId() const { return m_productId; }
    void setProductId(int value);
    int interfaceNumber() const { return m_interfaceNumber; }
    void setInterfaceNumber(int value);
    int bulkInEndpoint() const { return m_bulkInEndpoint; }
    void setBulkInEndpoint(int value);
    int bulkOutEndpoint() const { return m_bulkOutEndpoint; }
    void setBulkOutEndpoint(int value);
    int readPacketSize() const { return m_readPacketSize; }
    void setReadPacketSize(int value);
    int timeoutMs() const { return m_timeoutMs; }
    void setTimeoutMs(int value);
    int readIntervalMs() const { return m_readIntervalMs; }
    void setReadIntervalMs(int value);
    bool suppressOpenErrors() const { return m_suppressOpenErrors; }
    void setSuppressOpenErrors(bool value);

signals:
    void configChanged();

private slots:
    void readPending();

private:
    bool openPlatform();
    void closePlatform();
    qint64 writePlatform(const QByteArray &data);
    void readPlatform();
    QString formatVidPid() const;

    int m_vendorId = 0;
    int m_productId = 0;
    int m_interfaceNumber = 0;
    int m_bulkInEndpoint = 0x81;
    int m_bulkOutEndpoint = 0x01;
    int m_readPacketSize = 512;
    int m_timeoutMs = 20;
    int m_readIntervalMs = 2;
    bool m_suppressOpenErrors = false;
    bool m_open = false;
    QTimer m_readTimer;

    void *m_deviceHandle = nullptr;
    void *m_usbHandle = nullptr;
};

} // namespace Drivers
} // namespace IO

#endif // USBBULK_H
