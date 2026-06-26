#include "IO/Drivers/ZlgCanFd.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QPair>
#include <QProcessEnvironment>
#include <QThread>

#include <algorithm>
#include <array>

namespace {

constexpr quint32 ZCAN_STATUS_OK = 1;
constexpr quint32 ZCAN_TYPE_CAN = 0;
constexpr quint32 ZCAN_TYPE_CANFD = 1;
constexpr int ZCAN_MAX_CAN_PAYLOAD = 8;
constexpr int ZCAN_MAX_FD_PAYLOAD = 64;

struct DeviceTypeDef {
    const char *name;
    int value;
};

constexpr std::array<DeviceTypeDef, 6> DEVICE_TYPES = {{
    {"USBCANFD_200U", 41},
    {"USBCANFD_100U", 42},
    {"USBCANFD_MINI", 43},
    {"PCIE_CANFD_100U", 38},
    {"PCIE_CANFD_200U", 39},
    {"PCIE_CANFD_400U", 40},
}};

struct ZcanCanFdInitConfig {
    quint32 acc_code = 0;
    quint32 acc_mask = 0xFFFFFFFF;
    quint32 abit_timing = 101166;
    quint32 dbit_timing = 101166;
    quint32 brp = 0;
    quint8 filter = 0;
    quint8 mode = 0;
    quint16 pad = 0;
    quint32 reserved = 0;
};

struct ZcanCanInitConfig {
    quint32 acc_code = 0;
    quint32 acc_mask = 0xFFFFFFFF;
    quint32 reserved = 0;
    quint8 filter = 0;
    quint8 timing0 = 0;
    quint8 timing1 = 0;
    quint8 mode = 0;
};

union ZcanInitConfigUnion {
    ZcanCanInitConfig can;
    ZcanCanFdInitConfig canfd;
};

struct ZcanChannelInitConfig {
    quint32 can_type = ZCAN_TYPE_CANFD;
    ZcanInitConfigUnion config = {};
};

struct ZcanCanFrame {
    quint32 can_id : 29;
    quint32 err : 1;
    quint32 rtr : 1;
    quint32 eff : 1;
    quint8 can_dlc;
    quint8 pad;
    quint8 reserved0;
    quint8 reserved1;
    quint8 data[ZCAN_MAX_CAN_PAYLOAD];
};

struct ZcanTransmitCanData {
    ZcanCanFrame frame;
    quint32 transmit_type;
};

struct ZcanReceiveCanData {
    ZcanCanFrame frame;
    quint64 timestamp;
};

struct ZcanCanFdFrame {
    quint32 can_id : 29;
    quint32 err : 1;
    quint32 rtr : 1;
    quint32 eff : 1;
    quint8 length;
    quint8 brs : 1;
    quint8 esi : 1;
    quint8 reserved : 6;
    quint8 reserved0;
    quint8 reserved1;
    quint8 data[ZCAN_MAX_FD_PAYLOAD];
};

struct ZcanTransmitFdData {
    ZcanCanFdFrame frame;
    quint32 transmit_type;
};

struct ZcanReceiveFdData {
    ZcanCanFdFrame frame;
    quint64 timestamp;
};

static_assert(sizeof(ZcanCanFrame) == 16);
static_assert(sizeof(ZcanTransmitCanData) == 20);
static_assert(sizeof(ZcanReceiveCanData) == 24);
static_assert(sizeof(ZcanCanFdFrame) == 72);
static_assert(sizeof(ZcanTransmitFdData) == 76);
static_assert(sizeof(ZcanReceiveFdData) == 80);

QStringList deviceTypeNamesStatic()
{
    QStringList names;
    for (const auto &type : DEVICE_TYPES)
        names.append(QString::fromLatin1(type.name));
    return names;
}

int deviceTypeValueAt(int index)
{
    if (index < 0 || index >= static_cast<int>(DEVICE_TYPES.size()))
        return DEVICE_TYPES.front().value;
    return DEVICE_TYPES[static_cast<size_t>(index)].value;
}

int deviceTypeIndexOf(int value)
{
    for (int i = 0; i < static_cast<int>(DEVICE_TYPES.size()); ++i) {
        if (DEVICE_TYPES[static_cast<size_t>(i)].value == value)
            return i;
    }
    return 0;
}

QPair<quint8, quint8> canTimingForBitrate(int bitrate)
{
    switch (bitrate) {
    case 1000000: return {0x00, 0x14};
    case 800000: return {0x00, 0x16};
    case 500000: return {0x00, 0x1C};
    case 250000: return {0x01, 0x1C};
    case 125000: return {0x03, 0x1C};
    case 100000: return {0x04, 0x1C};
    case 50000: return {0x09, 0x1C};
    default: return {0x00, 0x1C};
    }
}

int clampPayloadSize(int format, int size)
{
    const int maxSize = format == 0 ? ZCAN_MAX_CAN_PAYLOAD : ZCAN_MAX_FD_PAYLOAD;
    return std::clamp(size, 1, maxSize);
}

QString normalizePath(const QString &path)
{
    return QDir::toNativeSeparators(path.trimmed());
}

bool isExistingFile(const QString &path)
{
    return !path.isEmpty() && QFileInfo::exists(path) && QFileInfo(path).isFile();
}

template <typename T>
T resolveSymbol(QLibrary &library, const char *name)
{
    return reinterpret_cast<T>(library.resolve(name));
}

#ifdef Q_OS_WIN
void prependDllSearchPaths(const QString &libraryPath)
{
    const QFileInfo info(libraryPath);
    if (!info.exists())
        return;

    QStringList paths;
    paths.append(info.absolutePath());

    const QString kernelDlls = info.absoluteDir().absoluteFilePath(QStringLiteral("kerneldlls"));
    if (QFileInfo::exists(kernelDlls))
        paths.append(kernelDlls);

    QByteArray pathEnv = qgetenv("PATH");
    const QString currentPath = QString::fromLocal8Bit(pathEnv);
    for (const QString &path : paths) {
        const QString nativePath = QDir::toNativeSeparators(path);
        if (nativePath.isEmpty() || currentPath.contains(nativePath, Qt::CaseInsensitive))
            continue;
        pathEnv.prepend(';');
        pathEnv.prepend(nativePath.toLocal8Bit());
    }
    qputenv("PATH", pathEnv);
}
#endif

} // namespace

namespace IO {
namespace Drivers {

ZlgCanFd::ZlgCanFd(QObject *parent)
    : HAL_Driver(parent)
{
    connect(&m_pollTimer, &QTimer::timeout, this, &ZlgCanFd::receivePendingFrames);
}

ZlgCanFd::~ZlgCanFd()
{
    close();
}

bool ZlgCanFd::open()
{
    if (isOpen())
        return true;

    const QString bridgeExe = bridgeExecutablePath();
    const QString script = bridgeScriptPath();
    if (bridgeExe.isEmpty() && script.isEmpty()) {
        emit errorOccurred(QStringLiteral("zlg_can_bridge helper not found"));
        return false;
    }

    const QString zlgLib = zlgLibraryPathForBridge();
    if (zlgLib.isEmpty()) {
        emit errorOccurred(QStringLiteral("zlgcan.dll not found"));
        return false;
    }

    auto *process = new QProcess(this);
    process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(process, &QProcess::readyReadStandardOutput, this, &ZlgCanFd::handleBridgeReadyRead);
    connect(process, &QProcess::readyReadStandardError, this, [this, process]() {
        const QString text = QString::fromLocal8Bit(process->readAllStandardError()).trimmed();
        if (!text.isEmpty())
            emit errorOccurred(text);
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError) {
        emit errorOccurred(QStringLiteral("CAN bridge error: %1").arg(process->errorString()));
    });
    connect(process, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        const bool wasOpen = m_bridgeOpen;
        m_bridgeOpen = false;
        m_bridgeProcess = nullptr;
        if (wasOpen)
            emit errorOccurred(QStringLiteral("CAN bridge exited: %1/%2").arg(exitCode).arg(static_cast<int>(exitStatus)));
        emit connectionChanged();
    });

    QString program = bridgeExe;
    QStringList args;
    if (program.isEmpty()) {
        program = QStringLiteral("python");
        args << script;
    }
    args << QStringLiteral("--zlg-lib") << QDir::toNativeSeparators(zlgLib)
         << QStringLiteral("--device-type") << QString::number(m_deviceType)
         << QStringLiteral("--device-index") << QString::number(m_deviceIndex)
         << QStringLiteral("--channel") << QString::number(m_channel)
         << QStringLiteral("--tx-id") << QStringLiteral("0x%1").arg(m_txId, 0, 16)
         << QStringLiteral("--rx-id") << QStringLiteral("0x%1").arg(m_rxId, 0, 16)
         << QStringLiteral("--format") << (m_frameFormat == 0 ? QStringLiteral("can") : QStringLiteral("canfd"))
         << QStringLiteral("--arb-baud") << QString::number(m_arbitrationBitrate)
         << QStringLiteral("--data-baud") << QString::number(m_dataBitrate)
         << QStringLiteral("--abit-timing") << QString::number(m_abitTiming)
         << QStringLiteral("--dbit-timing") << QString::number(m_dbitTiming)
         << QStringLiteral("--payload-size") << QString::number(clampPayloadSize(m_frameFormat, m_framePayloadSize))
         << QStringLiteral("--poll-ms") << QString::number(std::max(1, m_pollIntervalMs))
         << QStringLiteral("--inter-frame-delay-ms") << QString::number(std::max(0, m_interFrameDelayMs))
         << QStringLiteral("--transmit-type") << QString::number(m_transmitType);
    if (m_extended)
        args << QStringLiteral("--extended");
    if (m_brs)
        args << QStringLiteral("--brs");

    m_bridgeProcess = process;
    m_bridgeOpen = false;
    m_bridgeOutputBuffer.clear();
    process->start(program, args);
    if (!process->waitForStarted(3000)) {
        emit errorOccurred(QStringLiteral("CAN bridge start failed: %1").arg(process->errorString()));
        process->deleteLater();
        m_bridgeProcess = nullptr;
        return false;
    }

    if (!process->waitForReadyRead(5000)) {
        const QString err = QString::fromLocal8Bit(process->readAllStandardError()).trimmed();
        emit errorOccurred(err.isEmpty() ? QStringLiteral("CAN bridge open timeout") : err);
        close();
        return false;
    }
    handleBridgeReadyRead();
    if (!m_bridgeOpen) {
        close();
        return false;
    }
    emit connectionChanged();
    return true;
}

void ZlgCanFd::close()
{
    if (m_bridgeProcess) {
        QProcess *process = m_bridgeProcess;
        m_bridgeProcess = nullptr;
        m_bridgeOpen = false;
        if (process->state() != QProcess::NotRunning) {
            process->write("CLOSE\n");
            process->waitForBytesWritten(200);
            if (!process->waitForFinished(1000)) {
                process->terminate();
                if (!process->waitForFinished(1000))
                    process->kill();
            }
        }
        process->deleteLater();
    }

    emit connectionChanged();
}

bool ZlgCanFd::isOpen() const
{
    return m_bridgeOpen && m_bridgeProcess && m_bridgeProcess->state() == QProcess::Running;
}

bool ZlgCanFd::configurationOk() const noexcept
{
    return m_deviceType > 0 && m_deviceIndex >= 0 && m_channel >= 0;
}

bool ZlgCanFd::setChannelValue(const char *name, const QString &value, QString *errorMessage)
{
    if (!m_setValue) {
        if (errorMessage)
            *errorMessage = QStringLiteral("ZCAN_SetValue is not available in the selected ZLG library");
        return false;
    }

    const QByteArray path = QStringLiteral("%1/%2")
                                .arg(m_channel)
                                .arg(QString::fromLatin1(name))
                                .toLatin1();
    const QByteArray encodedValue = value.toLatin1();
    qInfo().noquote() << "ZLG SetValue:" << QString::fromLatin1(path) << "=" << value;
    const quint32 result = m_setValue(m_deviceHandle, path.constData(), encodedValue.constData());
    qInfo().noquote() << "ZLG SetValue result:" << result;
    if (result != ZCAN_STATUS_OK) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("ZCAN_SetValue failed: %1=%2 (%3)")
                                .arg(QString::fromLatin1(path))
                                .arg(value)
                                .arg(describeCurrentBus());
        }
        return false;
    }

    return true;
}

bool ZlgCanFd::setChannelValue(const char *name, int value, QString *errorMessage)
{
    return setChannelValue(name, QString::number(value), errorMessage);
}

bool ZlgCanFd::configureChannelPreInit(QString *errorMessage)
{
    if (m_setValue) {
        if (m_frameFormat == 0) {
            return setChannelValue("protocol", 0, errorMessage)
                && setChannelValue("can_baud_rate", m_arbitrationBitrate, errorMessage);
        }

        // Keep this pre-init sequence aligned with the reference updater.
        // Some ZLG DLL builds corrupt the process heap when extra post-init
        // properties are set from a Qt process.
        return setChannelValue("protocol", 1, errorMessage)
            && setChannelValue("canfd_standard", 0, errorMessage)
            && setChannelValue("clock", 60000000, errorMessage)
            && setChannelValue("canfd_abit_baud_rate", m_arbitrationBitrate, errorMessage)
            && setChannelValue("canfd_dbit_baud_rate", m_dataBitrate, errorMessage);
    }

    if (m_frameFormat == 0)
        return true;

    if (m_setAbitBaud && m_setDbitBaud) {
        if (m_setAbitBaud(m_deviceHandle, static_cast<quint32>(m_channel),
                          static_cast<quint32>(m_arbitrationBitrate)) != ZCAN_STATUS_OK) {
            if (errorMessage)
                *errorMessage = QStringLiteral("ZCAN_SetAbitBaud failed: %1").arg(describeCurrentBus());
            return false;
        }
        if (m_setDbitBaud(m_deviceHandle, static_cast<quint32>(m_channel),
                          static_cast<quint32>(m_dataBitrate)) != ZCAN_STATUS_OK) {
            if (errorMessage)
                *errorMessage = QStringLiteral("ZCAN_SetDbitBaud failed: %1").arg(describeCurrentBus());
            return false;
        }
        return true;
    }

    if (errorMessage) {
        *errorMessage = QStringLiteral("Cannot configure CAN FD bitrate: selected ZLG library has neither ZCAN_SetValue nor ZCAN_SetAbitBaud/ZCAN_SetDbitBaud");
    }
    return false;
}

bool ZlgCanFd::configureChannelPostInit(QString *errorMessage)
{
    Q_UNUSED(errorMessage)
    return true;
}

qint64 ZlgCanFd::write(const QByteArray &data)
{
    if (!isOpen() || data.isEmpty())
        return data.isEmpty() ? 0 : -1;

    const QByteArray line = QByteArrayLiteral("TX ")
        + data.toHex(' ').toUpper()
        + QByteArrayLiteral("\n");
    const qint64 queued = m_bridgeProcess->write(line);
    if (queued != line.size()) {
        emit errorOccurred(QStringLiteral("CAN bridge write failed"));
        return -1;
    }
    if (!m_bridgeProcess->waitForBytesWritten(1000)) {
        emit errorOccurred(QStringLiteral("CAN bridge write timeout"));
        return -1;
    }
    return data.size();
}

void ZlgCanFd::setProperty(const QString &key, const QVariant &value)
{
    if (key == "libraryPath") setLibraryPath(value.toString());
    else if (key == "deviceType") setDeviceType(value.toInt());
    else if (key == "deviceTypeIndex") setDeviceTypeIndex(value.toInt());
    else if (key == "deviceIndex") setDeviceIndex(value.toInt());
    else if (key == "channel") setChannel(value.toInt());
    else if (key == "txId") setTxId(value.toInt());
    else if (key == "rxId") setRxId(value.toInt());
    else if (key == "frameFormat") setFrameFormat(value.toInt());
    else if (key == "arbitrationBitrate") setArbitrationBitrate(value.toInt());
    else if (key == "dataBitrate") setDataBitrate(value.toInt());
    else if (key == "framePayloadSize") setFramePayloadSize(value.toInt());
    else if (key == "extended") setExtended(value.toBool());
    else if (key == "brs") setBrs(value.toBool());
    else if (key == "transmitType") setTransmitType(value.toInt());
    else if (key == "abitTiming") setAbitTiming(value.toInt());
    else if (key == "dbitTiming") setDbitTiming(value.toInt());
    else if (key == "pollIntervalMs") setPollIntervalMs(value.toInt());
    else if (key == "interFrameDelayMs") setInterFrameDelayMs(value.toInt());
}

QVariant ZlgCanFd::getProperty(const QString &key) const
{
    if (key == "libraryPath") return m_libraryPath;
    if (key == "deviceType") return m_deviceType;
    if (key == "deviceTypeIndex") return deviceTypeIndex();
    if (key == "deviceIndex") return m_deviceIndex;
    if (key == "channel") return m_channel;
    if (key == "txId") return m_txId;
    if (key == "rxId") return m_rxId;
    if (key == "frameFormat") return m_frameFormat;
    if (key == "arbitrationBitrate") return m_arbitrationBitrate;
    if (key == "dataBitrate") return m_dataBitrate;
    if (key == "framePayloadSize") return m_framePayloadSize;
    if (key == "extended") return m_extended;
    if (key == "brs") return m_brs;
    if (key == "transmitType") return m_transmitType;
    if (key == "abitTiming") return m_abitTiming;
    if (key == "dbitTiming") return m_dbitTiming;
    if (key == "pollIntervalMs") return m_pollIntervalMs;
    if (key == "interFrameDelayMs") return m_interFrameDelayMs;
    return {};
}

QList<IO::DriverProperty> ZlgCanFd::driverProperties() const
{
    return {
        {.key = "libraryPath", .label = "ZLG DLL", .description = "zlgcan.dll path, empty for auto-detect",
         .type = IO::DriverProperty::Text, .value = m_libraryPath},
        {.key = "deviceTypeIndex", .label = "Device", .description = "ZLG device type",
         .type = IO::DriverProperty::ComboBox, .value = deviceTypeIndex(),
         .options = deviceTypeNames()},
        {.key = "deviceIndex", .label = "Index", .description = "ZLG device index",
         .type = IO::DriverProperty::IntField, .value = m_deviceIndex, .min = 0, .max = 32},
        {.key = "channel", .label = "Channel", .description = "CAN channel",
         .type = IO::DriverProperty::IntField, .value = m_channel, .min = 0, .max = 16},
        {.key = "txId", .label = "TX ID", .description = "CAN FD transmit frame ID",
         .type = IO::DriverProperty::IntField, .value = m_txId, .min = 0, .max = 0x1FFFFFFF},
        {.key = "rxId", .label = "RX ID", .description = "CAN FD receive frame ID",
         .type = IO::DriverProperty::IntField, .value = m_rxId, .min = 0, .max = 0x1FFFFFFF},
        {.key = "frameFormat", .label = "Frame", .description = "0=standard CAN, 1=CAN FD",
         .type = IO::DriverProperty::ComboBox, .value = m_frameFormat,
         .options = {QStringLiteral("CAN"), QStringLiteral("CAN FD")}},
        {.key = "arbitrationBitrate", .label = "Arb baud", .description = "CAN arbitration bitrate",
         .type = IO::DriverProperty::IntField, .value = m_arbitrationBitrate},
        {.key = "dataBitrate", .label = "Data baud", .description = "CAN FD data bitrate",
         .type = IO::DriverProperty::IntField, .value = m_dataBitrate},
        {.key = "framePayloadSize", .label = "Frame len", .description = "Payload bytes per CAN frame",
         .type = IO::DriverProperty::IntField, .value = m_framePayloadSize, .min = 1, .max = ZCAN_MAX_FD_PAYLOAD},
        {.key = "extended", .label = "Ext", .description = "Use extended CAN ID",
         .type = IO::DriverProperty::CheckBox, .value = m_extended},
        {.key = "brs", .label = "BRS", .description = "Enable CAN FD bit-rate switching",
         .type = IO::DriverProperty::CheckBox, .value = m_brs},
        {.key = "abitTiming", .label = "ABit", .description = "Arbitration phase timing",
         .type = IO::DriverProperty::IntField, .value = m_abitTiming},
        {.key = "dbitTiming", .label = "DBit", .description = "Data phase timing",
         .type = IO::DriverProperty::IntField, .value = m_dbitTiming},
    };
}

QJsonObject ZlgCanFd::deviceIdentifier() const
{
    QJsonObject id;
    id["libraryPath"] = m_libraryPath;
    id["deviceType"] = m_deviceType;
    id["deviceIndex"] = m_deviceIndex;
    id["channel"] = m_channel;
    id["txId"] = m_txId;
    id["rxId"] = m_rxId;
    id["frameFormat"] = m_frameFormat;
    id["arbitrationBitrate"] = m_arbitrationBitrate;
    id["dataBitrate"] = m_dataBitrate;
    id["framePayloadSize"] = m_framePayloadSize;
    id["extended"] = m_extended;
    return id;
}

bool ZlgCanFd::selectByIdentifier(const QJsonObject &id)
{
    if (id.isEmpty())
        return false;
    setLibraryPath(id.value("libraryPath").toString());
    setDeviceType(id.value("deviceType").toInt(m_deviceType));
    setDeviceIndex(id.value("deviceIndex").toInt(m_deviceIndex));
    setChannel(id.value("channel").toInt(m_channel));
    setTxId(id.value("txId").toInt(m_txId));
    setRxId(id.value("rxId").toInt(m_rxId));
    setFrameFormat(id.value("frameFormat").toInt(m_frameFormat));
    setArbitrationBitrate(id.value("arbitrationBitrate").toInt(m_arbitrationBitrate));
    setDataBitrate(id.value("dataBitrate").toInt(m_dataBitrate));
    setFramePayloadSize(id.value("framePayloadSize").toInt(m_framePayloadSize));
    setExtended(id.value("extended").toBool(m_extended));
    return true;
}

void ZlgCanFd::setLibraryPath(const QString &path)
{
    const QString normalized = normalizePath(path);
    if (m_libraryPath == normalized)
        return;
    m_libraryPath = normalized;
    emit configChanged();
}

void ZlgCanFd::setDeviceType(int type)
{
    if (m_deviceType == type)
        return;
    m_deviceType = type;
    emit configChanged();
}

int ZlgCanFd::deviceTypeIndex() const
{
    return deviceTypeIndexOf(m_deviceType);
}

void ZlgCanFd::setDeviceTypeIndex(int index)
{
    setDeviceType(deviceTypeValueAt(index));
}

QStringList ZlgCanFd::deviceTypeNames() const
{
    return deviceTypeNamesStatic();
}

void ZlgCanFd::setDeviceIndex(int index)
{
    index = std::max(0, index);
    if (m_deviceIndex == index)
        return;
    m_deviceIndex = index;
    emit configChanged();
}

void ZlgCanFd::setChannel(int channel)
{
    channel = std::max(0, channel);
    if (m_channel == channel)
        return;
    m_channel = channel;
    emit configChanged();
}

void ZlgCanFd::setTxId(int id)
{
    id &= 0x1FFFFFFF;
    if (m_txId == id)
        return;
    m_txId = id;
    emit configChanged();
}

void ZlgCanFd::setRxId(int id)
{
    id &= 0x1FFFFFFF;
    if (m_rxId == id)
        return;
    m_rxId = id;
    emit configChanged();
}

void ZlgCanFd::setFrameFormat(int format)
{
    format = format == 0 ? 0 : 1;
    if (m_frameFormat == format)
        return;
    m_frameFormat = format;
    m_framePayloadSize = clampPayloadSize(m_frameFormat, m_framePayloadSize);
    emit configChanged();
}

void ZlgCanFd::setArbitrationBitrate(int bitrate)
{
    bitrate = std::clamp(bitrate, 10000, 1000000);
    if (m_arbitrationBitrate == bitrate)
        return;
    m_arbitrationBitrate = bitrate;
    emit configChanged();
}

void ZlgCanFd::setDataBitrate(int bitrate)
{
    bitrate = std::clamp(bitrate, 10000, 8000000);
    if (m_dataBitrate == bitrate)
        return;
    m_dataBitrate = bitrate;
    emit configChanged();
}

void ZlgCanFd::setFramePayloadSize(int size)
{
    size = clampPayloadSize(m_frameFormat, size);
    if (m_framePayloadSize == size)
        return;
    m_framePayloadSize = size;
    emit configChanged();
}

void ZlgCanFd::setExtended(bool enabled)
{
    if (m_extended == enabled)
        return;
    m_extended = enabled;
    emit configChanged();
}

void ZlgCanFd::setBrs(bool enabled)
{
    if (m_brs == enabled)
        return;
    m_brs = enabled;
    emit configChanged();
}

void ZlgCanFd::setTransmitType(int type)
{
    type = std::max(0, type);
    if (m_transmitType == type)
        return;
    m_transmitType = type;
    emit configChanged();
}

void ZlgCanFd::setAbitTiming(int timing)
{
    if (m_abitTiming == timing)
        return;
    m_abitTiming = timing;
    emit configChanged();
}

void ZlgCanFd::setDbitTiming(int timing)
{
    if (m_dbitTiming == timing)
        return;
    m_dbitTiming = timing;
    emit configChanged();
}

void ZlgCanFd::setPollIntervalMs(int intervalMs)
{
    intervalMs = std::max(1, intervalMs);
    if (m_pollIntervalMs == intervalMs)
        return;
    m_pollIntervalMs = intervalMs;
    if (m_pollTimer.isActive())
        m_pollTimer.start(m_pollIntervalMs);
    emit configChanged();
}

void ZlgCanFd::setInterFrameDelayMs(int delayMs)
{
    delayMs = std::max(0, delayMs);
    if (m_interFrameDelayMs == delayMs)
        return;
    m_interFrameDelayMs = delayMs;
    emit configChanged();
}

QString ZlgCanFd::bridgeExecutablePath() const
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir + QStringLiteral("/zlg_can_bridge.exe"),
    };
    for (const QString &candidate : candidates) {
        const QFileInfo info(candidate);
        if (info.exists() && info.isFile())
            return QDir::toNativeSeparators(info.absoluteFilePath());
    }
    return {};
}

QString ZlgCanFd::bridgeScriptPath() const
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir + QStringLiteral("/zlg_can_bridge.py"),
        QDir(appDir).absoluteFilePath(QStringLiteral("../app/tools/zlg_can_bridge.py")),
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("../../app/tools/zlg_can_bridge.py")),
        QStringLiteral("D:/code/msdk/ProtoDebug/app/tools/zlg_can_bridge.py"),
    };
    for (const QString &candidate : candidates) {
        const QFileInfo info(candidate);
        if (info.exists() && info.isFile())
            return QDir::toNativeSeparators(info.absoluteFilePath());
    }
    return {};
}

QString ZlgCanFd::zlgLibraryPathForBridge() const
{
    if (isExistingFile(m_libraryPath))
        return m_libraryPath;
    const QString appLocal = QCoreApplication::applicationDirPath() + QStringLiteral("/zlgcan.dll");
    if (isExistingFile(appLocal))
        return appLocal;
    for (const QString &candidate : libraryCandidates()) {
        if (isExistingFile(candidate))
            return candidate;
    }
    return {};
}

void ZlgCanFd::handleBridgeReadyRead()
{
    if (!m_bridgeProcess)
        return;
    m_bridgeOutputBuffer += m_bridgeProcess->readAllStandardOutput();
    while (true) {
        const int newline = m_bridgeOutputBuffer.indexOf('\n');
        if (newline < 0)
            break;
        const QByteArray rawLine = m_bridgeOutputBuffer.left(newline).trimmed();
        m_bridgeOutputBuffer.remove(0, newline + 1);
        if (!rawLine.isEmpty())
            handleBridgeLine(QString::fromLocal8Bit(rawLine));
    }
}

void ZlgCanFd::handleBridgeLine(const QString &line)
{
    if (line == QStringLiteral("OPENED")) {
        m_bridgeOpen = true;
        emit connectionChanged();
        return;
    }

    if (line.startsWith(QStringLiteral("RX "))) {
        const QByteArray data = QByteArray::fromHex(line.mid(3).toLatin1());
        if (!data.isEmpty())
            processReceivedData(data);
        return;
    }

    if (line.startsWith(QStringLiteral("ERR "))) {
        emit errorOccurred(line.mid(4));
        return;
    }
}

void ZlgCanFd::receivePendingFrames()
{
    handleBridgeReadyRead();
}

bool ZlgCanFd::loadApi(QString *errorMessage)
{
    if (m_library.isLoaded())
        return true;

    QStringList errors;
    const QStringList candidates = libraryCandidates();
    for (const QString &candidate : candidates) {
#ifdef Q_OS_WIN
        prependDllSearchPaths(candidate);
#endif
        m_library.setFileName(candidate);
        if (!m_library.load()) {
            errors.append(QStringLiteral("%1: %2").arg(candidate, m_library.errorString()));
            continue;
        }

        if (bindApi(errorMessage)) {
            m_loadedLibraryPath = candidate;
            return true;
        }

        errors.append(QStringLiteral("%1: %2").arg(candidate, errorMessage ? *errorMessage : QStringLiteral("bind failed")));
        m_library.unload();
    }

    if (errorMessage) {
        *errorMessage = QStringLiteral("Could not load ZLG CAN library. Set CAN DLL path or ZLGCAN_LIBRARY/ZLG_CAN_LIBRARY; zlgcan.dll may also require the VC++ 2013 x64 runtime (MSVCR120/MSVCP120). Tried: %1")
                            .arg(errors.join(QStringLiteral("; ")));
    }
    return false;
}

bool ZlgCanFd::bindApi(QString *errorMessage)
{
    m_openDevice = resolveSymbol<OpenDeviceFn>(m_library, "ZCAN_OpenDevice");
    m_closeDevice = resolveSymbol<CloseDeviceFn>(m_library, "ZCAN_CloseDevice");
    m_initCan = resolveSymbol<InitCanFn>(m_library, "ZCAN_InitCAN");
    m_startCan = resolveSymbol<StartCanFn>(m_library, "ZCAN_StartCAN");
    m_resetCan = resolveSymbol<ResetCanFn>(m_library, "ZCAN_ResetCAN");
    m_transmitCan = resolveSymbol<TransmitCanFn>(m_library, "ZCAN_Transmit");
    m_receiveCan = resolveSymbol<ReceiveCanFn>(m_library, "ZCAN_Receive");
    m_transmitFd = resolveSymbol<TransmitFdFn>(m_library, "ZCAN_TransmitFD");
    m_receiveFd = resolveSymbol<ReceiveFdFn>(m_library, "ZCAN_ReceiveFD");
    m_setAbitBaud = resolveSymbol<SetBitrateFn>(m_library, "ZCAN_SetAbitBaud");
    m_setDbitBaud = resolveSymbol<SetBitrateFn>(m_library, "ZCAN_SetDbitBaud");
    m_setValue = resolveSymbol<SetValueFn>(m_library, "ZCAN_SetValue");
    m_clearBuffer = resolveSymbol<ClearBufferFn>(m_library, "ZCAN_ClearBuffer");

    if (!m_openDevice || !m_closeDevice || !m_initCan || !m_startCan
        || !m_resetCan || !m_transmitCan || !m_receiveCan || !m_transmitFd || !m_receiveFd) {
        if (errorMessage)
            *errorMessage = QStringLiteral("missing required ZCAN_* symbols");
        return false;
    }

    return true;
}

QStringList ZlgCanFd::libraryCandidates() const
{
    QStringList candidates;
    if (!m_libraryPath.isEmpty())
        candidates.append(m_libraryPath);

    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString envA = env.value(QStringLiteral("ZLGCAN_LIBRARY"));
    const QString envB = env.value(QStringLiteral("ZLG_CAN_LIBRARY"));
    if (!envA.isEmpty()) candidates.append(envA);
    if (!envB.isEmpty()) candidates.append(envB);

    const QString appDir = QCoreApplication::applicationDirPath();
    candidates.append(appDir + QLatin1String("/zlgcan.dll"));
    candidates.append(appDir + QLatin1String("/zlgcan"));

#ifdef Q_OS_WIN
    const QString pf = env.value(QStringLiteral("ProgramW6432"), QStringLiteral("C:/Program Files"));
    const QString pf86 = env.value(QStringLiteral("ProgramFiles(x86)"), QStringLiteral("C:/Program Files (x86)"));
    candidates.append(QStringLiteral("D:/software/EricTool_v3.0.1/zlgcan.dll"));
    candidates.append(pf + QLatin1String("/ZCANPRO/zlgcan.dll"));
    if (sizeof(void *) == 4)
        candidates.append(pf86 + QLatin1String("/ZCANPRO/zlgcan.dll"));
    candidates.append(QStringLiteral("zlgcan"));
#else
    candidates.append(QStringLiteral("libzlgcan.so"));
    candidates.append(QStringLiteral("/usr/local/lib/libzlgcan.so"));
    candidates.append(QStringLiteral("/usr/lib/libzlgcan.so"));
#endif

    QStringList out;
    for (const QString &candidate : candidates) {
        const QString normalized = normalizePath(candidate);
        if (normalized.isEmpty() || out.contains(normalized))
            continue;
        if (normalized.contains(QLatin1Char('/')) || normalized.contains(QLatin1Char('\\'))) {
            if (isExistingFile(normalized))
                out.append(normalized);
        } else {
            out.append(normalized);
        }
    }

    if (out.isEmpty() && !m_libraryPath.isEmpty())
        out.append(m_libraryPath);
    return out;
}

QString ZlgCanFd::describeCurrentBus() const
{
    return QStringLiteral("deviceType=%1 deviceIndex=%2 channel=%3")
        .arg(m_deviceType)
        .arg(m_deviceIndex)
        .arg(m_channel);
}

void ZlgCanFd::unloadApi()
{
    m_openDevice = nullptr;
    m_closeDevice = nullptr;
    m_initCan = nullptr;
    m_startCan = nullptr;
    m_resetCan = nullptr;
    m_transmitCan = nullptr;
    m_receiveCan = nullptr;
    m_transmitFd = nullptr;
    m_receiveFd = nullptr;
    m_setAbitBaud = nullptr;
    m_setDbitBaud = nullptr;
    m_setValue = nullptr;
    m_clearBuffer = nullptr;
    m_loadedLibraryPath.clear();
    if (m_library.isLoaded())
        m_library.unload();
}

} // namespace Drivers
} // namespace IO
