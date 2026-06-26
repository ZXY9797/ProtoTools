#ifndef FIRMWAREUPGRADEMANAGER_H
#define FIRMWAREUPGRADEMANAGER_H

#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QVariantMap>

#include <functional>

class ConnectionManager;

class FirmwareUpgradeManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(int upgradeSuccessCount READ upgradeSuccessCount NOTIFY upgradeSuccessCountChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString firmwarePath READ firmwarePath WRITE setFirmwarePath NOTIFY firmwarePathChanged)
    Q_PROPERTY(QString deviceSummary READ deviceSummary NOTIFY deviceInfoChanged)
    Q_PROPERTY(QString deviceModule READ deviceModule NOTIFY deviceInfoChanged)
    Q_PROPERTY(QString deviceSerialNumber READ deviceSerialNumber NOTIFY deviceInfoChanged)
    Q_PROPERTY(QString deviceModuleId READ deviceModuleId NOTIFY deviceInfoChanged)
    Q_PROPERTY(QString deviceHardwareVersion READ deviceHardwareVersion NOTIFY deviceInfoChanged)
    Q_PROPERTY(QString deviceAppVersion READ deviceAppVersion NOTIFY deviceInfoChanged)
    Q_PROPERTY(QString deviceBootloaderVersion READ deviceBootloaderVersion NOTIFY deviceInfoChanged)
    Q_PROPERTY(QString deviceRuntime READ deviceRuntime NOTIFY deviceInfoChanged)
    Q_PROPERTY(QString deviceModes READ deviceModes NOTIFY deviceInfoChanged)
    Q_PROPERTY(QString deviceRawResponse READ deviceRawResponse NOTIFY deviceInfoChanged)
    Q_PROPERTY(int srcAddr READ srcAddr WRITE setSrcAddr NOTIFY configChanged)
    Q_PROPERTY(int dstAddr READ dstAddr WRITE setDstAddr NOTIFY configChanged)
    Q_PROPERTY(int packetSize READ packetSize WRITE setPacketSize NOTIFY configChanged)
    Q_PROPERTY(int timeoutMs READ timeoutMs WRITE setTimeoutMs NOTIFY configChanged)
    Q_PROPERTY(int retries READ retries WRITE setRetries NOTIFY configChanged)

public:
    explicit FirmwareUpgradeManager(ConnectionManager *connectionManager, QObject *parent = nullptr);

    bool running() const { return m_running; }
    int progress() const { return m_progress; }
    int upgradeSuccessCount() const { return m_stressPassed; }
    QString status() const { return m_status; }
    QString firmwarePath() const { return m_firmwarePath; }
    QString deviceSummary() const { return m_deviceSummary; }
    QString deviceModule() const { return m_deviceModule; }
    QString deviceSerialNumber() const { return m_deviceSerialNumber; }
    QString deviceModuleId() const { return m_deviceModuleId; }
    QString deviceHardwareVersion() const { return m_deviceHardwareVersion; }
    QString deviceAppVersion() const { return m_deviceAppVersion; }
    QString deviceBootloaderVersion() const { return m_deviceBootloaderVersion; }
    QString deviceRuntime() const { return m_deviceRuntime; }
    QString deviceModes() const { return m_deviceModes; }
    QString deviceRawResponse() const { return m_deviceRawResponse; }
    int srcAddr() const { return m_srcAddr; }
    int dstAddr() const { return m_dstAddr; }
    int packetSize() const { return m_requestedPacketSize; }
    int timeoutMs() const { return m_timeoutMs; }
    int retries() const { return m_retries; }

    void setFirmwarePath(const QString &path);
    void setSrcAddr(int value);
    void setDstAddr(int value);
    void setPacketSize(int value);
    void setTimeoutMs(int value);
    void setRetries(int value);

    Q_INVOKABLE QString browseFirmware(const QString &currentPath = QString());
    Q_INVOKABLE void ping();
    Q_INVOKABLE void queryDevice();
    Q_INVOKABLE void startUpgrade();
    Q_INVOKABLE void startStressUpgrade(int count);
    Q_INVOKABLE void cancel();

signals:
    void runningChanged();
    void progressChanged();
    void upgradeSuccessCountChanged();
    void statusChanged();
    void firmwarePathChanged();
    void deviceInfoChanged();
    void configChanged();
    void errorOccurred(const QString &message);

private slots:
    void onFrameReceived(const QVariantMap &frame);
    void onRequestTimeout();

private:
    struct DeviceInfo {
        quint32 appVersion = 0xFFFFFFFF;
        quint32 bootloaderVersion = 0xFFFFFFFF;
        quint32 moduleId = 0;
        quint32 hardwareVersion = 0;
        quint8 supportedModes = 0;
        quint8 runtimeMode = 0xFF;
        QString serialNumber;
        QString moduleName;
        bool valid = false;
    };

    struct EnterUpgradeInfo {
        bool upgradeReady = false;
        bool pushStatusSupported = false;
        quint16 enterUpgradeDelayMs = 0;
        quint16 startUpgradeDelayMs = 0;
        quint16 transferPacketDelayMs = 0;
        quint16 finishUpgradeDelayMs = 0;
        quint16 rebootDeviceDelayMs = 0;
    };

    using ResponseHandler = std::function<void(const QByteArray &)>;
    using FailureHandler = std::function<void(const QString &)>;
    using Continuation = std::function<void()>;

    void setRunning(bool value);
    void setProgress(int value);
    void setUpgradeSuccessCount(int value);
    void setStatus(const QString &value);
    void setDeviceSummary(const QString &value);
    void setDeviceRawResponse(const QString &value);
    void setDeviceDetails(const QString &summary,
                          const QString &module,
                          const QString &serialNumber,
                          const QString &moduleId,
                          const QString &hardwareVersion,
                          const QString &appVersion,
                          const QString &bootloaderVersion,
                          const QString &runtime,
                          const QString &modes,
                          const QString &rawResponse);
    void fail(const QString &message);
    void setUpgradeConnectionHold(bool enabled);

    void resetTransferState();
    void beginUpgradeOnce();
    void startNextStressIteration();
    void finishSuccessfulUpgrade();
    void queryDeviceViaCanHelper();
    void startUpgradeViaCanHelper();
    bool loadAndValidateFirmware();
    bool validateFirmware(const QByteArray &firmware, QString *errorMessage) const;

    void request(quint16 cmd,
                 const QByteArray &data,
                 const QString &context,
                 ResponseHandler onSuccess,
                 FailureHandler onFailure = {},
                 int retriesOverride = -1);
    void sendPendingRequest();
    void clearPendingRequest();

    void queryDeviceInfo(Continuation next, FailureHandler onFailure = {}, int retriesOverride = -1);
    void queryRuntime(std::function<void(int)> next, FailureHandler onFailure = {});
    void requestEnterUpgrade(quint8 requestedMode, std::function<void(const EnterUpgradeInfo &)> next);
    void reboot(int mode, Continuation next);
    void waitRuntime(int expectedRuntime, int timeoutMs, Continuation next);
    void pollRuntime();
    void reopenLinkAndEnterUpgrade(quint8 requestedMode, const EnterUpgradeInfo &enter);
    void scheduleStartUpgradeSession();
    void startUpgradeSession();
    void transferNextPacket();
    void finishUpgrade();
    void rebootToApp();
    void completeUpgrade();

    QByteArray buildFrame(quint16 cmd, quint16 seq, const QByteArray &data) const;
    static QByteArray hexToBytes(QString hex);
    static QString bytesToHex(const QByteArray &data);
    static QByteArray le16(quint16 value);
    static QByteArray le32(quint32 value);
    static quint16 u16(const QByteArray &data, int offset);
    static quint32 u32(const QByteArray &data, int offset);
    static quint8 crc8(const QByteArray &data);
    static quint16 crc16(const QByteArray &data);
    static quint32 crc32(const QByteArray &data);
    static QString formatVersion(quint32 value);
    static QString retName(quint8 ret);
    static int parseNumberString(const QString &value);
    static DeviceInfo parseDeviceInfoPayload(const QByteArray &data);
    static EnterUpgradeInfo parseEnterUpgradePayload(const QByteArray &data);
    static bool isPowerOfTwo(int value);
    static int choosePacketSize(int minSize, int maxSize);
    static quint8 upgradeModeFromFirmware(const QByteArray &firmware);
    static quint32 firmwareImageVersion(const QByteArray &firmware);

    ConnectionManager *m_connectionManager = nullptr;
    QTimer m_requestTimer;
    QElapsedTimer m_pendingReconnectTimer;

    bool m_running = false;
    bool m_cancelled = false;
    bool m_stressMode = false;
    bool m_pendingReconnectActive = false;
    int m_stressTotal = 0;
    int m_stressCurrent = 0;
    int m_stressPassed = 0;
    int m_progress = 0;
    QString m_status;
    QString m_firmwarePath;
    QString m_deviceSummary;
    QString m_deviceModule;
    QString m_deviceSerialNumber;
    QString m_deviceModuleId;
    QString m_deviceHardwareVersion;
    QString m_deviceAppVersion;
    QString m_deviceBootloaderVersion;
    QString m_deviceRuntime;
    QString m_deviceModes;
    QString m_deviceRawResponse;

    int m_srcAddr = 0x0101;
    int m_dstAddr = 0x0500;
    int m_requestedPacketSize = 65535;
    int m_timeoutMs = 3000;
    int m_retries = 3;

    quint16 m_nextSeq = 1;
    quint16 m_pendingSeq = 0;
    quint16 m_pendingCmd = 0;
    QByteArray m_pendingFrame;
    QString m_pendingContext;
    int m_pendingRetriesLeft = 0;
    ResponseHandler m_pendingSuccess;
    FailureHandler m_pendingFailure;

    QByteArray m_firmware;
    quint32 m_firmwareCrc = 0;
    quint32 m_targetImageVersion = 0xFFFFFFFF;
    quint8 m_requestedUpgradeMode = 1;
    DeviceInfo m_deviceInfo;
    EnterUpgradeInfo m_upgradeTiming;
    int m_runtime = -1;
    int m_effectivePacketSize = 512;
    int m_transferOffset = 0;
    quint32 m_packetIndex = 0;

    int m_waitRuntimeExpected = -1;
    int m_waitRuntimeTimeoutMs = 0;
    QElapsedTimer m_waitRuntimeTimer;
    Continuation m_waitRuntimeContinuation;
};

#endif // FIRMWAREUPGRADEMANAGER_H
