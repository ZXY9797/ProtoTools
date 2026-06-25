#include "IO/ConnectionManager.h"

#include "DataModel/SettingsManager.h"
#include "IO/Drivers/UART.h"
#include "IO/Drivers/UsbBulk.h"
#include "IO/Drivers/ZlgCanFd.h"
#include "IO/Drivers/BluetoothLE.h"

namespace {

int parseIntegerSetting(const QVariant &value, int fallback)
{
    bool ok = false;
    const int parsed = value.toString().toInt(&ok, 0);
    if (ok)
        return parsed;
    const int decimal = value.toInt(&ok);
    return ok ? decimal : fallback;
}

QString normalizeLinkType(const QString &type)
{
    const QString trimmed = type.trimmed();
    if (trimmed == QStringLiteral("串口") || trimmed.compare(QStringLiteral("UART"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("UART");
    return trimmed;
}

int defaultCanPayloadSize(int frameFormat)
{
    return frameFormat == 0 ? 8 : 64;
}

} // namespace

ConnectionManager::ConnectionManager(QObject *parent)
    : QObject(parent)
    , m_uartUi(std::make_unique<IO::Drivers::UART>())
    , m_usbUi(std::make_unique<IO::Drivers::UsbBulk>())
    , m_canUi(std::make_unique<IO::Drivers::ZlgCanFd>())
    , m_bluetoothLEUi(std::make_unique<IO::Drivers::BluetoothLE>())
{
    // 链路速率计算
    connect(&m_speedTimer, &QTimer::timeout, this, [this]() {
        m_rxSpeed = m_rxBytes - m_rxBytesLast;
        m_txSpeed = m_txBytes - m_txBytesLast;
        if (m_rxSpeed < 0) m_rxSpeed = 0;
        if (m_txSpeed < 0) m_txSpeed = 0;
        m_rxBytesLast = m_rxBytes;
        m_txBytesLast = m_txBytes;
        emit statsChanged();
    });
    m_speedTimer.start(1000);

    // 串口热插拔轮询
    connect(&m_portPollTimer, &QTimer::timeout, this, &ConnectionManager::refreshPorts);
    m_lastPorts = availablePorts();
    m_portPollTimer.start(1000);

    // 恢复链路类型
    m_linkType = normalizeLinkType(SettingsManager::instance()->loadValue("setup.linkType", "UART").toString());
    const QStringList knownLinkTypes = linkTypes();
    if (!knownLinkTypes.contains(m_linkType)) {
        m_linkType = knownLinkTypes.value(0);
    }
    SettingsManager::instance()->saveValue("setup.linkType", m_linkType);

    if (auto *settings = SettingsManager::instance()) {
        m_usbUi->setVendorId(parseIntegerSetting(settings->loadValue(QStringLiteral("setup.usb.vendorId"), m_usbUi->vendorId()), m_usbUi->vendorId()));
        m_usbUi->setProductId(parseIntegerSetting(settings->loadValue(QStringLiteral("setup.usb.productId"), m_usbUi->productId()), m_usbUi->productId()));
        m_usbUi->setInterfaceNumber(settings->loadValue(QStringLiteral("setup.usb.interfaceNumber"), m_usbUi->interfaceNumber()).toInt());
        m_usbUi->setBulkInEndpoint(parseIntegerSetting(settings->loadValue(QStringLiteral("setup.usb.bulkInEndpoint"), m_usbUi->bulkInEndpoint()), m_usbUi->bulkInEndpoint()));
        m_usbUi->setBulkOutEndpoint(parseIntegerSetting(settings->loadValue(QStringLiteral("setup.usb.bulkOutEndpoint"), m_usbUi->bulkOutEndpoint()), m_usbUi->bulkOutEndpoint()));
        m_usbUi->setReadPacketSize(settings->loadValue(QStringLiteral("setup.usb.readPacketSize"), m_usbUi->readPacketSize()).toInt());
        m_usbUi->setTimeoutMs(20);
        m_usbUi->setReadIntervalMs(settings->loadValue(QStringLiteral("setup.usb.readIntervalMs"), m_usbUi->readIntervalMs()).toInt());

        m_canUi->setLibraryPath(QString());
        m_canUi->setDeviceTypeIndex(settings->loadValue(QStringLiteral("setup.can.deviceTypeIndex"), m_canUi->deviceTypeIndex()).toInt());
        m_canUi->setDeviceIndex(settings->loadValue(QStringLiteral("setup.can.deviceIndex"), m_canUi->deviceIndex()).toInt());
        m_canUi->setChannel(settings->loadValue(QStringLiteral("setup.can.channel"), m_canUi->channel()).toInt());
        m_canUi->setTxId(parseIntegerSetting(settings->loadValue(QStringLiteral("setup.can.txId"), m_canUi->txId()), m_canUi->txId()));
        m_canUi->setRxId(parseIntegerSetting(settings->loadValue(QStringLiteral("setup.can.rxId"), m_canUi->rxId()), m_canUi->rxId()));
        m_canUi->setFrameFormat(settings->loadValue(QStringLiteral("setup.can.frameFormat"), m_canUi->frameFormat()).toInt());
        m_canUi->setArbitrationBitrate(settings->loadValue(QStringLiteral("setup.can.arbitrationBitrate"), m_canUi->arbitrationBitrate()).toInt());
        m_canUi->setDataBitrate(settings->loadValue(QStringLiteral("setup.can.dataBitrate"), m_canUi->dataBitrate()).toInt());
        m_canUi->setFramePayloadSize(defaultCanPayloadSize(m_canUi->frameFormat()));
        settings->saveValue(QStringLiteral("setup.can.framePayloadSize"), m_canUi->framePayloadSize());
        m_canUi->setExtended(settings->loadValue(QStringLiteral("setup.can.extended"), m_canUi->extended()).toBool());
        m_canUi->setBrs(settings->loadValue(QStringLiteral("setup.can.brs"), m_canUi->brs()).toBool());
        m_canUi->setTransmitType(settings->loadValue(QStringLiteral("setup.can.transmitType"), m_canUi->transmitType()).toInt());
        m_canUi->setAbitTiming(settings->loadValue(QStringLiteral("setup.can.abitTiming"), m_canUi->abitTiming()).toInt());
        m_canUi->setDbitTiming(settings->loadValue(QStringLiteral("setup.can.dbitTiming"), m_canUi->dbitTiming()).toInt());
        m_canUi->setPollIntervalMs(settings->loadValue(QStringLiteral("setup.can.pollIntervalMs"), m_canUi->pollIntervalMs()).toInt());
        m_canUi->setInterFrameDelayMs(settings->loadValue(QStringLiteral("setup.can.interFrameDelayMs"), m_canUi->interFrameDelayMs()).toInt());
    }
}

ConnectionManager::~ConnectionManager()
{
    closeConnection();
}

// ============================================================================
// 驱动访问
// ============================================================================

IO::HAL_Driver *ConnectionManager::currentDriver() const
{
    if (m_liveDevice && m_liveDevice->isOpen())
        return m_liveDevice->driver();
    return nullptr;
}

IO::Drivers::UART *ConnectionManager::uartDriver()       { return m_uartUi.get(); }
IO::Drivers::UsbBulk *ConnectionManager::usbDriver()     { return m_usbUi.get(); }
IO::Drivers::ZlgCanFd *ConnectionManager::canDriver()    { return m_canUi.get(); }
IO::Drivers::BluetoothLE *ConnectionManager::bleDriver() { return m_bluetoothLEUi.get(); }

QObject *ConnectionManager::uartObj() const    { return m_uartUi.get(); }
QObject *ConnectionManager::usbObj() const     { return m_usbUi.get(); }
QObject *ConnectionManager::canObj() const     { return m_canUi.get(); }
QObject *ConnectionManager::bleObj() const     { return m_bluetoothLEUi.get(); }

IO::HAL_Driver *ConnectionManager::activeUiDriver() const
{
    if (m_linkType == "UART") return m_uartUi.get();
    if (m_linkType == "USB") return m_usbUi.get();
    if (m_linkType == "CAN") return m_canUi.get();
    if (m_linkType == "BLE") return m_bluetoothLEUi.get();
    return nullptr;
}

// ============================================================================
// 总线类型
// ============================================================================

void ConnectionManager::setLinkType(const QString &type)
{
    const QStringList knownLinkTypes = linkTypes();
    const QString normalizedInput = normalizeLinkType(type);
    const QString normalizedType = knownLinkTypes.contains(normalizedInput) ? normalizedInput : knownLinkTypes.value(0);
    if (m_linkType == normalizedType) return;
    setConnecting(false);
    closeConnection();
    m_linkType = normalizedType;
    SettingsManager::instance()->saveValue("setup.linkType", normalizedType);
    createLiveDevice();
    emit linkTypeChanged();
    emit connectionChanged();
}

bool ConnectionManager::isConnected() const
{
    return m_liveDevice && m_liveDevice->isOpen();
}

void ConnectionManager::setErrorReportingSuppressed(bool value)
{
    if (m_errorReportingSuppressed == value)
        return;
    m_errorReportingSuppressed = value;
    emit errorReportingSuppressedChanged();
}

QStringList ConnectionManager::availablePorts() const
{
    return m_uartUi->availablePorts();
}

QStringList ConnectionManager::linkTypes() const
{
    return {"UART", "USB", "CAN", "BLE"};
}

QString ConnectionManager::connectedLinkName() const
{
    if (!isConnected()) return QStringLiteral("未连接");
    if (m_linkType == "UART") return m_uartUi->portName();
    if (m_linkType == "USB") {
        return QStringLiteral("USB VID 0x%1 PID 0x%2")
            .arg(m_usbUi->vendorId(), 4, 16, QLatin1Char('0'))
            .arg(m_usbUi->productId(), 4, 16, QLatin1Char('0'))
            .toUpper();
    }
    if (m_linkType == "CAN") {
        return QStringLiteral("CAN ch%1 TX 0x%2 RX 0x%3")
            .arg(m_canUi->channel())
            .arg(m_canUi->txId(), 0, 16)
            .arg(m_canUi->rxId(), 0, 16)
            .toUpper();
    }
    if (m_linkType == "BLE") {
        int idx = m_bluetoothLEUi->deviceIndex();
        auto names = m_bluetoothLEUi->deviceNames();
        if (idx >= 0 && idx < names.size()) return names[idx];
    }
    return m_linkType;
}

void ConnectionManager::setRxBytes(qint64 v)
{
    m_rxBytes = v;
    m_rxBytesLast = v;
    m_rxSpeed = 0;
    emit statsChanged();
}

void ConnectionManager::setTxBytes(qint64 v)
{
    m_txBytes = v;
    m_txBytesLast = v;
    m_txSpeed = 0;
    emit statsChanged();
}

// ============================================================================
// 连接管理
// ============================================================================

void ConnectionManager::openConnection(bool resetStatistics)
{
    // 总是重建 live device，确保使用最新的 UI 驱动配置
    createLiveDevice();
    if (!m_liveDevice) return;

    if (m_linkType == "BLE") m_bluetoothLEUi->stopScan();

    // 将 live driver 的连接状态/错误信号桥接到 ConnectionManager
    if (auto *drv = m_liveDevice->driver()) {
        connect(drv, &IO::HAL_Driver::connectionChanged,
                this, &ConnectionManager::connectionChanged, Qt::UniqueConnection);
        connect(drv, &IO::HAL_Driver::errorOccurred,
                this, [this](const QString &msg) {
                    setConnecting(false);
                    if (!m_errorReportingSuppressed)
                        emit errorOccurred(msg);
                });
        // 连接失败时清除 connecting 状态
        connect(drv, &IO::HAL_Driver::connectionChanged,
                this, [this]() {
                    if (m_liveDevice && m_liveDevice->isOpen())
                        setConnecting(false);
                });
    }

    if (resetStatistics)
        resetStats();
    setConnecting(true);
    m_liveDevice->open();
    if (!m_liveDevice->isOpen())
        setConnecting(false);

    emit connectionChanged();
}

void ConnectionManager::closeConnection()
{
    setConnecting(false);
    if (m_liveDevice) m_liveDevice->close();
    emit connectionChanged();
}

void ConnectionManager::sendData(const QByteArray &data)
{
    if (!m_liveDevice || !m_liveDevice->isOpen()) return;
    qint64 written = m_liveDevice->write(data);
    if (written > 0) {
        m_txBytes += written;
        emit dataSent(written == data.size() ? data : data.left(static_cast<int>(written)));
        emit statsChanged();
    }
}

void ConnectionManager::refreshPorts()
{
    QStringList current = availablePorts();
    if (current != m_lastPorts) {
        m_lastPorts = current;
        emit availablePortsChanged();
        // 通知 UART 驱动更新 QML uartDriver.availablePorts 绑定
        emit m_uartUi->portsChanged();
    }
}

void ConnectionManager::resetStats()
{
    m_rxBytes = 0;
    m_txBytes = 0;
    m_rxBytesLast = 0;
    m_txBytesLast = 0;
    m_rxSpeed = 0;
    m_txSpeed = 0;
    emit statsChanged();
}

// ============================================================================
// DriverProperty 接口
// ============================================================================

QVariantList ConnectionManager::activeDriverProperties() const
{
    QVariantList list;
    IO::HAL_Driver *drv = activeUiDriver();
    if (!drv) return list;

    for (const auto &prop : drv->driverProperties()) {
        QVariantMap m;
        m["key"]   = prop.key;
        m["label"] = prop.label;
        m["desc"]  = prop.description;
        m["type"]  = static_cast<int>(prop.type);
        m["value"] = prop.value;
        if (!prop.options.isEmpty()) m["options"] = prop.options;
        if (prop.min.isValid()) m["min"] = prop.min;
        if (prop.max.isValid()) m["max"] = prop.max;
        list.append(m);
    }
    return list;
}

void ConnectionManager::setUiDriverProperty(const QString &key, const QVariant &value)
{
    IO::HAL_Driver *drv = activeUiDriver();
    if (drv) drv->setProperty(key, value);
}

// ============================================================================
// Live Device 管理
// ============================================================================

void ConnectionManager::createLiveDevice()
{
    destroyLiveDevice();

    std::unique_ptr<IO::HAL_Driver> driver;

    if (m_linkType == "UART") {
        driver = std::make_unique<IO::Drivers::UART>();
        auto *s = m_uartUi.get();
        auto *d = static_cast<IO::Drivers::UART *>(driver.get());
        d->setPortName(s->portName());
        d->setBaudRate(s->baudRate());
        d->setDataBits(s->dataBits());
        d->setStopBits(s->stopBits());
        d->setParity(s->parity());
    } else if (m_linkType == "USB") {
        driver = std::make_unique<IO::Drivers::UsbBulk>();
        auto *s = m_usbUi.get();
        auto *d = static_cast<IO::Drivers::UsbBulk *>(driver.get());
        d->setVendorId(s->vendorId());
        d->setProductId(s->productId());
        d->setInterfaceNumber(s->interfaceNumber());
        d->setBulkInEndpoint(s->bulkInEndpoint());
        d->setBulkOutEndpoint(s->bulkOutEndpoint());
        d->setReadPacketSize(s->readPacketSize());
        d->setTimeoutMs(20);
        d->setReadIntervalMs(s->readIntervalMs());
        d->setSuppressOpenErrors(s->suppressOpenErrors());
    } else if (m_linkType == "CAN") {
        driver = std::make_unique<IO::Drivers::ZlgCanFd>();
        auto *s = m_canUi.get();
        auto *d = static_cast<IO::Drivers::ZlgCanFd *>(driver.get());
        d->setLibraryPath(QString());
        d->setDeviceType(s->deviceType());
        d->setDeviceIndex(s->deviceIndex());
        d->setChannel(s->channel());
        d->setTxId(s->txId());
        d->setRxId(s->rxId());
        d->setFrameFormat(s->frameFormat());
        d->setArbitrationBitrate(s->arbitrationBitrate());
        d->setDataBitrate(s->dataBitrate());
        d->setFramePayloadSize(defaultCanPayloadSize(s->frameFormat()));
        d->setExtended(s->extended());
        d->setBrs(s->brs());
        d->setTransmitType(s->transmitType());
        d->setAbitTiming(s->abitTiming());
        d->setDbitTiming(s->dbitTiming());
        d->setPollIntervalMs(s->pollIntervalMs());
        d->setInterFrameDelayMs(s->interFrameDelayMs());
    } else if (m_linkType == "BLE") {
        driver = std::make_unique<IO::Drivers::BluetoothLE>();
        auto *s = m_bluetoothLEUi.get();
        auto *d = static_cast<IO::Drivers::BluetoothLE *>(driver.get());
        d->selectDevice(s->deviceIndex());
        d->setTxCharacteristicIndex(s->txCharacteristicIndex());
        d->setRxCharacteristicIndex(s->rxCharacteristicIndex());
    }

    if (driver) {
        // BLE: 转发错误信号供 StatusBar 显示
        auto *bleRaw = qobject_cast<IO::Drivers::BluetoothLE *>(driver.get());
        if (bleRaw) {
            connect(bleRaw, &IO::Drivers::BluetoothLE::bleError,
                    this, [this](const QString &msg) {
                        if (!m_errorReportingSuppressed)
                            emit errorOccurred(msg);
                    }, Qt::UniqueConnection);
        }

        m_liveDevice = std::make_unique<IO::DeviceManager>(0, std::move(driver));
        connect(m_liveDevice.get(), &IO::DeviceManager::frameReady,
                this, &ConnectionManager::onFrameReady);
        connect(m_liveDevice.get(), &IO::DeviceManager::rawDataReceived,
                this, &ConnectionManager::onRawDataReady);
    }
}

void ConnectionManager::destroyLiveDevice()
{
    if (m_liveDevice) {
        m_liveDevice->close();
        disconnect(m_liveDevice.get(), nullptr, this, nullptr);
        m_liveDevice.reset();
    }
}

// ============================================================================
// 数据管线路由
// ============================================================================

void ConnectionManager::onFrameReady(int deviceId, const QVariantMap &frame)
{
    Q_UNUSED(deviceId);
    emit frameReceived(frame);
}

void ConnectionManager::onRawDataReady(int deviceId, const QByteArray &data)
{
    Q_UNUSED(deviceId);
    m_rxBytes += data.size();
    emit rawDataReceived(data);
    emit statsChanged();
}
