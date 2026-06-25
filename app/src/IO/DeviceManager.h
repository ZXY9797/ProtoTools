#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include <memory>
#include <QObject>
#include <QPointer>
#include <QIODevice>
#include <QByteArray>

#include "IO/HAL_Driver.h"
#include "IO/FrameProcessor.h"

namespace IO {

/**
 * @brief 设备管理器 — 封装一个 HAL driver + FrameProcessor 的完整生命周期
 *
 * 设计借鉴 Serial-Studio 的 DeviceManager：
 * - 持有独立 driver 实例（非单例）
 * - 管理 FrameProcessor（线程安全的数据管线）
 * - 发射 frameReady / rawDataReceived 给 ConnectionManager 路由
 *
 * 职责边界：
 * - ConnectionManager 负责：连接/断开/切换总线/统计
 * - DeviceManager 负责：driver 生命周期、数据管线
 */
class DeviceManager : public QObject
{
    Q_OBJECT

public:
    explicit DeviceManager(int deviceId,
                           std::unique_ptr<HAL_Driver> driver,
                           QObject *parent = nullptr);
    ~DeviceManager();

    int deviceId() const noexcept { return m_deviceId; }
    bool isOpen() const;
    bool isWritable() const;
    HAL_Driver *driver() const noexcept { return m_driver.get(); }
    FrameProcessor *frameProcessor() { return &m_frameProcessor; }

    qint64 write(const QByteArray &data);

public slots:
    void open(QIODevice::OpenMode mode = QIODevice::ReadWrite);
    void close();

signals:
    void frameReady(int deviceId, const QVariantMap &frame);
    void rawDataReceived(int deviceId, const QByteArray &data);

private slots:
    void onDriverDataReceived(const QByteArray &data);

private:
    int m_deviceId;
    std::unique_ptr<HAL_Driver> m_driver;
    FrameProcessor m_frameProcessor;
};

} // namespace IO

#endif // DEVICEMANAGER_H
