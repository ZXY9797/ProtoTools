#ifndef CONNECTIONMANAGER_H
#define CONNECTIONMANAGER_H

#include <memory>
#include <QObject>
#include <QStringList>
#include <QVariantMap>
#include <QTimer>

#include "IO/HAL_Driver.h"
#include "IO/DeviceManager.h"

namespace IO { namespace Drivers {
    class UART;
    class UsbBulk;
    class ZlgCanFd;
    class BluetoothLE;
}}

/**
 * @brief 连接管理器 — IO 层统一入口
 *
 * UI-config 驱动：每种总线一个实例，供 Setup 面板通过 driverProperties() 动态生成表单。
 * Live DeviceManager：实际连接时创建独立 driver 实例，封装 FrameProcessor 管线。
 *
 * QML 接口：
 *   - linkType / linkTypes → 总线类型切换
 *   - connected / connectedLinkName → 连接状态
 *   - rxBytes / txBytes / rxSpeed / txSpeed / linkSpeed → 统计
 *   - activeDriverProperties() → 当前总线类型的配置属性列表
 *   - setUiDriverProperty(key, value) → 设置属性
 */
class ConnectionManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString linkType READ linkType WRITE setLinkType NOTIFY linkTypeChanged)
    Q_PROPERTY(QStringList linkTypes READ linkTypes CONSTANT)
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectionChanged)
    Q_PROPERTY(bool connecting READ isConnecting NOTIFY connectingChanged)
    Q_PROPERTY(bool persistSettings READ persistSettings WRITE setPersistSettings NOTIFY persistSettingsChanged)
    Q_PROPERTY(bool errorReportingSuppressed READ errorReportingSuppressed WRITE setErrorReportingSuppressed NOTIFY errorReportingSuppressedChanged)
    Q_PROPERTY(QStringList availablePorts READ availablePorts NOTIFY availablePortsChanged)
    Q_PROPERTY(QString connectedLinkName READ connectedLinkName NOTIFY connectionChanged)

    Q_PROPERTY(qint64 rxBytes READ rxBytes WRITE setRxBytes NOTIFY statsChanged)
    Q_PROPERTY(qint64 txBytes READ txBytes WRITE setTxBytes NOTIFY statsChanged)
    Q_PROPERTY(qint64 rxSpeed READ rxSpeed NOTIFY statsChanged)
    Q_PROPERTY(qint64 txSpeed READ txSpeed NOTIFY statsChanged)
    Q_PROPERTY(qint64 linkSpeed READ linkSpeed NOTIFY statsChanged)

    /// 终端模式：终端 tab 激活时为 true，此时 Lua 回调暂停，ScriptEditor 隐藏
    Q_PROPERTY(bool terminalMode READ isTerminalMode WRITE setTerminalMode NOTIFY terminalModeChanged)

    // 各驱动对象（供 QML 驱动面板直接绑定属性）
    Q_PROPERTY(QObject* uart READ uartObj CONSTANT)
    Q_PROPERTY(QObject* usb READ usbObj CONSTANT)
    Q_PROPERTY(QObject* can READ canObj CONSTANT)
    Q_PROPERTY(QObject* ble READ bleObj CONSTANT)

public:
    explicit ConnectionManager(QObject *parent = nullptr);
    ~ConnectionManager() override;

    IO::HAL_Driver *currentDriver() const;
    IO::Drivers::UART *uartDriver();
    IO::Drivers::UsbBulk *usbDriver();
    IO::Drivers::ZlgCanFd *canDriver();
    IO::Drivers::BluetoothLE *bleDriver();

    // ========== 属性 ==========
    QString linkType() const { return m_linkType; }
    void setLinkType(const QString &type);
    bool isConnected() const;
    bool transportOpen() const;
    bool persistSettings() const { return m_persistSettings; }
    void setPersistSettings(bool value);
    void setConnectionStateHold(bool enabled, int timeoutMs = 60000);
    bool errorReportingSuppressed() const { return m_errorReportingSuppressed; }
    void setErrorReportingSuppressed(bool value);
    QStringList availablePorts() const;
    QStringList linkTypes() const;

    qint64 rxBytes() const { return m_rxBytes; }
    void setRxBytes(qint64 v);
    qint64 txBytes() const { return m_txBytes; }
    void setTxBytes(qint64 v);
    qint64 rxSpeed() const { return m_rxSpeed; }
    qint64 txSpeed() const { return m_txSpeed; }
    qint64 linkSpeed() const { return m_rxSpeed + m_txSpeed; }
    QString connectedLinkName() const;

    QObject *uartObj() const;
    QObject *usbObj() const;
    QObject *canObj() const;
    QObject *bleObj() const;

    // ========== DriverProperty 接口 ==========
    /// 返回当前总线类型 UI-driver 的配置属性列表（QML 动态生成表单）
    Q_INVOKABLE QVariantList activeDriverProperties() const;

    /// 设置当前总线类型 UI-driver 的配置属性
    Q_INVOKABLE void setUiDriverProperty(const QString &key, const QVariant &value);

    // ========== 操作 ==========
    Q_INVOKABLE void openConnection(bool resetStatistics = true);
    Q_INVOKABLE void closeConnection();
    void closeConnection(bool forceVisibleDisconnect);
    Q_INVOKABLE void sendData(const QByteArray &data);
    Q_INVOKABLE void refreshPorts();
    Q_INVOKABLE void resetStats();

    bool isConnecting() const { return m_connecting; }
    void setConnecting(bool v) {
        if (m_connecting != v) {
            m_connecting = v;
            emit connectingChanged();
        }
    }

    bool isTerminalMode() const { return m_terminalMode; }
    void setTerminalMode(bool v) {
        if (m_terminalMode != v) {
            m_terminalMode = v;
            emit terminalModeChanged();
        }
    }

signals:
    void linkTypeChanged();
    void connectionChanged();
    void availablePortsChanged();
    void statsChanged();
    void errorOccurred(const QString &error);
    void terminalModeChanged();
    void connectingChanged();
    void persistSettingsChanged();
    void errorReportingSuppressedChanged();

    // 数据管线
    void frameReceived(const QVariantMap &frame);
    void rawDataReceived(const QByteArray &data);
    void dataSent(const QByteArray &data);

private slots:
    void onFrameReady(int deviceId, const QVariantMap &frame);
    void onRawDataReady(int deviceId, const QByteArray &data);
    void onDriverConnectionChanged();
    void onConnectionLossTimeout();

private:
    void createLiveDevice();
    void destroyLiveDevice();
    void updateVisibleConnection(bool forceDisconnect = false);
    void setVisibleConnected(bool value);

    IO::HAL_Driver *activeUiDriver() const;

    std::unique_ptr<IO::Drivers::UART> m_uartUi;
    std::unique_ptr<IO::Drivers::UsbBulk> m_usbUi;
    std::unique_ptr<IO::Drivers::ZlgCanFd> m_canUi;
    std::unique_ptr<IO::Drivers::BluetoothLE> m_bluetoothLEUi;

    std::unique_ptr<IO::DeviceManager> m_liveDevice;

    QString m_linkType = "UART";
    bool m_terminalMode = false;
    bool m_connecting = false;
    bool m_errorReportingSuppressed = false;
    bool m_persistSettings = true;
    bool m_holdConnectionState = false;
    bool m_visibleConnected = false;
    int m_connectionLossTimeoutMs = 60000;

    qint64 m_rxBytes = 0;
    qint64 m_txBytes = 0;
    qint64 m_rxSpeed = 0;
    qint64 m_txSpeed = 0;
    qint64 m_rxBytesLast = 0;
    qint64 m_txBytesLast = 0;
    QTimer m_speedTimer;
    QTimer m_portPollTimer;
    QTimer m_connectionLossTimer;
    QStringList m_lastPorts;
};

#endif // CONNECTIONMANAGER_H
