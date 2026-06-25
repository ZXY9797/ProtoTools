#include "IO/DeviceManager.h"

namespace IO {

DeviceManager::DeviceManager(int deviceId,
                             std::unique_ptr<HAL_Driver> driver,
                             QObject *parent)
    : QObject(parent)
    , m_deviceId(deviceId)
    , m_driver(std::move(driver))
{
    if (m_driver) {
        // 驱动数据 → 帧处理器
        connect(m_driver.get(), &HAL_Driver::dataReceived,
                this, &DeviceManager::onDriverDataReceived);

        // 帧处理器产出完整帧 → 直接转发
        connect(&m_frameProcessor, &FrameProcessor::frameReceived,
                this, [this](const QVariantMap &frame) {
                    emit frameReady(m_deviceId, frame);
                });
    }
}

DeviceManager::~DeviceManager()
{
    close();
}

bool DeviceManager::isOpen() const
{
    return m_driver && m_driver->isOpen();
}

bool DeviceManager::isWritable() const
{
    return m_driver && m_driver->isOpen() && m_driver->isWritable();
}

qint64 DeviceManager::write(const QByteArray &data)
{
    if (!m_driver || !m_driver->isOpen())
        return -1;
    return m_driver->write(data);
}

void DeviceManager::open(QIODevice::OpenMode mode)
{
    if (!m_driver)
        return;
    if (!m_driver->isOpen()) {
        m_driver->open();
    }
}

void DeviceManager::close()
{
    if (m_driver && m_driver->isOpen()) {
        m_driver->flushBuffer();
        m_driver->close();
    }
}

void DeviceManager::onDriverDataReceived(const QByteArray &data)
{
    // 发射原始数据（给终端等消费者）
    emit rawDataReceived(m_deviceId, data);

    // 喂入帧处理器
    m_frameProcessor.feedData(data);
}

} // namespace IO
