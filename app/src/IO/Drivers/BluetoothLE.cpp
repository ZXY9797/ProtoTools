#include "IO/Drivers/BluetoothLE.h"

namespace IO {
namespace Drivers {

// ============================================================================
// 共享静态状态
// ============================================================================

bool BluetoothLE::s_initialized = false;
bool BluetoothLE::s_adapterAvailable = true;
QStringList BluetoothLE::s_deviceNames;

#ifdef HAS_QTBLUETOOTH
QBluetoothLocalDevice *BluetoothLE::s_localDevice = nullptr;
QBluetoothDeviceDiscoveryAgent *BluetoothLE::s_discoveryAgent = nullptr;
QList<QBluetoothDeviceInfo> BluetoothLE::s_devices;
QList<BluetoothLE *> BluetoothLE::s_instances;
#endif

void BluetoothLE::initSharedState()
{
    s_initialized = true;
}

#ifdef HAS_QTBLUETOOTH
void BluetoothLE::onDeviceDiscovered(const QBluetoothDeviceInfo &device)
{
    if (!(device.coreConfigurations() & QBluetoothDeviceInfo::LowEnergyCoreConfiguration))
        return;
    if (!device.isValid() || device.name().isEmpty())
        return;
    const QString address = device.address().toString();
    for (const auto &known : std::as_const(s_devices)) {
        if (known.address().toString().compare(address, Qt::CaseInsensitive) == 0)
            return;
    }

    if (s_devices.contains(device))
        return;

    s_devices.append(device);
    s_deviceNames.append(device.name());

    for (auto *inst : std::as_const(s_instances)) {
        if (inst->m_deviceConnected)
            continue;
        Q_EMIT inst->devicesChanged();
    }
}
#endif

// ============================================================================
// 构造/析构
// ============================================================================

BluetoothLE::BluetoothLE(QObject *parent)
    : HAL_Driver(parent)
{
    initSharedState();
#ifdef HAS_QTBLUETOOTH
    s_instances.append(this);
#endif
}

BluetoothLE::~BluetoothLE()
{
    close();
#ifdef HAS_QTBLUETOOTH
    s_instances.removeAll(this);
#endif
}

// ============================================================================
// 连接管理 — 参照 Serial-Studio
// ============================================================================

bool BluetoothLE::open()
{
#ifdef HAS_QTBLUETOOTH
    if (m_deviceIndex < 0 || m_deviceIndex >= s_devices.size()) {
        emit bleError("未选择 BLE 设备");
        return false;
    }

    close();

    auto device  = s_devices.at(m_deviceIndex);
    m_controller = QLowEnergyController::createCentral(device, this);

    // discoveryFinished — 服务列表在此事件后才可用
    connect(m_controller, &QLowEnergyController::discoveryFinished,
            this, &BluetoothLE::onServiceDiscoveryFinished);

    connect(m_controller, &QLowEnergyController::connected,
            this, &BluetoothLE::onControllerConnected);

    connect(m_controller, &QLowEnergyController::disconnected,
            this, &BluetoothLE::onControllerDisconnected);

    connect(m_controller, &QLowEnergyController::errorOccurred,
            this, &BluetoothLE::onControllerError);

    m_controller->connectToDevice();
    return true;
#else
    emit bleError("Qt Bluetooth 模块未编译");
    return false;
#endif
}

void BluetoothLE::close()
{
#ifdef HAS_QTBLUETOOTH
    if (m_service) {
        m_service->deleteLater();
        m_service = nullptr;
    }
    if (m_controller) {
        m_controller->disconnectFromDevice();
        m_controller->deleteLater();
        m_controller = nullptr;
    }
#endif
    m_deviceConnected = false;
    m_serviceNames.clear();
    m_characteristicNames.clear();
    m_characteristics.clear();
    m_selectedCharacteristic = -1;
    m_rxCharacteristic = -1;
    emit characteristicsChanged();
    emit servicesChanged();
    emit txCharacteristicIndexChanged();
    emit rxCharacteristicIndexChanged();
    emit deviceConnectedChanged();
    emit connectionChanged();

    // 传播断开到 UI 驱动
    for (auto *inst : std::as_const(s_instances)) {
        if (inst != this && inst->m_deviceIndex == m_deviceIndex) {
            inst->m_deviceConnected = false;
            inst->m_serviceNames.clear();
            inst->m_characteristicNames.clear();
            inst->m_selectedCharacteristic = -1;
            inst->m_rxCharacteristic = -1;
            Q_EMIT inst->deviceConnectedChanged();
            Q_EMIT inst->servicesChanged();
            Q_EMIT inst->characteristicsChanged();
            Q_EMIT inst->txCharacteristicIndexChanged();
            Q_EMIT inst->rxCharacteristicIndexChanged();
        }
    }
}

bool BluetoothLE::configurationOk() const noexcept
{
#ifdef HAS_QTBLUETOOTH
    return m_deviceIndex >= 0 && m_deviceIndex < s_devices.size();
#else
    return m_deviceIndex >= 0;
#endif
}

bool BluetoothLE::adapterAvailable() const
{
    return s_adapterAvailable;
}

qint64 BluetoothLE::write(const QByteArray &data)
{
#ifdef HAS_QTBLUETOOTH
    // 如果本实例没有 service，转发到有 service 的实例（live driver）
    if (!m_service) {
        for (auto *inst : std::as_const(s_instances)) {
            if (inst != this && inst->m_service
                && inst->m_deviceIndex == m_deviceIndex
                && inst->m_selectedCharacteristic >= 0)
                return inst->write(data);
        }
        return -1;
    }

    if (m_selectedCharacteristic < 0 || m_selectedCharacteristic >= m_characteristics.size())
        return -1;

    const auto &c = m_characteristics.at(m_selectedCharacteristic);
    if (!(c.properties() & (QLowEnergyCharacteristic::Write | QLowEnergyCharacteristic::WriteNoResponse))) {
        emit bleError("BLE TX UUID 不支持写入");
        return -1;
    }

    auto mode = (c.properties() & QLowEnergyCharacteristic::WriteNoResponse)
        ? QLowEnergyService::WriteWithoutResponse
        : QLowEnergyService::WriteWithResponse;
    m_service->writeCharacteristic(c, data, mode);
    notifyDataSent(data);
    return data.size();
#else
    Q_UNUSED(data);
    return -1;
#endif
}

// ============================================================================
// 属性接口
// ============================================================================

void BluetoothLE::setProperty(const QString &key, const QVariant &value)
{
    if (key == "device") {
        QString name = value.toString();
        for (int i = 0; i < s_deviceNames.size(); ++i)
            if (s_deviceNames[i] == name) { selectDevice(i); break; }
    } else if (key == "txCharacteristic") {
        QString name = value.toString();
        for (int i = 0; i < m_characteristicNames.size(); ++i)
            if (m_characteristicNames[i] == name) { setTxCharacteristicIndex(i + 1); break; }
    } else if (key == "rxCharacteristic") {
        QString name = value.toString();
        for (int i = 0; i < m_characteristicNames.size(); ++i)
            if (m_characteristicNames[i] == name) { setRxCharacteristicIndex(i + 1); break; }
    }
}

QVariant BluetoothLE::getProperty(const QString &key) const
{
    if (key == "device") {
        if (m_deviceIndex >= 0 && m_deviceIndex < s_deviceNames.size())
            return s_deviceNames[m_deviceIndex];
        return QString();
    }
    if (key == "txCharacteristic") {
        if (m_selectedCharacteristic >= 0 && m_selectedCharacteristic < m_characteristicNames.size())
            return m_characteristicNames[m_selectedCharacteristic];
        return QString();
    }
    if (key == "rxCharacteristic") {
        if (m_rxCharacteristic >= 0 && m_rxCharacteristic < m_characteristicNames.size())
            return m_characteristicNames[m_rxCharacteristic];
        return QString();
    }
    return {};
}

QList<IO::DriverProperty> BluetoothLE::driverProperties() const
{
    QString devName;
    if (m_deviceIndex >= 0 && m_deviceIndex < s_deviceNames.size())
        devName = s_deviceNames[m_deviceIndex];

    QString txCharName;
    if (m_selectedCharacteristic >= 0 && m_selectedCharacteristic < m_characteristicNames.size())
        txCharName = m_characteristicNames[m_selectedCharacteristic];

    QString rxCharName;
    if (m_rxCharacteristic >= 0 && m_rxCharacteristic < m_characteristicNames.size())
        rxCharName = m_characteristicNames[m_rxCharacteristic];

    return {
        {.key = "device", .label = "BLE 设备", .description = "选择蓝牙设备",
         .type = IO::DriverProperty::ComboBox, .value = devName,
         .options = s_deviceNames},
        {.key = "txCharacteristic", .label = "TX UUID", .description = "选择用于写入的 GATT 特征 UUID",
         .type = IO::DriverProperty::ComboBox, .value = txCharName,
         .options = m_characteristicNames},
        {.key = "rxCharacteristic", .label = "RX UUID", .description = "选择用于通知接收的 GATT 特征 UUID",
         .type = IO::DriverProperty::ComboBox, .value = rxCharName,
         .options = m_characteristicNames},
    };
}

QJsonObject BluetoothLE::deviceIdentifier() const
{
    QJsonObject id;
#ifdef HAS_QTBLUETOOTH
    if (m_deviceIndex >= 0 && m_deviceIndex < s_devices.size()) {
        id["address"] = s_devices[m_deviceIndex].address().toString();
        id["name"] = s_devices[m_deviceIndex].name();
    }
#endif
    return id;
}

bool BluetoothLE::selectByIdentifier(const QJsonObject &id)
{
#ifdef HAS_QTBLUETOOTH
    QString addr = id.value("address").toString();
    QString name = id.value("name").toString();
    for (int i = 0; i < s_devices.size(); ++i) {
        if (s_devices[i].address().toString() == addr ||
            (!name.isEmpty() && s_devices[i].name() == name)) {
            selectDevice(i);
            return true;
        }
    }
#else
    Q_UNUSED(id);
#endif
    return false;
}

// ============================================================================
// 访问器
// ============================================================================

int BluetoothLE::deviceCount() const
{
#ifdef HAS_QTBLUETOOTH
    return s_devices.size();
#else
    return 0;
#endif
}

QStringList BluetoothLE::deviceNames() const { return s_deviceNames; }

QStringList BluetoothLE::deviceAddresses() const
{
    QStringList addresses;
#ifdef HAS_QTBLUETOOTH
    addresses.reserve(s_devices.size());
    for (const auto &device : s_devices)
        addresses.append(device.address().toString());
#endif
    return addresses;
}

QStringList BluetoothLE::serviceNames() const
{
    // 参照 Serial-Studio: index 0 为占位符 "选择服务..."，index 1+ 为实际服务 UUID
    QStringList list;
    list.append(QStringLiteral("选择服务..."));
    list.append(m_serviceNames);
    return list;
}

QStringList BluetoothLE::characteristicNames() const
{
    // index 0 为占位符 "选择特征值..."，index 1+ 为实际特征名
    QStringList list;
    list.append(QStringLiteral("选择特征值..."));
    list.append(m_characteristicNames);
    return list;
}

bool BluetoothLE::scanning() const
{
#ifdef HAS_QTBLUETOOTH
    return s_discoveryAgent && s_discoveryAgent->isActive();
#else
    return false;
#endif
}

int BluetoothLE::txCharacteristicIndex() const
{
    // 1-based: 0 = 占位符 "选择特征值..."，1+ = 实际特征
    if (m_selectedCharacteristic >= 0)
        return m_selectedCharacteristic + 1;

    // 回退到 live driver 的选择（参照 Serial-Studio）
    for (auto *inst : std::as_const(s_instances))
        if (inst != this && inst->m_deviceIndex == m_deviceIndex && inst->m_selectedCharacteristic >= 0)
            return inst->m_selectedCharacteristic + 1;

    return 0;
}

int BluetoothLE::rxCharacteristicIndex() const
{
    // 1-based: 0 = 占位符 "选择特征值..."，1+ = 实际特征
    if (m_rxCharacteristic >= 0)
        return m_rxCharacteristic + 1;

    // 回退到 live driver 的选择
    for (auto *inst : std::as_const(s_instances))
        if (inst != this && inst->m_deviceIndex == m_deviceIndex && inst->m_rxCharacteristic >= 0)
            return inst->m_rxCharacteristic + 1;

    return 0;
}

// ============================================================================
// 扫描 — 参照 Serial-Studio startDiscovery
// ============================================================================

void BluetoothLE::startScan()
{
#ifdef HAS_QTBLUETOOTH
    // 延迟初始化适配器
    if (!s_localDevice) {
        s_localDevice = new QBluetoothLocalDevice();
        auto hostMode = s_localDevice->hostMode();
        s_adapterAvailable = (hostMode != QBluetoothLocalDevice::HostPoweredOff);
        QObject::connect(s_localDevice, &QBluetoothLocalDevice::hostModeStateChanged,
                         s_localDevice, [](QBluetoothLocalDevice::HostMode mode) {
            s_adapterAvailable = (mode != QBluetoothLocalDevice::HostPoweredOff);
            for (auto *inst : std::as_const(s_instances))
                Q_EMIT inst->adapterAvailabilityChanged();
        });
    }

    if (!s_adapterAvailable) {
        emit bleError("蓝牙适配器不可用");
        return;
    }

    if (s_discoveryAgent && s_discoveryAgent->isActive())
        return;

    if (s_discoveryAgent) {
        QObject::disconnect(s_discoveryAgent, nullptr, nullptr, nullptr);
        s_discoveryAgent->deleteLater();
        s_discoveryAgent = nullptr;
    }

    s_devices.clear();
    s_deviceNames.clear();

    s_discoveryAgent = new QBluetoothDeviceDiscoveryAgent();
    s_discoveryAgent->setLowEnergyDiscoveryTimeout(10000);

    QObject::connect(s_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
                     s_discoveryAgent, [](const QBluetoothDeviceInfo &info) {
        onDeviceDiscovered(info);
        for (auto *inst : std::as_const(s_instances))
            Q_EMIT inst->devicesChanged();
    });

    QObject::connect(s_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::finished,
                     s_discoveryAgent, []() {
        for (auto *inst : std::as_const(s_instances))
            Q_EMIT inst->scanningChanged();
    });

    QObject::connect(s_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::errorOccurred,
                     s_discoveryAgent, [](QBluetoothDeviceDiscoveryAgent::Error error) {
        Q_UNUSED(error);
        for (auto *inst : std::as_const(s_instances))
            Q_EMIT inst->scanningChanged();
    });

    s_discoveryAgent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
    for (auto *inst : std::as_const(s_instances))
        Q_EMIT inst->scanningChanged();
#endif
}

void BluetoothLE::stopScan()
{
#ifdef HAS_QTBLUETOOTH
    if (s_discoveryAgent && s_discoveryAgent->isActive()) {
        s_discoveryAgent->stop();
        for (auto *inst : std::as_const(s_instances))
            Q_EMIT inst->scanningChanged();
    }
#endif
}

void BluetoothLE::selectDevice(int index)
{
    if (m_deviceIndex == index) return;
    m_deviceIndex = index;
    emit deviceIndexChanged();
    emit configurationChanged();
}

// ============================================================================
// 服务/特征选择 — 参照 Serial-Studio
// ============================================================================

void BluetoothLE::selectService(int index)
{
#ifdef HAS_QTBLUETOOTH
    // 如果本实例没有 controller，转发到有 controller 的实例
    if (!m_controller) {
        for (auto *inst : std::as_const(s_instances)) {
            if (inst != this && inst->m_controller
                && inst->m_deviceIndex == m_deviceIndex) {
                inst->selectService(index);
                return;
            }
        }
        m_pendingServiceIndex = index;
        return;
    }

    if (m_service) {
        m_service->deleteLater();
        m_service = nullptr;
    }

    m_characteristics.clear();
    m_characteristicNames.clear();
    m_selectedCharacteristic = -1;
    m_rxCharacteristic = -1;
    emit characteristicsChanged();
    emit txCharacteristicIndexChanged();
    emit rxCharacteristicIndexChanged();

    // index 为 1-based（QML convention）
    if (index >= 1 && index <= m_serviceNames.count()) {
        if (index - 1 >= m_controller->services().count())
            return;

        auto serviceUuid = m_controller->services().at(index - 1);
        m_service = m_controller->createServiceObject(serviceUuid, this);
        if (m_service) {
            connect(m_service, &QLowEnergyService::characteristicChanged,
                    this, &BluetoothLE::onCharacteristicChanged);
            connect(m_service, &QLowEnergyService::stateChanged,
                    this, &BluetoothLE::onServiceDiscovered);
            connect(m_service, &QLowEnergyService::errorOccurred,
                    this, &BluetoothLE::onServiceError);

            if (m_service->state() == QLowEnergyService::RemoteService)
                m_service->discoverDetails();
            else
                configureCharacteristics();
        }
    }
#else
    Q_UNUSED(index);
#endif
}

void BluetoothLE::setTxCharacteristicIndex(int index)
{
#ifdef HAS_QTBLUETOOTH
    // 如果本实例没有 service，转发到有 service 的实例
    if (!m_service) {
        for (auto *inst : std::as_const(s_instances)) {
            if (inst != this && inst->m_service
                && inst->m_deviceIndex == m_deviceIndex) {
                inst->setTxCharacteristicIndex(index);
                return;
            }
        }
        m_pendingTxCharacteristicIndex = index;
        const int newIdx = index - 1;
        if (m_selectedCharacteristic != newIdx) {
            m_selectedCharacteristic = newIdx;
            emit txCharacteristicIndexChanged();
            emit configurationChanged();
        }
        return;
    }

    int newIdx = index - 1;  // 1-based → 0-based; index=0 → newIdx=-1 (deselect)
    if (m_selectedCharacteristic == newIdx) return;

    m_selectedCharacteristic = newIdx;

    emit txCharacteristicIndexChanged();
    emit configurationChanged();

    // 传播到其他实例
    for (auto *inst : std::as_const(s_instances)) {
        if (inst != this && inst->m_deviceIndex == m_deviceIndex) {
            inst->m_selectedCharacteristic = m_selectedCharacteristic;
            Q_EMIT inst->txCharacteristicIndexChanged();
        }
    }
#else
    Q_UNUSED(index);
#endif
}

void BluetoothLE::setRxCharacteristicIndex(int index)
{
#ifdef HAS_QTBLUETOOTH
    // 如果本实例没有 service，转发到有 service 的实例
    if (!m_service) {
        for (auto *inst : std::as_const(s_instances)) {
            if (inst != this && inst->m_service
                && inst->m_deviceIndex == m_deviceIndex) {
                inst->setRxCharacteristicIndex(index);
                return;
            }
        }
        m_pendingRxCharacteristicIndex = index;
        const int newIdx = index - 1;
        if (m_rxCharacteristic != newIdx) {
            m_rxCharacteristic = newIdx;
            emit rxCharacteristicIndexChanged();
            emit configurationChanged();
        }
        return;
    }

    const int newIdx = index - 1;  // 1-based → 0-based; index=0 → newIdx=-1 (deselect)
    if (m_rxCharacteristic == newIdx)
        return;

    const int oldIdx = m_rxCharacteristic;
    m_rxCharacteristic = newIdx;
    configureRxNotifications(oldIdx, m_rxCharacteristic);

    emit rxCharacteristicIndexChanged();
    emit configurationChanged();

    // 传播到其他实例
    for (auto *inst : std::as_const(s_instances)) {
        if (inst != this && inst->m_deviceIndex == m_deviceIndex) {
            inst->m_rxCharacteristic = m_rxCharacteristic;
            Q_EMIT inst->rxCharacteristicIndexChanged();
        }
    }
#else
    Q_UNUSED(index);
#endif
}

// ============================================================================
// configureCharacteristics — 参照 Serial-Studio
// ============================================================================

void BluetoothLE::configureCharacteristics()
{
#ifdef HAS_QTBLUETOOTH
    if (!m_service) return;

    m_characteristics.clear();
    m_characteristicNames.clear();
    m_selectedCharacteristic = -1;
    m_rxCharacteristic = -1;

    const auto chars = m_service->characteristics();
    for (const auto &c : chars) {
        if (!c.isValid()) continue;
        m_characteristics.append(c);
        m_characteristicNames.append(c.uuid().toString(QUuid::WithoutBraces).toUpper());
    }

    emit characteristicsChanged();
    emit txCharacteristicIndexChanged();
    emit rxCharacteristicIndexChanged();

    const int pendingTx = m_pendingTxCharacteristicIndex;
    const int pendingRx = m_pendingRxCharacteristicIndex;
    m_pendingTxCharacteristicIndex = -1;
    m_pendingRxCharacteristicIndex = -1;

    if (pendingTx > 0)
        setTxCharacteristicIndex(pendingTx);
    if (pendingRx > 0)
        setRxCharacteristicIndex(pendingRx);

    propagateCharacteristicSelection();
#endif
}

void BluetoothLE::configureRxNotifications(int oldIndex, int newIndex)
{
#ifdef HAS_QTBLUETOOTH
    if (!m_service)
        return;

    auto writeCccd = [this](int index, const QByteArray &value) {
        if (index < 0 || index >= m_characteristics.size())
            return;
        const auto &c = m_characteristics.at(index);
        const auto &cccd = c.clientCharacteristicConfiguration();
        if (cccd.isValid())
            m_service->writeDescriptor(cccd, value);
    };

    if (oldIndex >= 0 && oldIndex != newIndex)
        writeCccd(oldIndex, QLowEnergyCharacteristic::CCCDDisable);

    if (newIndex < 0 || newIndex >= m_characteristics.size())
        return;

    const auto &c = m_characteristics.at(newIndex);
    const auto properties = c.properties();
    if (properties & QLowEnergyCharacteristic::Notify) {
        writeCccd(newIndex, QLowEnergyCharacteristic::CCCDEnableNotification);
    } else if (properties & QLowEnergyCharacteristic::Indicate) {
        writeCccd(newIndex, QLowEnergyCharacteristic::CCCDEnableIndication);
    } else {
        emit bleError("BLE RX UUID 不支持通知/指示");
    }
#else
    Q_UNUSED(oldIndex);
    Q_UNUSED(newIndex);
#endif
}

void BluetoothLE::propagateCharacteristicSelection()
{
#ifdef HAS_QTBLUETOOTH
    for (auto *inst : std::as_const(s_instances)) {
        if (inst != this && inst->m_deviceIndex == m_deviceIndex) {
            inst->m_characteristicNames = m_characteristicNames;
            inst->m_selectedCharacteristic = m_selectedCharacteristic;
            inst->m_rxCharacteristic = m_rxCharacteristic;
            Q_EMIT inst->characteristicsChanged();
            Q_EMIT inst->txCharacteristicIndexChanged();
            Q_EMIT inst->rxCharacteristicIndexChanged();
        }
    }
#endif
}

// ============================================================================
// 私有槽 — 参照 Serial-Studio
// ============================================================================

#ifdef HAS_QTBLUETOOTH

void BluetoothLE::onControllerConnected()
{
    m_deviceConnected = true;
    emit deviceConnectedChanged();
    emit connectionChanged();

    // 启动服务发现
    m_controller->discoverServices();

    // 传播连接状态
    for (auto *inst : std::as_const(s_instances)) {
        if (inst != this && inst->m_deviceIndex == m_deviceIndex) {
            inst->m_deviceConnected = true;
            Q_EMIT inst->deviceConnectedChanged();
        }
    }
}

void BluetoothLE::onControllerDisconnected()
{
    close();
}

void BluetoothLE::onControllerError(QLowEnergyController::Error error)
{
    Q_UNUSED(error);
    if (m_controller)
        emit bleError("BLE 控制器错误: " + m_controller->errorString());
}

void BluetoothLE::onServiceDiscoveryFinished()
{
    if (!m_controller) return;

    m_serviceNames.clear();
    const auto services = m_controller->services();
    for (const auto &service : services)
        m_serviceNames.append(service.toString());

    emit servicesChanged();

    // 传播到 UI 驱动
    for (auto *inst : std::as_const(s_instances)) {
        if (inst != this && inst->m_deviceIndex == m_deviceIndex) {
            inst->m_serviceNames = m_serviceNames;
            Q_EMIT inst->servicesChanged();
        }
    }

    // 待同步服务选择
    if (m_pendingServiceIndex > 0) {
        selectService(m_pendingServiceIndex);
        m_pendingServiceIndex = -1;
    }
}

void BluetoothLE::onServiceError(QLowEnergyService::ServiceError error)
{
    Q_UNUSED(error);
    emit bleError("BLE 服务错误");
}

void BluetoothLE::onServiceDiscovered(QLowEnergyService::ServiceState state)
{
    if (state != QLowEnergyService::RemoteServiceDiscovered || !m_service) return;
    configureCharacteristics();
}

void BluetoothLE::onCharacteristicChanged(const QLowEnergyCharacteristic &c, const QByteArray &value)
{
    if (m_rxCharacteristic < 0 || m_rxCharacteristic >= m_characteristics.size())
        return;
    if (c != m_characteristics.at(m_rxCharacteristic))
        return;
    if (!value.isEmpty())
        processReceivedData(value);
}

#endif

} // namespace Drivers
} // namespace IO
