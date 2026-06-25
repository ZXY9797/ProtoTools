#ifndef ZLGCANFD_H
#define ZLGCANFD_H

#include "IO/HAL_Driver.h"

#include <QLibrary>
#include <QProcess>
#include <QTimer>

#ifndef Q_DECL_STDCALL
#define Q_DECL_STDCALL
#endif

namespace IO {
namespace Drivers {

class ZlgCanFd : public HAL_Driver
{
    Q_OBJECT
    Q_PROPERTY(QString libraryPath READ libraryPath WRITE setLibraryPath NOTIFY configChanged)
    Q_PROPERTY(int deviceType READ deviceType WRITE setDeviceType NOTIFY configChanged)
    Q_PROPERTY(int deviceTypeIndex READ deviceTypeIndex WRITE setDeviceTypeIndex NOTIFY configChanged)
    Q_PROPERTY(QStringList deviceTypeNames READ deviceTypeNames CONSTANT)
    Q_PROPERTY(int deviceIndex READ deviceIndex WRITE setDeviceIndex NOTIFY configChanged)
    Q_PROPERTY(int channel READ channel WRITE setChannel NOTIFY configChanged)
    Q_PROPERTY(int txId READ txId WRITE setTxId NOTIFY configChanged)
    Q_PROPERTY(int rxId READ rxId WRITE setRxId NOTIFY configChanged)
    Q_PROPERTY(int frameFormat READ frameFormat WRITE setFrameFormat NOTIFY configChanged)
    Q_PROPERTY(int arbitrationBitrate READ arbitrationBitrate WRITE setArbitrationBitrate NOTIFY configChanged)
    Q_PROPERTY(int dataBitrate READ dataBitrate WRITE setDataBitrate NOTIFY configChanged)
    Q_PROPERTY(int framePayloadSize READ framePayloadSize WRITE setFramePayloadSize NOTIFY configChanged)
    Q_PROPERTY(bool extended READ extended WRITE setExtended NOTIFY configChanged)
    Q_PROPERTY(bool brs READ brs WRITE setBrs NOTIFY configChanged)
    Q_PROPERTY(int transmitType READ transmitType WRITE setTransmitType NOTIFY configChanged)
    Q_PROPERTY(int abitTiming READ abitTiming WRITE setAbitTiming NOTIFY configChanged)
    Q_PROPERTY(int dbitTiming READ dbitTiming WRITE setDbitTiming NOTIFY configChanged)
    Q_PROPERTY(int pollIntervalMs READ pollIntervalMs WRITE setPollIntervalMs NOTIFY configChanged)
    Q_PROPERTY(int interFrameDelayMs READ interFrameDelayMs WRITE setInterFrameDelayMs NOTIFY configChanged)

public:
    explicit ZlgCanFd(QObject *parent = nullptr);
    ~ZlgCanFd() override;

    QString driverName() const override { return QStringLiteral("CAN"); }

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

    QString libraryPath() const { return m_libraryPath; }
    void setLibraryPath(const QString &path);

    int deviceType() const { return m_deviceType; }
    void setDeviceType(int type);

    int deviceTypeIndex() const;
    void setDeviceTypeIndex(int index);
    QStringList deviceTypeNames() const;

    int deviceIndex() const { return m_deviceIndex; }
    void setDeviceIndex(int index);

    int channel() const { return m_channel; }
    void setChannel(int channel);

    int txId() const { return m_txId; }
    void setTxId(int id);

    int rxId() const { return m_rxId; }
    void setRxId(int id);

    int frameFormat() const { return m_frameFormat; }
    void setFrameFormat(int format);

    int arbitrationBitrate() const { return m_arbitrationBitrate; }
    void setArbitrationBitrate(int bitrate);

    int dataBitrate() const { return m_dataBitrate; }
    void setDataBitrate(int bitrate);

    int framePayloadSize() const { return m_framePayloadSize; }
    void setFramePayloadSize(int size);

    bool extended() const { return m_extended; }
    void setExtended(bool enabled);

    bool brs() const { return m_brs; }
    void setBrs(bool enabled);

    int transmitType() const { return m_transmitType; }
    void setTransmitType(int type);

    int abitTiming() const { return m_abitTiming; }
    void setAbitTiming(int timing);

    int dbitTiming() const { return m_dbitTiming; }
    void setDbitTiming(int timing);

    int pollIntervalMs() const { return m_pollIntervalMs; }
    void setPollIntervalMs(int intervalMs);

    int interFrameDelayMs() const { return m_interFrameDelayMs; }
    void setInterFrameDelayMs(int delayMs);

signals:
    void configChanged();

private slots:
    void receivePendingFrames();

private:
    using OpenDeviceFn = void *(Q_DECL_STDCALL *)(quint32, quint32, quint32);
    using CloseDeviceFn = quint32 (Q_DECL_STDCALL *)(void *);
    using InitCanFn = void *(Q_DECL_STDCALL *)(void *, quint32, void *);
    using StartCanFn = quint32 (Q_DECL_STDCALL *)(void *);
    using ResetCanFn = quint32 (Q_DECL_STDCALL *)(void *);
    using TransmitCanFn = quint32 (Q_DECL_STDCALL *)(void *, void *, quint32);
    using ReceiveCanFn = quint32 (Q_DECL_STDCALL *)(void *, void *, quint32, qint32);
    using TransmitFdFn = quint32 (Q_DECL_STDCALL *)(void *, void *, quint32);
    using ReceiveFdFn = quint32 (Q_DECL_STDCALL *)(void *, void *, quint32, qint32);
    using SetBitrateFn = quint32 (Q_DECL_STDCALL *)(void *, quint32, quint32);
    using SetValueFn = quint32 (Q_DECL_STDCALL *)(void *, const char *, const char *);
    using ClearBufferFn = quint32 (Q_DECL_STDCALL *)(void *);

    bool loadApi(QString *errorMessage);
    bool bindApi(QString *errorMessage);
    QStringList libraryCandidates() const;
    QString describeCurrentBus() const;
    bool configureChannelPreInit(QString *errorMessage);
    bool configureChannelPostInit(QString *errorMessage);
    bool setChannelValue(const char *name, const QString &value, QString *errorMessage);
    bool setChannelValue(const char *name, int value, QString *errorMessage);
    void unloadApi();
    QString bridgeExecutablePath() const;
    QString bridgeScriptPath() const;
    QString zlgLibraryPathForBridge() const;
    void handleBridgeReadyRead();
    void handleBridgeLine(const QString &line);

    QLibrary m_library;
    QString m_libraryPath;
    QString m_loadedLibraryPath;
    void *m_deviceHandle = nullptr;
    void *m_channelHandle = nullptr;
    QTimer m_pollTimer;
    QProcess *m_bridgeProcess = nullptr;
    QByteArray m_bridgeOutputBuffer;
    bool m_bridgeOpen = false;

    int m_deviceType = 41;
    int m_deviceIndex = 0;
    int m_channel = 0;
    int m_txId = 0x100;
    int m_rxId = 0x200;
    int m_frameFormat = 1;
    int m_arbitrationBitrate = 1000000;
    int m_dataBitrate = 1000000;
    int m_framePayloadSize = 64;
    bool m_extended = false;
    bool m_brs = true;
    int m_transmitType = 0;
    int m_abitTiming = 101166;
    int m_dbitTiming = 101166;
    int m_pollIntervalMs = 5;
    int m_interFrameDelayMs = 0;

    OpenDeviceFn m_openDevice = nullptr;
    CloseDeviceFn m_closeDevice = nullptr;
    InitCanFn m_initCan = nullptr;
    StartCanFn m_startCan = nullptr;
    ResetCanFn m_resetCan = nullptr;
    TransmitCanFn m_transmitCan = nullptr;
    ReceiveCanFn m_receiveCan = nullptr;
    TransmitFdFn m_transmitFd = nullptr;
    ReceiveFdFn m_receiveFd = nullptr;
    SetBitrateFn m_setAbitBaud = nullptr;
    SetBitrateFn m_setDbitBaud = nullptr;
    SetValueFn m_setValue = nullptr;
    ClearBufferFn m_clearBuffer = nullptr;
};

} // namespace Drivers
} // namespace IO

#endif // ZLGCANFD_H
