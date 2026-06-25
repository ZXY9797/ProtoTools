#ifndef BLUETOOTHLE_H
#define BLUETOOTHLE_H

#include "IO/HAL_Driver.h"

#ifdef HAS_QTBLUETOOTH
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QBluetoothLocalDevice>
#include <QLowEnergyCharacteristic>
#include <QLowEnergyController>
#include <QLowEnergyService>
#endif

namespace IO {
namespace Drivers {

/**
 * @brief BLE 驱动 — 参照 Serial-Studio BluetoothLE 完整实现
 *
 * 连接流程:
 *   open() → createCentral → connectToDevice()
 *   connected → discoverServices() ← 关键!
 *   discoveryFinished → onServiceDiscoveryFinished() → 读取 services
 *   selectService() → createServiceObject → discoverDetails()
 *   service state(RemoteServiceDiscovered) → onServiceDiscovered() → 读取 characteristics
 */
class BluetoothLE : public HAL_Driver
{
    Q_OBJECT
    Q_PROPERTY(int deviceCount READ deviceCount NOTIFY devicesChanged)
    Q_PROPERTY(QStringList deviceNames READ deviceNames NOTIFY devicesChanged)
    Q_PROPERTY(QStringList deviceAddresses READ deviceAddresses NOTIFY devicesChanged)
    Q_PROPERTY(int deviceIndex READ deviceIndex WRITE selectDevice NOTIFY deviceIndexChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(QStringList serviceNames READ serviceNames NOTIFY servicesChanged)
    Q_PROPERTY(QStringList characteristicNames READ characteristicNames NOTIFY characteristicsChanged)
    Q_PROPERTY(bool deviceConnected READ deviceConnected NOTIFY deviceConnectedChanged)
    Q_PROPERTY(bool adapterAvailable READ adapterAvailable NOTIFY adapterAvailabilityChanged)
    Q_PROPERTY(int txCharacteristicIndex READ txCharacteristicIndex WRITE setTxCharacteristicIndex NOTIFY txCharacteristicIndexChanged)
    Q_PROPERTY(int rxCharacteristicIndex READ rxCharacteristicIndex WRITE setRxCharacteristicIndex NOTIFY rxCharacteristicIndexChanged)

signals:
    void devicesChanged();
    void deviceIndexChanged();
    void scanningChanged();
    void servicesChanged();
    void characteristicsChanged();
    void txCharacteristicIndexChanged();
    void rxCharacteristicIndexChanged();
    void deviceConnectedChanged();
    void adapterAvailabilityChanged();
    void bleError(const QString &message);

public:
    explicit BluetoothLE(QObject *parent = nullptr);
    ~BluetoothLE() override;

    QString driverName() const override { return QStringLiteral("BLE"); }

    bool open() override;
    void close() override;
    bool isOpen() const noexcept override { return m_deviceConnected; }
    bool isReadable() const noexcept override { return m_deviceConnected; }
    bool isWritable() const noexcept override { return m_deviceConnected; }
    bool configurationOk() const noexcept override;
    qint64 write(const QByteArray &data) override;

    void setProperty(const QString &key, const QVariant &value) override;
    QVariant getProperty(const QString &key) const override;
    QList<IO::DriverProperty> driverProperties() const override;

    QJsonObject deviceIdentifier() const override;
    bool selectByIdentifier(const QJsonObject &id) override;

    int deviceCount() const;
    int deviceIndex() const { return m_deviceIndex; }
    QStringList deviceNames() const;
    QStringList deviceAddresses() const;
    bool scanning() const;
    bool adapterAvailable() const;
    QStringList serviceNames() const;
    QStringList characteristicNames() const;
    bool deviceConnected() const { return m_deviceConnected; }
    int txCharacteristicIndex() const;
    int rxCharacteristicIndex() const;

public slots:
    void startScan();
    void stopScan();
    void selectDevice(int index);
    void selectService(int index);
    void setTxCharacteristicIndex(int index);
    void setRxCharacteristicIndex(int index);

#ifdef HAS_QTBLUETOOTH
private slots:
    void onControllerConnected();
    void onControllerDisconnected();
    void onServiceDiscoveryFinished();
    void onServiceDiscovered(QLowEnergyService::ServiceState state);
    void onCharacteristicChanged(const QLowEnergyCharacteristic &c, const QByteArray &value);
    void onControllerError(QLowEnergyController::Error error);
    void onServiceError(QLowEnergyService::ServiceError error);
#endif

private:
    static void initSharedState();
    void configureCharacteristics();
    void configureRxNotifications(int oldIndex, int newIndex);
    void propagateCharacteristicSelection();

#ifdef HAS_QTBLUETOOTH
    static void onDeviceDiscovered(const QBluetoothDeviceInfo &device);
#endif

    int m_deviceIndex = -1;
    bool m_deviceConnected = false;
    int m_selectedCharacteristic = -1;  // TX: 0-based, -1 = 未选择
    int m_rxCharacteristic = -1;        // RX: 0-based, -1 = 未选择

    // 待同步标记
    int m_pendingServiceIndex = -1;
    int m_pendingTxCharacteristicIndex = -1;
    int m_pendingRxCharacteristicIndex = -1;
    QJsonObject m_pendingIdentifier;

#ifdef HAS_QTBLUETOOTH
    QLowEnergyController *m_controller = nullptr;
    QLowEnergyService *m_service = nullptr;
    QList<QLowEnergyCharacteristic> m_characteristics;
#endif

    QStringList m_serviceNames;
    QStringList m_characteristicNames;

    // ===== 共享静态状态 =====
    static bool s_initialized;
    static bool s_adapterAvailable;
    static QStringList s_deviceNames;
#ifdef HAS_QTBLUETOOTH
    static QBluetoothLocalDevice *s_localDevice;
    static QBluetoothDeviceDiscoveryAgent *s_discoveryAgent;
    static QList<QBluetoothDeviceInfo> s_devices;
    static QList<BluetoothLE *> s_instances;
#endif
};

} // namespace Drivers
} // namespace IO

#endif // BLUETOOTHLE_H
