#include "DataModel/FirmwareUpgradeManager.h"

#include "IO/ConnectionManager.h"
#include "IO/Drivers/UsbBulk.h"
#include "IO/Drivers/ZlgCanFd.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QTimer>

#include <algorithm>

namespace {

constexpr quint8 SOP = 0xAA;
constexpr quint8 VERSION = 0x01;
constexpr int HEADER_SIZE = 10;
constexpr int CRC16_SIZE = 2;
constexpr quint8 NEXT_HEADER_GENERIC = 0x01;
constexpr int GENERIC_HEADER_SIZE = 6;
constexpr quint16 FLAG_RESPONSE = 1u << 0;
constexpr quint16 FLAG_ACK_REQUIRED = 1u << 1;

constexpr quint16 CMD_PING = 0x0000;
constexpr quint16 CMD_GET_DEVICE_INFO = 0x0001;
constexpr quint16 CMD_REQUEST_ENTER_UPGRADE = 0x0002;
constexpr quint16 CMD_START_UPGRADE = 0x0003;
constexpr quint16 CMD_TRANSFER_PACKET = 0x0004;
constexpr quint16 CMD_FINISH_UPGRADE = 0x0005;
constexpr quint16 CMD_REBOOT = 0x0006;
constexpr quint16 CMD_PUSH_UPGRADE_STATUS = 0x0020;

constexpr quint8 UPGRADE_MODE_APP = 1u << 0;
constexpr quint8 UPGRADE_MODE_BOOTLOADER = 1u << 1;
constexpr int RUNTIME_APP = 0;
constexpr int RUNTIME_BOOTLOADER = 1;
constexpr int REBOOT_NORMAL = 0;
constexpr int REBOOT_BOOTLOADER = 1;
constexpr quint32 MISSING_VERSION = 0xFFFFFFFF;
constexpr quint32 FIRMWARE_MAGIC = 0x3146574B;
constexpr int MIN_FIRMWARE_HEADER_SIZE = 164;
constexpr int DEFAULT_ENTER_UPGRADE_DELAY_MS = 1000;
constexpr int DEFAULT_REBOOT_DELAY_MS = 10;
constexpr int DEFAULT_RUNTIME_SWITCH_TIMEOUT_MS = 5000;
constexpr int USB_RUNTIME_SWITCH_TIMEOUT_MS = 15000;
constexpr int RUNTIME_POLL_INTERVAL_MS = 300;
constexpr quint32 FIRMWARE_TYPE_APP = 0;
constexpr quint32 FIRMWARE_TYPE_BOOTLOADER = 1;

QString runtimeName(int runtime)
{
    return runtime == RUNTIME_BOOTLOADER ? QStringLiteral("bootloader")
                                         : QStringLiteral("app");
}

QString hexNumber(qulonglong value, int minDigits)
{
    return QStringLiteral("0x%1").arg(QStringLiteral("%1").arg(value, minDigits, 16, QChar('0')).toUpper());
}

QString canHelperScriptPath()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir + QStringLiteral("/kp_canfd_firmware_update.py"),
        QStringLiteral("D:/code/knowin_code/rtos_sdk/tools/kp_firmware_update/kp_canfd_firmware_update.py"),
        QStringLiteral("D:/code/knowin_code/rtos_sdk_new/tools/kp_firmware_update/kp_canfd_firmware_update.py"),
    };
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate))
            return QDir::toNativeSeparators(candidate);
    }
    return {};
}

} // namespace

FirmwareUpgradeManager::FirmwareUpgradeManager(ConnectionManager *connectionManager, QObject *parent)
    : QObject(parent)
    , m_connectionManager(connectionManager)
{
    m_requestTimer.setSingleShot(true);
    connect(&m_requestTimer, &QTimer::timeout, this, &FirmwareUpgradeManager::onRequestTimeout);
    if (m_connectionManager) {
        connect(m_connectionManager, &ConnectionManager::frameReceived,
                this, &FirmwareUpgradeManager::onFrameReceived);
    }
}

void FirmwareUpgradeManager::setFirmwarePath(const QString &path)
{
    if (m_firmwarePath == path)
        return;
    m_firmwarePath = path;
    emit firmwarePathChanged();
}

void FirmwareUpgradeManager::setSrcAddr(int value)
{
    value &= 0xFFFF;
    if (m_srcAddr == value)
        return;
    m_srcAddr = value;
    emit configChanged();
}

void FirmwareUpgradeManager::setDstAddr(int value)
{
    value &= 0xFFFF;
    if (m_dstAddr == value)
        return;
    m_dstAddr = value;
    emit configChanged();
}

void FirmwareUpgradeManager::setPacketSize(int value)
{
    value = std::clamp(value, 1, 65535);
    if (m_requestedPacketSize == value)
        return;
    m_requestedPacketSize = value;
    emit configChanged();
}

void FirmwareUpgradeManager::setTimeoutMs(int value)
{
    value = std::clamp(value, 100, 30000);
    if (m_timeoutMs == value)
        return;
    m_timeoutMs = value;
    emit configChanged();
}

void FirmwareUpgradeManager::setRetries(int value)
{
    value = std::clamp(value, 0, 20);
    if (m_retries == value)
        return;
    m_retries = value;
    emit configChanged();
}

QString FirmwareUpgradeManager::browseFirmware(const QString &currentPath)
{
    const QString startDir = QFileInfo(currentPath.isEmpty() ? m_firmwarePath : currentPath).absolutePath();
    const QString path = QFileDialog::getOpenFileName(nullptr,
                                                      QStringLiteral("Select firmware"),
                                                      startDir,
                                                      QStringLiteral("Firmware (*.bin);;All files (*.*)"));
    if (!path.isEmpty())
        setFirmwarePath(path);
    return path;
}

void FirmwareUpgradeManager::queryDevice()
{
    if (m_running)
        return;
    if (m_connectionManager
        && m_connectionManager->linkType() == QStringLiteral("CAN")
        && !m_connectionManager->isConnected()) {
        queryDeviceViaCanHelper();
        return;
    }
    if (!m_connectionManager || !m_connectionManager->isConnected()) {
        fail(QStringLiteral("link is not connected"));
        return;
    }

    resetTransferState();
    setRunning(true);
    setProgress(0);
    setStatus(QStringLiteral("query device info"));

    queryDeviceInfo([this]() {
        m_runtime = m_deviceInfo.runtimeMode;
        setStatus(QStringLiteral("runtime=%1").arg(runtimeName(m_runtime)));
        setRunning(false);
    });
}

void FirmwareUpgradeManager::queryDeviceViaCanHelper()
{
    if (!m_connectionManager) {
        fail(QStringLiteral("link manager is not available"));
        return;
    }

    const QString script = canHelperScriptPath();
    if (script.isEmpty()) {
        fail(QStringLiteral("CAN helper script not found"));
        return;
    }

    auto *can = m_connectionManager->canDriver();
    if (!can) {
        fail(QStringLiteral("CAN driver config is not available"));
        return;
    }

    resetTransferState();
    setRunning(true);
    setProgress(0);
    setStatus(QStringLiteral("query device info"));

    QString zlgLib = can->libraryPath();
    if (zlgLib.trimmed().isEmpty())
        zlgLib = QCoreApplication::applicationDirPath() + QStringLiteral("/zlgcan.dll");

    QStringList args;
    args << script
         << QStringLiteral("--zlg-lib") << QDir::toNativeSeparators(zlgLib)
         << QStringLiteral("--device-type") << QString::number(can->deviceType())
         << QStringLiteral("--device-index") << QString::number(can->deviceIndex())
         << QStringLiteral("--channel") << QString::number(can->channel())
         << QStringLiteral("--tx-id") << QStringLiteral("0x%1").arg(can->txId(), 0, 16)
         << QStringLiteral("--rx-id") << QStringLiteral("0x%1").arg(can->rxId(), 0, 16)
         << QStringLiteral("--abit-timing") << QString::number(can->abitTiming())
         << QStringLiteral("--dbit-timing") << QString::number(can->dbitTiming())
         << (can->brs() ? QStringLiteral("--brs") : QStringLiteral("--no-brs"))
         << QStringLiteral("--info-only");
    if (can->extended())
        args << QStringLiteral("--extended");

    auto *process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);
    connect(process, &QProcess::finished, this,
            [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
        const QString output = QString::fromLocal8Bit(process->readAll()).trimmed();
        process->deleteLater();
        setDeviceRawResponse(output);

        if (exitStatus != QProcess::NormalExit || exitCode != 0) {
            fail(QStringLiteral("CAN helper failed: exit=%1 %2").arg(exitCode).arg(output));
            return;
        }

        const QRegularExpression moduleRe(
            QStringLiteral("\\[device\\]\\s+module=([^\\s]*)\\s+sn=([^\\s]*)\\s+module_id=0x([0-9A-Fa-f]+)\\s+hw=0x([0-9A-Fa-f]+)\\s+modes=0x([0-9A-Fa-f]+)"));
        const QRegularExpression versionRe(QStringLiteral("\\[device\\]\\s+app=([0-9.]+)\\s+bl=([0-9.]+)"));
        const QRegularExpression runtimeRe(QStringLiteral("\\[device\\]\\s+runtime=([^\\s]+)"));

        const auto moduleMatch = moduleRe.match(output);
        const auto versionMatch = versionRe.match(output);
        const auto runtimeMatch = runtimeRe.match(output);
        if (!moduleMatch.hasMatch() || !versionMatch.hasMatch()) {
            fail(QStringLiteral("CAN helper returned unrecognized output: %1").arg(output));
            return;
        }

        const QString runtime = runtimeMatch.hasMatch() ? runtimeMatch.captured(1) : QStringLiteral("unknown");
        const QString module = moduleMatch.captured(1);
        const QString serialNumber = moduleMatch.captured(2);
        const QString moduleId = QStringLiteral("0x%1").arg(moduleMatch.captured(3).toUpper());
        const QString hardwareVersion = QStringLiteral("0x%1").arg(moduleMatch.captured(4).toUpper());
        const QString appVersion = versionMatch.captured(1);
        const QString bootloaderVersion = versionMatch.captured(2);
        const QString modes = QStringLiteral("0x%1").arg(moduleMatch.captured(5).toUpper());
        setDeviceDetails(QStringLiteral("module=%1 sn=%2 module_id=%3 hw=%4 app=%5 bl=%6 runtime=%7 modes=%8")
                             .arg(module, serialNumber, moduleId, hardwareVersion, appVersion, bootloaderVersion, runtime, modes),
                         module,
                         serialNumber,
                         moduleId,
                         hardwareVersion,
                         appVersion,
                         bootloaderVersion,
                         runtime,
                         modes,
                         output);
        setStatus(QStringLiteral("runtime=%1").arg(runtime));
        setRunning(false);
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError) {
        const QString error = process->errorString();
        process->deleteLater();
        fail(QStringLiteral("CAN helper error: %1").arg(error));
    });
    process->start(QStringLiteral("python"), args);
}

void FirmwareUpgradeManager::ping()
{
    if (m_running)
        return;
    if (!m_connectionManager || !m_connectionManager->isConnected()) {
        fail(QStringLiteral("link is not connected"));
        return;
    }

    resetTransferState();
    setRunning(true);
    setProgress(0);
    setStatus(QStringLiteral("ping"));
    request(CMD_PING, {}, QStringLiteral("ping"),
            [this](const QByteArray &data) {
        if (!data.isEmpty() && static_cast<quint8>(data[0]) != 0) {
            fail(QStringLiteral("ping: ret=%1").arg(retName(static_cast<quint8>(data[0]))));
            return;
        }
        setStatus(QStringLiteral("ping ok"));
        setRunning(false);
    });
}

void FirmwareUpgradeManager::startUpgradeViaCanHelper()
{
    if (!m_connectionManager) {
        fail(QStringLiteral("link manager is not available"));
        return;
    }
    if (m_firmwarePath.trimmed().isEmpty()) {
        fail(QStringLiteral("firmware path is empty"));
        return;
    }
    if (!loadAndValidateFirmware())
        return;

    const QString script = canHelperScriptPath();
    if (script.isEmpty()) {
        fail(QStringLiteral("CAN helper script not found"));
        return;
    }

    auto *can = m_connectionManager->canDriver();
    if (!can) {
        fail(QStringLiteral("CAN driver config is not available"));
        return;
    }

    resetTransferState();
    setRunning(true);
    setProgress(0);
    setStatus(QStringLiteral("start CAN upgrade"));

    QString zlgLib = can->libraryPath();
    if (zlgLib.trimmed().isEmpty())
        zlgLib = QCoreApplication::applicationDirPath() + QStringLiteral("/zlgcan.dll");

    QStringList args;
    args << script
         << QStringLiteral("--zlg-lib") << QDir::toNativeSeparators(zlgLib)
         << QStringLiteral("--device-type") << QString::number(can->deviceType())
         << QStringLiteral("--device-index") << QString::number(can->deviceIndex())
         << QStringLiteral("--channel") << QString::number(can->channel())
         << QStringLiteral("--tx-id") << QStringLiteral("0x%1").arg(can->txId(), 0, 16)
         << QStringLiteral("--rx-id") << QStringLiteral("0x%1").arg(can->rxId(), 0, 16)
         << QStringLiteral("--abit-timing") << QString::number(can->abitTiming())
         << QStringLiteral("--dbit-timing") << QString::number(can->dbitTiming())
         << (can->brs() ? QStringLiteral("--brs") : QStringLiteral("--no-brs"))
         << QStringLiteral("--firmware") << QDir::toNativeSeparators(m_firmwarePath)
         << QStringLiteral("--src-addr") << QStringLiteral("0x%1").arg(m_srcAddr, 0, 16)
         << QStringLiteral("--dst-addr") << QStringLiteral("0x%1").arg(m_dstAddr, 0, 16)
         << QStringLiteral("--packet-size") << QString::number(65535)
         << QStringLiteral("--timeout-s") << QString::number(std::max(1, m_timeoutMs) / 1000.0, 'f', 3)
         << QStringLiteral("--retries") << QString::number(m_retries);
    if (can->extended())
        args << QStringLiteral("--extended");

    auto *process = new QProcess(this);
    auto *outputBuffer = new QString;
    connect(process, &QObject::destroyed, this, [outputBuffer]() {
        delete outputBuffer;
    });
    process->setProcessChannelMode(QProcess::MergedChannels);

    connect(process, &QProcess::readyReadStandardOutput, this, [this, process, outputBuffer]() {
        const QString chunk = QString::fromLocal8Bit(process->readAllStandardOutput());
        outputBuffer->append(chunk);

        QRegularExpression progressRe(QStringLiteral("(\\d+)/(\\d+)\\s+bytes\\s+(\\d+)%"));
        auto it = progressRe.globalMatch(*outputBuffer);
        QRegularExpressionMatch lastProgress;
        while (it.hasNext())
            lastProgress = it.next();
        if (lastProgress.hasMatch()) {
            setProgress(lastProgress.captured(3).toInt());
            setStatus(QStringLiteral("transfer %1/%2")
                .arg(lastProgress.captured(1))
                .arg(lastProgress.captured(2)));
        } else if (chunk.contains(QStringLiteral("[upgrade]"))) {
            const QStringList lines = chunk.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);
            if (!lines.isEmpty())
                setStatus(lines.last().trimmed());
        }
    });

    connect(process, &QProcess::finished, this,
            [this, process, outputBuffer](int exitCode, QProcess::ExitStatus exitStatus) {
        const QString remaining = QString::fromLocal8Bit(process->readAllStandardOutput());
        outputBuffer->append(remaining);
        const QString output = outputBuffer->trimmed();
        process->deleteLater();
        setDeviceRawResponse(output);

        if (m_cancelled) {
            if (m_stressMode) {
                m_stressMode = false;
            }
            setRunning(false);
            setStatus(QStringLiteral("cancelled"));
            return;
        }

        if (exitStatus != QProcess::NormalExit || exitCode != 0) {
            fail(QStringLiteral("CAN upgrade helper failed: exit=%1 %2").arg(exitCode).arg(output));
            return;
        }

        const QRegularExpression moduleRe(
            QStringLiteral("\\[device\\]\\s+module=([^\\s]*)\\s+sn=([^\\s]*)\\s+module_id=0x([0-9A-Fa-f]+)\\s+hw=0x([0-9A-Fa-f]+)\\s+modes=0x([0-9A-Fa-f]+)"));
        const QRegularExpression versionRe(QStringLiteral("\\[after-upgrade\\]\\s+app=([^\\s]+)\\s+bl=([^\\s]+)"));
        const auto moduleMatch = moduleRe.match(output);
        const auto versionMatch = versionRe.match(output);
        if (moduleMatch.hasMatch() && versionMatch.hasMatch()) {
            const QString module = moduleMatch.captured(1);
            const QString serialNumber = moduleMatch.captured(2);
            const QString moduleId = QStringLiteral("0x%1").arg(moduleMatch.captured(3).toUpper());
            const QString hardwareVersion = QStringLiteral("0x%1").arg(moduleMatch.captured(4).toUpper());
            const QString appVersion = versionMatch.captured(1);
            const QString bootloaderVersion = versionMatch.captured(2);
            const QString modes = QStringLiteral("0x%1").arg(moduleMatch.captured(5).toUpper());
            setDeviceDetails(QStringLiteral("module=%1 sn=%2 module_id=%3 hw=%4 app=%5 bl=%6 modes=%7")
                                 .arg(module, serialNumber, moduleId, hardwareVersion, appVersion, bootloaderVersion, modes),
                             module,
                             serialNumber,
                             moduleId,
                             hardwareVersion,
                             appVersion,
                             bootloaderVersion,
                             QStringLiteral("unknown"),
                             modes,
                             output);
        }

        finishSuccessfulUpgrade();
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError) {
        const QString error = process->errorString();
        process->deleteLater();
        fail(QStringLiteral("CAN upgrade helper error: %1").arg(error));
    });
    process->start(QStringLiteral("python"), args);
}

void FirmwareUpgradeManager::startUpgrade()
{
    if (m_running)
        return;
    m_stressMode = false;
    m_stressTotal = 0;
    m_stressCurrent = 0;
    setUpgradeSuccessCount(0);
    m_cancelled = false;
    beginUpgradeOnce();
}

void FirmwareUpgradeManager::startStressUpgrade(int count)
{
    if (m_running)
        return;

    m_stressMode = true;
    m_stressTotal = std::clamp(count, 1, 9999);
    m_stressCurrent = 0;
    setUpgradeSuccessCount(0);
    m_cancelled = false;
    startNextStressIteration();
}

void FirmwareUpgradeManager::beginUpgradeOnce()
{
    if (m_connectionManager
        && m_connectionManager->linkType() == QStringLiteral("CAN")
        && !m_connectionManager->isConnected()) {
        startUpgradeViaCanHelper();
        return;
    }
    if (!m_connectionManager || !m_connectionManager->isConnected()) {
        if (m_connectionManager
            && m_connectionManager->linkType() == QStringLiteral("USB")
            && m_stressMode
            && m_running) {
            waitRuntime(RUNTIME_APP, USB_RUNTIME_SWITCH_TIMEOUT_MS, [this]() {
                beginUpgradeOnce();
            });
            return;
        }
        fail(QStringLiteral("link is not connected"));
        return;
    }

    resetTransferState();
    if (!loadAndValidateFirmware())
        return;

    m_connectionManager->setErrorReportingSuppressed(true);
    setRunning(true);
    setProgress(0);
    setStatus(QStringLiteral("query device info"));

    queryDeviceInfo([this]() {
        m_runtime = m_deviceInfo.runtimeMode;
        if ((m_deviceInfo.supportedModes & m_requestedUpgradeMode) == 0) {
            fail(QStringLiteral("device does not support requested upgrade mode %1")
                     .arg(hexNumber(m_requestedUpgradeMode, 2)));
            return;
        }

        requestEnterUpgrade(m_requestedUpgradeMode, [this](const EnterUpgradeInfo &enter) {
            m_upgradeTiming = enter;
            if (enter.upgradeReady) {
                scheduleStartUpgradeSession();
                return;
            }

            reopenLinkAndEnterUpgrade(m_requestedUpgradeMode, enter);
        });
    });
}

void FirmwareUpgradeManager::startNextStressIteration()
{
    if (!m_stressMode || m_cancelled)
        return;
    if (m_stressCurrent >= m_stressTotal) {
        m_stressMode = false;
        if (m_connectionManager)
            m_connectionManager->setErrorReportingSuppressed(false);
        setProgress(100);
        setStatus(QStringLiteral("upgrade stress complete %1/%2").arg(m_stressPassed).arg(m_stressTotal));
        setRunning(false);
        return;
    }

    ++m_stressCurrent;
    setStatus(QStringLiteral("upgrade stress %1/%2").arg(m_stressCurrent).arg(m_stressTotal));
    beginUpgradeOnce();
}

void FirmwareUpgradeManager::finishSuccessfulUpgrade()
{
    setProgress(100);

    if (!m_stressMode) {
        if (m_connectionManager)
            m_connectionManager->setErrorReportingSuppressed(false);
        setUpgradeSuccessCount(1);
        setStatus(QStringLiteral("upgrade complete"));
        setRunning(false);
        return;
    }

    setUpgradeSuccessCount(m_stressCurrent);
    if (m_stressCurrent >= m_stressTotal) {
        m_stressMode = false;
        if (m_connectionManager)
            m_connectionManager->setErrorReportingSuppressed(false);
        setStatus(QStringLiteral("upgrade stress complete %1/%2").arg(m_stressPassed).arg(m_stressTotal));
        setRunning(false);
        return;
    }

    setStatus(QStringLiteral("upgrade stress %1/%2 complete").arg(m_stressCurrent).arg(m_stressTotal));
    QTimer::singleShot(500, this, [this]() {
        if (!m_running || !m_stressMode || m_cancelled)
            return;
        startNextStressIteration();
    });
}

void FirmwareUpgradeManager::cancel()
{
    m_cancelled = true;
    if (m_stressMode) {
        m_stressMode = false;
    }
    if (m_connectionManager && m_connectionManager->linkType() == QStringLiteral("USB")) {
        if (auto *usb = m_connectionManager->usbDriver())
            usb->setSuppressOpenErrors(false);
    }
    if (m_connectionManager)
        m_connectionManager->setErrorReportingSuppressed(false);
    clearPendingRequest();
    setRunning(false);
    setStatus(QStringLiteral("cancelled"));
}

void FirmwareUpgradeManager::onFrameReceived(const QVariantMap &frame)
{
    const int pushedCmd = parseNumberString(frame.value(QStringLiteral("cmd")).toString());
    if (pushedCmd == CMD_PUSH_UPGRADE_STATUS) {
        const QByteArray pushed = hexToBytes(frame.value(QStringLiteral("data")).toString());
        if (pushed.size() >= 5 && u32(pushed, 0) == 1)
            setProgress(std::clamp<int>(static_cast<quint8>(pushed[4]), 0, 100));
    }

    if (!m_running || m_pendingCmd == 0)
        return;

    const int seq = frame.value(QStringLiteral("seq")).toString().toInt();
    const int cmd = parseNumberString(frame.value(QStringLiteral("cmd")).toString());
    const QString type = frame.value(QStringLiteral("type")).toString();
    if (seq != m_pendingSeq || cmd != m_pendingCmd || type != QStringLiteral("ACK"))
        return;

    const QByteArray data = hexToBytes(frame.value(QStringLiteral("data")).toString());
    auto onSuccess = m_pendingSuccess;
    clearPendingRequest();

    if (onSuccess)
        onSuccess(data);
}

void FirmwareUpgradeManager::onRequestTimeout()
{
    if (!m_running || m_pendingCmd == 0)
        return;

    if (m_pendingRetriesLeft > 0) {
        --m_pendingRetriesLeft;
        sendPendingRequest();
        return;
    }

    const QString message = QStringLiteral("%1: no response").arg(m_pendingContext);
    auto onFailure = m_pendingFailure;
    clearPendingRequest();
    if (onFailure)
        onFailure(message);
    else
        fail(message);
}

void FirmwareUpgradeManager::setRunning(bool value)
{
    if (m_running == value)
        return;
    m_running = value;
    emit runningChanged();
}

void FirmwareUpgradeManager::setProgress(int value)
{
    value = std::clamp(value, 0, 100);
    if (m_progress == value)
        return;
    m_progress = value;
    emit progressChanged();
}

void FirmwareUpgradeManager::setUpgradeSuccessCount(int value)
{
    value = std::max(0, value);
    if (m_stressPassed == value)
        return;
    m_stressPassed = value;
    emit upgradeSuccessCountChanged();
}

void FirmwareUpgradeManager::setStatus(const QString &value)
{
    if (m_status == value)
        return;
    m_status = value;
    emit statusChanged();
}

void FirmwareUpgradeManager::setDeviceSummary(const QString &value)
{
    if (m_deviceSummary == value)
        return;
    m_deviceSummary = value;
    emit deviceInfoChanged();
}

void FirmwareUpgradeManager::setDeviceRawResponse(const QString &value)
{
    if (m_deviceRawResponse == value)
        return;
    m_deviceRawResponse = value;
    emit deviceInfoChanged();
}

void FirmwareUpgradeManager::setDeviceDetails(const QString &summary,
                                              const QString &module,
                                              const QString &serialNumber,
                                              const QString &moduleId,
                                              const QString &hardwareVersion,
                                              const QString &appVersion,
                                              const QString &bootloaderVersion,
                                              const QString &runtime,
                                              const QString &modes,
                                              const QString &rawResponse)
{
    if (m_deviceSummary == summary
        && m_deviceModule == module
        && m_deviceSerialNumber == serialNumber
        && m_deviceModuleId == moduleId
        && m_deviceHardwareVersion == hardwareVersion
        && m_deviceAppVersion == appVersion
        && m_deviceBootloaderVersion == bootloaderVersion
        && m_deviceRuntime == runtime
        && m_deviceModes == modes
        && m_deviceRawResponse == rawResponse) {
        return;
    }

    m_deviceSummary = summary;
    m_deviceModule = module;
    m_deviceSerialNumber = serialNumber;
    m_deviceModuleId = moduleId;
    m_deviceHardwareVersion = hardwareVersion;
    m_deviceAppVersion = appVersion;
    m_deviceBootloaderVersion = bootloaderVersion;
    m_deviceRuntime = runtime;
    m_deviceModes = modes;
    m_deviceRawResponse = rawResponse;
    emit deviceInfoChanged();
}

void FirmwareUpgradeManager::fail(const QString &message)
{
    const bool wasStress = m_stressMode;
    const int failedIteration = m_stressCurrent;
    const int totalIterations = m_stressTotal;
    if (m_stressMode) {
        m_stressMode = false;
    }
    if (m_connectionManager && m_connectionManager->linkType() == QStringLiteral("USB")) {
        if (auto *usb = m_connectionManager->usbDriver())
            usb->setSuppressOpenErrors(false);
    }
    if (m_connectionManager)
        m_connectionManager->setErrorReportingSuppressed(false);
    clearPendingRequest();
    setRunning(false);
    if (wasStress) {
        setStatus(QStringLiteral("error: stress %1/%2 failed: %3")
                      .arg(failedIteration)
                      .arg(totalIterations)
                      .arg(message));
    } else {
        setStatus(QStringLiteral("error: %1").arg(message));
    }
    emit errorOccurred(message);
}

void FirmwareUpgradeManager::resetTransferState()
{
    m_cancelled = false;
    clearPendingRequest();
    m_nextSeq = 1;
    m_runtime = -1;
    m_effectivePacketSize = 65535;
    m_transferOffset = 0;
    m_packetIndex = 0;
    m_upgradeTiming = {};
    m_waitRuntimeExpected = -1;
    m_waitRuntimeContinuation = {};
}

bool FirmwareUpgradeManager::loadAndValidateFirmware()
{
    QFile file(m_firmwarePath);
    if (!file.open(QIODevice::ReadOnly)) {
        fail(QStringLiteral("cannot open firmware: %1").arg(m_firmwarePath));
        return false;
    }

    m_firmware = file.readAll();
    QString error;
    if (!validateFirmware(m_firmware, &error)) {
        fail(error);
        return false;
    }

    m_firmwareCrc = crc32(m_firmware);
    m_requestedUpgradeMode = upgradeModeFromFirmware(m_firmware);
    m_targetImageVersion = firmwareImageVersion(m_firmware);
    return true;
}

bool FirmwareUpgradeManager::validateFirmware(const QByteArray &firmware, QString *errorMessage) const
{
    if (firmware.size() < MIN_FIRMWARE_HEADER_SIZE) {
        if (errorMessage) *errorMessage = QStringLiteral("firmware too small");
        return false;
    }

    const quint32 magic = u32(firmware, 0);
    const quint16 headerVersion = u16(firmware, 4);
    const quint16 headerSize = u16(firmware, 6);
    if (magic != FIRMWARE_MAGIC) {
        if (errorMessage) *errorMessage = QStringLiteral("bad firmware magic: %1").arg(hexNumber(magic, 8));
        return false;
    }
    if (headerVersion != 1) {
        if (errorMessage) *errorMessage = QStringLiteral("unsupported firmware header version: %1").arg(headerVersion);
        return false;
    }
    if (headerSize < MIN_FIRMWARE_HEADER_SIZE || headerSize > firmware.size()) {
        if (errorMessage) *errorMessage = QStringLiteral("bad firmware header size: %1").arg(headerSize);
        return false;
    }

    const quint32 imageSize = u32(firmware, 24);
    if (headerSize + imageSize != static_cast<quint32>(firmware.size())) {
        if (errorMessage) *errorMessage = QStringLiteral("firmware size does not match header image_size");
        return false;
    }
    return true;
}

void FirmwareUpgradeManager::request(quint16 cmd,
                                     const QByteArray &data,
                                     const QString &context,
                                     ResponseHandler onSuccess,
                                     FailureHandler onFailure)
{
    if (m_cancelled)
        return;

    clearPendingRequest();
    m_pendingCmd = cmd;
    m_pendingSeq = m_nextSeq++;
    if (m_nextSeq == 0)
        m_nextSeq = 1;
    m_pendingFrame = buildFrame(cmd, m_pendingSeq, data);
    m_pendingContext = context;
    m_pendingRetriesLeft = m_retries;
    m_pendingReconnectActive = false;
    m_pendingSuccess = std::move(onSuccess);
    m_pendingFailure = std::move(onFailure);
    sendPendingRequest();
}

void FirmwareUpgradeManager::sendPendingRequest()
{
    if (!m_connectionManager || !m_connectionManager->isConnected()) {
        if (m_connectionManager
            && m_connectionManager->linkType() == QStringLiteral("USB")
            && m_running
            && m_pendingCmd != 0) {
            if (!m_pendingReconnectActive) {
                m_pendingReconnectActive = true;
                m_pendingReconnectTimer.restart();
            }
            if (m_pendingReconnectTimer.elapsed() < USB_RUNTIME_SWITCH_TIMEOUT_MS) {
                setStatus(QStringLiteral("wait USB reconnect"));
                if (auto *usb = m_connectionManager->usbDriver())
                    usb->setSuppressOpenErrors(true);
                m_connectionManager->openConnection(false);
                QTimer::singleShot(RUNTIME_POLL_INTERVAL_MS, this, [this]() {
                    if (!m_running || m_cancelled || m_pendingCmd == 0)
                        return;
                    sendPendingRequest();
                });
                return;
            }
        }
        const QString message = QStringLiteral("link disconnected");
        auto onFailure = m_pendingFailure;
        clearPendingRequest();
        if (onFailure)
            onFailure(message);
        else
            fail(message);
        return;
    }
    m_pendingReconnectActive = false;
    m_connectionManager->sendData(m_pendingFrame);
    m_requestTimer.start(m_timeoutMs);
}

void FirmwareUpgradeManager::clearPendingRequest()
{
    m_requestTimer.stop();
    m_pendingCmd = 0;
    m_pendingSeq = 0;
    m_pendingFrame.clear();
    m_pendingContext.clear();
    m_pendingRetriesLeft = 0;
    m_pendingReconnectActive = false;
    m_pendingSuccess = {};
    m_pendingFailure = {};
}

void FirmwareUpgradeManager::queryDeviceInfo(Continuation next, FailureHandler onFailure)
{
    setStatus(QStringLiteral("get device info"));
    request(CMD_GET_DEVICE_INFO, {}, QStringLiteral("get device info"),
            [this, next = std::move(next), onFailure](const QByteArray &data) mutable {
        setDeviceRawResponse(QStringLiteral("payload: %1").arg(bytesToHex(data)));
        if (data.isEmpty() || static_cast<quint8>(data[0]) != 0) {
            const QString message = QStringLiteral("get device info: ret=%1")
                .arg(data.isEmpty() ? QStringLiteral("empty") : retName(static_cast<quint8>(data[0])));
            if (onFailure)
                onFailure(message);
            else
                fail(message);
            return;
        }

        if (data.size() < 51) {
            const QString message = QStringLiteral("get device info: short response");
            if (onFailure)
                onFailure(message);
            else
                fail(message);
            return;
        }

        m_deviceInfo = parseDeviceInfoPayload(data);
        const QString module = m_deviceInfo.moduleName;
        const QString serialNumber = m_deviceInfo.serialNumber;
        const QString moduleId = hexNumber(m_deviceInfo.moduleId, 8);
        const QString hardwareVersion = hexNumber(m_deviceInfo.hardwareVersion, 8);
        const QString appVersion = formatVersion(m_deviceInfo.appVersion);
        const QString bootloaderVersion = formatVersion(m_deviceInfo.bootloaderVersion);
        const QString runtime = runtimeName(m_deviceInfo.runtimeMode);
        const QString modes = hexNumber(m_deviceInfo.supportedModes, 2);
        setDeviceDetails(QStringLiteral("module=%1 sn=%2 module_id=%3 hw=%4 app=%5 bl=%6 runtime=%7 modes=%8")
                             .arg(module, serialNumber, moduleId, hardwareVersion, appVersion, bootloaderVersion, runtime, modes),
                         module,
                         serialNumber,
                         moduleId,
                         hardwareVersion,
                         appVersion,
                         bootloaderVersion,
                         runtime,
                         modes,
                         QStringLiteral("payload: %1").arg(bytesToHex(data)));

        if (next)
            next();
    }, [this, onFailure](const QString &message) {
        if (onFailure)
            onFailure(message);
        else
            fail(message);
    });
}

void FirmwareUpgradeManager::queryRuntime(std::function<void(int)> next, FailureHandler onFailure)
{
    if (!m_connectionManager || !m_connectionManager->isConnected()) {
        if (onFailure)
            onFailure(QStringLiteral("link disconnected"));
        else
            fail(QStringLiteral("link disconnected"));
        return;
    }

    queryDeviceInfo([this, next = std::move(next)]() mutable {
        const int runtime = m_deviceInfo.runtimeMode;
        if (next)
            next(runtime);
    }, std::move(onFailure));
}

void FirmwareUpgradeManager::requestEnterUpgrade(quint8 requestedMode, std::function<void(const EnterUpgradeInfo &)> next)
{
    setStatus(QStringLiteral("request enter upgrade"));
    QByteArray data;
    data.append(static_cast<char>(requestedMode));
    request(CMD_REQUEST_ENTER_UPGRADE, data, QStringLiteral("request enter upgrade"),
            [this, next = std::move(next)](const QByteArray &response) mutable {
        if (response.size() < 13 || static_cast<quint8>(response[0]) != 0) {
            fail(QStringLiteral("request enter upgrade: ret=%1")
                     .arg(response.isEmpty() ? QStringLiteral("empty") : retName(static_cast<quint8>(response[0]))));
            return;
        }
        if (next)
            next(parseEnterUpgradePayload(response));
    });
}

void FirmwareUpgradeManager::reboot(int mode, Continuation next)
{
    Q_UNUSED(mode);
    const int rebootDelayMs = m_upgradeTiming.rebootDeviceDelayMs > 0
        ? m_upgradeTiming.rebootDeviceDelayMs
        : DEFAULT_REBOOT_DELAY_MS;
    QByteArray data;
    data.append(le32(static_cast<quint32>(rebootDelayMs)));
    data.append(static_cast<char>(1));

    request(CMD_REBOOT, data, QStringLiteral("reboot"),
            [this, rebootDelayMs, next = std::move(next)](const QByteArray &response) mutable {
        if (response.isEmpty() || static_cast<quint8>(response[0]) != 0) {
            fail(QStringLiteral("reboot: ret=%1").arg(response.isEmpty() ? QStringLiteral("empty") : retName(static_cast<quint8>(response[0]))));
            return;
        }
        setStatus(QStringLiteral("rebooting"));
        if (m_connectionManager && m_connectionManager->linkType() == QStringLiteral("USB"))
            m_connectionManager->closeConnection();
        if (next)
            QTimer::singleShot(rebootDelayMs, this, std::move(next));
    });
}

void FirmwareUpgradeManager::waitRuntime(int expectedRuntime, int timeoutMs, Continuation next)
{
    m_waitRuntimeExpected = expectedRuntime;
    m_waitRuntimeTimeoutMs = timeoutMs;
    m_waitRuntimeTimer.restart();
    m_waitRuntimeContinuation = std::move(next);
    setStatus(QStringLiteral("wait runtime=%1").arg(runtimeName(expectedRuntime)));
    QTimer::singleShot(0, this, &FirmwareUpgradeManager::pollRuntime);
}

void FirmwareUpgradeManager::pollRuntime()
{
    if (!m_running || m_cancelled || m_waitRuntimeExpected < 0)
        return;

    const auto retryLater = [this]() {
        if (!m_running || m_cancelled || m_waitRuntimeExpected < 0)
            return;
        if (m_waitRuntimeTimer.elapsed() >= m_waitRuntimeTimeoutMs) {
            fail(QStringLiteral("runtime mode did not switch"));
            return;
        }
        QTimer::singleShot(RUNTIME_POLL_INTERVAL_MS, this, &FirmwareUpgradeManager::pollRuntime);
    };

    if (!m_connectionManager || !m_connectionManager->isConnected()) {
        if (m_connectionManager && m_connectionManager->linkType() == QStringLiteral("USB")) {
            setStatus(QStringLiteral("wait runtime=%1 reconnect USB").arg(runtimeName(m_waitRuntimeExpected)));
            if (auto *usb = m_connectionManager->usbDriver())
                usb->setSuppressOpenErrors(true);
            m_connectionManager->openConnection(false);
            retryLater();
            return;
        }
        retryLater();
        return;
    }

    queryRuntime([this](int runtime) {
        if (runtime == m_waitRuntimeExpected) {
            if (m_connectionManager && m_connectionManager->linkType() == QStringLiteral("USB")) {
                if (auto *usb = m_connectionManager->usbDriver())
                    usb->setSuppressOpenErrors(false);
            }
            auto cont = std::move(m_waitRuntimeContinuation);
            m_waitRuntimeExpected = -1;
            if (cont)
                cont();
            return;
        }

        if (m_waitRuntimeTimer.elapsed() >= m_waitRuntimeTimeoutMs) {
            fail(QStringLiteral("runtime mode did not switch"));
            return;
        }
        QTimer::singleShot(RUNTIME_POLL_INTERVAL_MS, this, &FirmwareUpgradeManager::pollRuntime);
    }, [this](const QString &) {
        if (m_connectionManager && m_connectionManager->linkType() == QStringLiteral("USB"))
            m_connectionManager->closeConnection();
        if (m_waitRuntimeTimer.elapsed() >= m_waitRuntimeTimeoutMs) {
            fail(QStringLiteral("runtime mode did not switch"));
            return;
        }
        QTimer::singleShot(RUNTIME_POLL_INTERVAL_MS, this, &FirmwareUpgradeManager::pollRuntime);
    });
}

void FirmwareUpgradeManager::reopenLinkAndEnterUpgrade(quint8 requestedMode, const EnterUpgradeInfo &enter)
{
    const int delayMs = enter.enterUpgradeDelayMs > 0 ? enter.enterUpgradeDelayMs
                                                      : DEFAULT_ENTER_UPGRADE_DELAY_MS;
    setStatus(QStringLiteral("wait enter upgrade %1 ms").arg(delayMs));
    clearPendingRequest();
    if (m_connectionManager)
        m_connectionManager->closeConnection();

    QTimer::singleShot(delayMs, this, [this, requestedMode]() {
        if (!m_running || m_cancelled)
            return;
        if (!m_connectionManager) {
            fail(QStringLiteral("link is not connected"));
            return;
        }
        const int timeoutMs = m_connectionManager
                && m_connectionManager->linkType() == QStringLiteral("USB")
            ? USB_RUNTIME_SWITCH_TIMEOUT_MS
            : DEFAULT_RUNTIME_SWITCH_TIMEOUT_MS;
        waitRuntime(RUNTIME_BOOTLOADER, timeoutMs, [this, requestedMode]() {
            requestEnterUpgrade(requestedMode, [this](const EnterUpgradeInfo &updated) {
                m_upgradeTiming = updated;
                if (!updated.upgradeReady) {
                    fail(QStringLiteral("device did not enter upgrade state"));
                    return;
                }
                scheduleStartUpgradeSession();
            });
        });
    });
}

void FirmwareUpgradeManager::scheduleStartUpgradeSession()
{
    const int delayMs = m_upgradeTiming.startUpgradeDelayMs;
    if (delayMs <= 0) {
        startUpgradeSession();
        return;
    }

    QTimer::singleShot(delayMs, this, [this]() {
        if (!m_running || m_cancelled)
            return;
        startUpgradeSession();
    });
}

void FirmwareUpgradeManager::startUpgradeSession()
{
    QByteArray data;
    data.append(le32(static_cast<quint32>(m_firmware.size())));
    data.append(le32(m_firmwareCrc));

    setStatus(QStringLiteral("start upgrade"));
    request(CMD_START_UPGRADE, data, QStringLiteral("start upgrade"),
            [this](const QByteArray &response) {
        if (response.size() < 5 || static_cast<quint8>(response[0]) != 0) {
            fail(QStringLiteral("start upgrade: ret=%1").arg(response.isEmpty() ? QStringLiteral("empty") : retName(static_cast<quint8>(response[0]))));
            return;
        }

        const int minSize = u16(response, 1);
        const int maxSize = u16(response, 3);
        m_effectivePacketSize = choosePacketSize(minSize, maxSize);
        if (m_effectivePacketSize <= 0) {
            fail(QStringLiteral("start upgrade: invalid packet size range %1-%2").arg(minSize).arg(maxSize));
            return;
        }
        m_transferOffset = 0;
        m_packetIndex = 0;
        setStatus(QStringLiteral("transfer packet_size=%1 range=%2-%3").arg(m_effectivePacketSize).arg(minSize).arg(maxSize));
        transferNextPacket();
    });
}

void FirmwareUpgradeManager::transferNextPacket()
{
    if (m_cancelled)
        return;

    if (m_transferOffset >= m_firmware.size()) {
        finishUpgrade();
        return;
    }

    const QByteArray chunk = m_firmware.mid(m_transferOffset, m_effectivePacketSize);
    QByteArray data;
    data.append(le32(m_packetIndex));
    data.append(le16(static_cast<quint16>(chunk.size())));
    data.append(chunk);

    setStatus(QStringLiteral("transfer %1/%2").arg(m_transferOffset).arg(m_firmware.size()));
    request(CMD_TRANSFER_PACKET, data, QStringLiteral("transfer packet"),
            [this, chunkSize = chunk.size(), packetIndex = m_packetIndex](const QByteArray &response) {
        if (response.size() < 5 || static_cast<quint8>(response[0]) != 0) {
            fail(QStringLiteral("transfer packet %1: ret=%2").arg(packetIndex).arg(response.isEmpty() ? QStringLiteral("empty") : retName(static_cast<quint8>(response[0]))));
            return;
        }
        if (u32(response, 1) != packetIndex) {
            fail(QStringLiteral("transfer packet %1: bad ack").arg(packetIndex));
            return;
        }

        m_transferOffset += chunkSize;
        ++m_packetIndex;
        const qint64 firmwareSize = std::max<qint64>(1, static_cast<qint64>(m_firmware.size()));
        setProgress(static_cast<int>((static_cast<qint64>(m_transferOffset) * 100) / firmwareSize));
        const int delayMs = m_upgradeTiming.transferPacketDelayMs;
        if (delayMs > 0) {
            QTimer::singleShot(delayMs, this, &FirmwareUpgradeManager::transferNextPacket);
        } else {
            QTimer::singleShot(0, this, &FirmwareUpgradeManager::transferNextPacket);
        }
    });
}

void FirmwareUpgradeManager::finishUpgrade()
{
    setStatus(QStringLiteral("finish upgrade"));
    request(CMD_FINISH_UPGRADE, {}, QStringLiteral("finish upgrade"),
            [this](const QByteArray &response) {
        if (response.isEmpty() || static_cast<quint8>(response[0]) != 0) {
            fail(QStringLiteral("finish upgrade: ret=%1").arg(response.isEmpty() ? QStringLiteral("empty") : retName(static_cast<quint8>(response[0]))));
            return;
        }
        const int delayMs = m_upgradeTiming.finishUpgradeDelayMs;
        if (delayMs > 0) {
            QTimer::singleShot(delayMs, this, &FirmwareUpgradeManager::rebootToApp);
        } else {
            rebootToApp();
        }
    });
}

void FirmwareUpgradeManager::rebootToApp()
{
    setStatus(QStringLiteral("reboot to app"));
    reboot(REBOOT_NORMAL, [this]() {
        const int timeoutMs = m_connectionManager
                && m_connectionManager->linkType() == QStringLiteral("USB")
            ? USB_RUNTIME_SWITCH_TIMEOUT_MS
            : DEFAULT_RUNTIME_SWITCH_TIMEOUT_MS;
        waitRuntime(RUNTIME_APP, timeoutMs, [this]() {
            queryDeviceInfo([this]() {
                completeUpgrade();
            });
        });
    });
}

void FirmwareUpgradeManager::completeUpgrade()
{
    if (m_connectionManager && m_connectionManager->linkType() == QStringLiteral("USB")) {
        if (auto *usb = m_connectionManager->usbDriver())
            usb->setSuppressOpenErrors(false);
    }
    if (m_targetImageVersion != MISSING_VERSION && m_deviceInfo.appVersion != m_targetImageVersion) {
        fail(QStringLiteral("version mismatch: target=%1 actual=%2")
                 .arg(formatVersion(m_targetImageVersion))
                 .arg(formatVersion(m_deviceInfo.appVersion)));
        return;
    }
    finishSuccessfulUpgrade();
}

QByteArray FirmwareUpgradeManager::buildFrame(quint16 cmd, quint16 seq, const QByteArray &data) const
{
    QByteArray payload;
    payload.append(le16(seq));
    payload.append(le16(cmd));
    payload.append(le16(FLAG_ACK_REQUIRED));
    payload.append(data);

    QByteArray frame;
    frame.append(static_cast<char>(SOP));
    frame.append(static_cast<char>(VERSION));
    frame.append(le16(static_cast<quint16>(m_srcAddr)));
    frame.append(le16(static_cast<quint16>(m_dstAddr)));
    frame.append(le16(static_cast<quint16>(payload.size())));
    frame.append(static_cast<char>(NEXT_HEADER_GENERIC));
    frame.append(static_cast<char>(crc8(frame)));
    frame.append(payload);
    frame.append(le16(crc16(payload)));
    return frame;
}

QByteArray FirmwareUpgradeManager::hexToBytes(QString hex)
{
    hex.remove(QStringLiteral("0x"), Qt::CaseInsensitive);
    hex.remove(QLatin1Char(' '));
    hex.remove(QLatin1Char('\n'));
    hex.remove(QLatin1Char('\r'));
    hex.remove(QLatin1Char('\t'));
    return QByteArray::fromHex(hex.toLatin1());
}

QString FirmwareUpgradeManager::bytesToHex(const QByteArray &data)
{
    return QString::fromLatin1(data.toHex(' ')).toUpper();
}

QByteArray FirmwareUpgradeManager::le16(quint16 value)
{
    QByteArray out;
    out.append(static_cast<char>(value & 0xFF));
    out.append(static_cast<char>((value >> 8) & 0xFF));
    return out;
}

QByteArray FirmwareUpgradeManager::le32(quint32 value)
{
    QByteArray out;
    out.append(static_cast<char>(value & 0xFF));
    out.append(static_cast<char>((value >> 8) & 0xFF));
    out.append(static_cast<char>((value >> 16) & 0xFF));
    out.append(static_cast<char>((value >> 24) & 0xFF));
    return out;
}

quint16 FirmwareUpgradeManager::u16(const QByteArray &data, int offset)
{
    return static_cast<quint8>(data[offset])
         | (static_cast<quint8>(data[offset + 1]) << 8);
}

quint32 FirmwareUpgradeManager::u32(const QByteArray &data, int offset)
{
    return static_cast<quint32>(static_cast<quint8>(data[offset]))
         | (static_cast<quint32>(static_cast<quint8>(data[offset + 1])) << 8)
         | (static_cast<quint32>(static_cast<quint8>(data[offset + 2])) << 16)
         | (static_cast<quint32>(static_cast<quint8>(data[offset + 3])) << 24);
}

quint8 FirmwareUpgradeManager::crc8(const QByteArray &data)
{
    quint8 crc = 0;
    for (char ch : data) {
        crc ^= static_cast<quint8>(ch);
        for (int i = 0; i < 8; ++i)
            crc = (crc & 1) ? static_cast<quint8>((crc >> 1) ^ 0x8C)
                            : static_cast<quint8>(crc >> 1);
    }
    return crc;
}

quint16 FirmwareUpgradeManager::crc16(const QByteArray &data)
{
    quint16 crc = 0xFFFF;
    for (char ch : data) {
        crc ^= static_cast<quint16>(static_cast<quint8>(ch) << 8);
        for (int i = 0; i < 8; ++i)
            crc = (crc & 0x8000) ? static_cast<quint16>((crc << 1) ^ 0x1021)
                                 : static_cast<quint16>(crc << 1);
    }
    return crc;
}

quint32 FirmwareUpgradeManager::crc32(const QByteArray &data)
{
    quint32 crc = 0xFFFFFFFF;
    for (char ch : data) {
        crc ^= static_cast<quint8>(ch);
        for (int i = 0; i < 8; ++i)
            crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320u : crc >> 1;
    }
    return ~crc;
}

QString FirmwareUpgradeManager::formatVersion(quint32 value)
{
    if (value == MISSING_VERSION)
        return QStringLiteral("missing");
    return QStringLiteral("%1.%2.%3.%4")
        .arg((value >> 24) & 0xFF)
        .arg((value >> 16) & 0xFF)
        .arg((value >> 8) & 0xFF)
        .arg(value & 0xFF);
}

QString FirmwareUpgradeManager::retName(quint8 ret)
{
    switch (ret) {
    case 0x00: return QStringLiteral("ok");
    case 0x01: return QStringLiteral("unsupported_command");
    case 0x02: return QStringLiteral("invalid_argument");
    case 0x03: return QStringLiteral("invalid_state");
    case 0x04: return QStringLiteral("busy");
    case 0x05: return QStringLiteral("timeout");
    case 0x06: return QStringLiteral("no_memory");
    case 0x07: return QStringLiteral("storage_full");
    case 0x08: return QStringLiteral("hardware_error");
    case 0x09: return QStringLiteral("permission_denied");
    case 0x0A: return QStringLiteral("checksum_error");
    case 0x0B: return QStringLiteral("not_found");
    case 0x0C: return QStringLiteral("already_exists");
    case 0x0D: return QStringLiteral("unsupported_version");
    case 0x0E: return QStringLiteral("cancelled");
    case 0x0F: return QStringLiteral("format_error");
    case 0x7F: return QStringLiteral("common_unknown_error");
    case 0x80: return QStringLiteral("unsupported_upgrade_mode");
    case 0x81: return QStringLiteral("upgrade_mode_conflict");
    case 0x82: return QStringLiteral("firmware_too_large");
    case 0x83: return QStringLiteral("packet_index_error");
    case 0x84: return QStringLiteral("packet_size_error");
    case 0x85: return QStringLiteral("firmware_crc_error");
    case 0x86: return QStringLiteral("firmware_header_error");
    case 0x87: return QStringLiteral("firmware_signature_error");
    case 0x88: return QStringLiteral("firmware_hash_error");
    case 0x89: return QStringLiteral("firmware_type_mismatch");
    case 0x8A: return QStringLiteral("module_or_hw_mismatch");
    case 0x8B: return QStringLiteral("flash_erase_write_error");
    case 0xFF: return QStringLiteral("upgrade_unknown_error");
    default: return QStringLiteral("unknown_%1").arg(hexNumber(ret, 2));
    }
}

int FirmwareUpgradeManager::parseNumberString(const QString &value)
{
    bool ok = false;
    QString trimmed = value.trimmed();
    int base = 10;
    if (trimmed.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        trimmed = trimmed.mid(2);
        base = 16;
    }
    int result = trimmed.toInt(&ok, base);
    return ok ? result : 0;
}

FirmwareUpgradeManager::DeviceInfo FirmwareUpgradeManager::parseDeviceInfoPayload(const QByteArray &data)
{
    DeviceInfo info;
    info.appVersion = u32(data, 1);
    info.bootloaderVersion = u32(data, 5);
    info.moduleId = u32(data, 9);
    info.hardwareVersion = u32(data, 13);
    info.supportedModes = static_cast<quint8>(data[17]);
    info.runtimeMode = static_cast<quint8>(data[18]);
    info.serialNumber = QString::fromLatin1(data.mid(19, 16).split('\0').value(0));
    info.moduleName = QString::fromLatin1(data.mid(35, 16).split('\0').value(0));
    info.valid = true;
    return info;
}

FirmwareUpgradeManager::EnterUpgradeInfo FirmwareUpgradeManager::parseEnterUpgradePayload(const QByteArray &data)
{
    EnterUpgradeInfo info;
    info.upgradeReady = static_cast<quint8>(data[1]) != 0;
    info.pushStatusSupported = static_cast<quint8>(data[2]) != 0;
    info.enterUpgradeDelayMs = u16(data, 3);
    info.startUpgradeDelayMs = u16(data, 5);
    info.transferPacketDelayMs = u16(data, 7);
    info.finishUpgradeDelayMs = u16(data, 9);
    info.rebootDeviceDelayMs = u16(data, 11);
    return info;
}

bool FirmwareUpgradeManager::isPowerOfTwo(int value)
{
    return value > 0 && (value & (value - 1)) == 0;
}

int FirmwareUpgradeManager::choosePacketSize(int minSize, int maxSize)
{
    if (minSize > maxSize || !isPowerOfTwo(minSize) || !isPowerOfTwo(maxSize))
        return -1;
    return maxSize;
}

quint8 FirmwareUpgradeManager::upgradeModeFromFirmware(const QByteArray &firmware)
{
    const quint32 type = u32(firmware, 20);
    if (type == FIRMWARE_TYPE_BOOTLOADER)
        return UPGRADE_MODE_BOOTLOADER;
    return UPGRADE_MODE_APP;
}

quint32 FirmwareUpgradeManager::firmwareImageVersion(const QByteArray &firmware)
{
    return u32(firmware, 28);
}
