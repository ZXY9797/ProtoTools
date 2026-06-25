#include "IO/Drivers/UsbBulk.h"

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QUuid>

#include <algorithm>
#include <cwchar>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <setupapi.h>
#include <winusb.h>
#endif

#if defined(HAS_LIBUSB)
#include <libusb-1.0/libusb.h>
#endif

namespace {

int clampByte(int value)
{
    return std::max(0, std::min(0xFF, value));
}

int clampPositive(int value, int fallback, int maxValue)
{
    if (value <= 0)
        return fallback;
    return std::min(value, maxValue);
}

QString hex4(int value)
{
    return QStringLiteral("0x%1").arg(QStringLiteral("%1").arg(value & 0xFFFF, 4, 16, QLatin1Char('0')).toUpper());
}

QString hex2(int value)
{
    return QStringLiteral("0x%1").arg(QStringLiteral("%1").arg(clampByte(value), 2, 16, QLatin1Char('0')).toUpper());
}

#ifdef Q_OS_WIN
constexpr GUID GUID_DEVINTERFACE_USB_DEVICE_KPTOOLS = {
    0xA5DCBF10, 0x6530, 0x11D2,
    {0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED}
};

bool isTransientUsbDisconnectError(DWORD error)
{
    switch (error) {
    case ERROR_BAD_COMMAND:
    case ERROR_GEN_FAILURE:
    case ERROR_INVALID_HANDLE:
    case ERROR_NOT_READY:
    case ERROR_OPERATION_ABORTED:
    case ERROR_DEVICE_NOT_CONNECTED:
        return true;
    default:
        return false;
    }
}

bool guidFromString(const QString &text, GUID *guid)
{
    const QUuid uuid(text.trimmed());
    if (uuid.isNull())
        return false;
    guid->Data1 = uuid.data1;
    guid->Data2 = uuid.data2;
    guid->Data3 = uuid.data3;
    for (int i = 0; i < 8; ++i)
        guid->Data4[i] = uuid.data4[i];
    return true;
}

QStringList registryDeviceInterfaceGuids(const QString &instanceId)
{
    QStringList guids;
    const QString keyName = QStringLiteral("SYSTEM\\CurrentControlSet\\Enum\\%1\\Device Parameters").arg(instanceId);

    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, reinterpret_cast<LPCWSTR>(keyName.utf16()),
                      0, KEY_READ, &key) != ERROR_SUCCESS) {
        return guids;
    }

    const auto readValue = [&](const wchar_t *valueName) {
        DWORD type = 0;
        DWORD byteCount = 0;
        if (RegQueryValueExW(key, valueName, nullptr, &type, nullptr, &byteCount) != ERROR_SUCCESS || byteCount == 0)
            return;

        QByteArray storage(static_cast<int>(byteCount + sizeof(wchar_t)), 0);
        if (RegQueryValueExW(key, valueName, nullptr, &type,
                             reinterpret_cast<LPBYTE>(storage.data()), &byteCount) != ERROR_SUCCESS) {
            return;
        }

        const auto *text = reinterpret_cast<const wchar_t *>(storage.constData());
        if (type == REG_MULTI_SZ) {
            for (const wchar_t *p = text; p && *p; p += std::wcslen(p) + 1) {
                const QString guid = QString::fromWCharArray(p).trimmed();
                if (!guid.isEmpty() && !guids.contains(guid, Qt::CaseInsensitive))
                    guids.append(guid);
            }
        } else if (type == REG_SZ || type == REG_EXPAND_SZ) {
            const QString guid = QString::fromWCharArray(text).trimmed();
            if (!guid.isEmpty() && !guids.contains(guid, Qt::CaseInsensitive))
                guids.append(guid);
        }
    };

    readValue(L"DeviceInterfaceGUID");
    readValue(L"DeviceInterfaceGUIDs");
    RegCloseKey(key);
    return guids;
}

bool usbPathMatches(const QString &path, const QString &vidNeedle,
                    const QString &pidNeedle, const QString &miNeedle)
{
    const QString lowerPath = path.toLower();
    if (!lowerPath.contains(vidNeedle) || !lowerPath.contains(pidNeedle))
        return false;
    return !lowerPath.contains(QStringLiteral("mi_")) || lowerPath.contains(miNeedle);
}
#endif

} // namespace

namespace IO {
namespace Drivers {

UsbBulk::UsbBulk(QObject *parent)
    : HAL_Driver(parent)
{
    connect(&m_readTimer, &QTimer::timeout, this, &UsbBulk::readPending);
}

UsbBulk::~UsbBulk()
{
    close();
}

bool UsbBulk::open()
{
    if (isOpen())
        return true;
    if (!configurationOk()) {
        emit errorOccurred(QStringLiteral("USB VID/PID 未配置"));
        return false;
    }
    if (!openPlatform())
        return false;

    m_open = true;
    m_readTimer.start(std::max(1, m_readIntervalMs));
    emit connectionChanged();
    return true;
}

void UsbBulk::close()
{
    if (!m_open && !m_deviceHandle && !m_usbHandle)
        return;
    m_readTimer.stop();
    closePlatform();
    m_open = false;
    emit connectionChanged();
}

bool UsbBulk::isOpen() const
{
    return m_open;
}

bool UsbBulk::configurationOk() const noexcept
{
    return m_vendorId > 0 && m_productId > 0
        && m_bulkInEndpoint > 0 && m_bulkOutEndpoint > 0;
}

qint64 UsbBulk::write(const QByteArray &data)
{
    if (!isOpen() || data.isEmpty())
        return -1;
    return writePlatform(data);
}

void UsbBulk::setProperty(const QString &key, const QVariant &value)
{
    if (key == QLatin1String("vendorId")) setVendorId(value.toInt());
    else if (key == QLatin1String("productId")) setProductId(value.toInt());
    else if (key == QLatin1String("interfaceNumber")) setInterfaceNumber(value.toInt());
    else if (key == QLatin1String("bulkInEndpoint")) setBulkInEndpoint(value.toInt());
    else if (key == QLatin1String("bulkOutEndpoint")) setBulkOutEndpoint(value.toInt());
    else if (key == QLatin1String("readPacketSize")) setReadPacketSize(value.toInt());
    else if (key == QLatin1String("timeoutMs")) setTimeoutMs(value.toInt());
    else if (key == QLatin1String("readIntervalMs")) setReadIntervalMs(value.toInt());
}

QVariant UsbBulk::getProperty(const QString &key) const
{
    if (key == QLatin1String("vendorId")) return m_vendorId;
    if (key == QLatin1String("productId")) return m_productId;
    if (key == QLatin1String("interfaceNumber")) return m_interfaceNumber;
    if (key == QLatin1String("bulkInEndpoint")) return m_bulkInEndpoint;
    if (key == QLatin1String("bulkOutEndpoint")) return m_bulkOutEndpoint;
    if (key == QLatin1String("readPacketSize")) return m_readPacketSize;
    if (key == QLatin1String("timeoutMs")) return m_timeoutMs;
    if (key == QLatin1String("readIntervalMs")) return m_readIntervalMs;
    return {};
}

QList<IO::DriverProperty> UsbBulk::driverProperties() const
{
    return {
        {.key = "vendorId", .label = "VID", .description = "USB Vendor ID",
         .type = IO::DriverProperty::HexText, .value = hex4(m_vendorId)},
        {.key = "productId", .label = "PID", .description = "USB Product ID",
         .type = IO::DriverProperty::HexText, .value = hex4(m_productId)},
        {.key = "interfaceNumber", .label = "Interface", .description = "USB interface number",
         .type = IO::DriverProperty::IntField, .value = m_interfaceNumber, .min = 0, .max = 32},
        {.key = "bulkInEndpoint", .label = "Bulk IN", .description = "Bulk IN endpoint address",
         .type = IO::DriverProperty::HexText, .value = hex2(m_bulkInEndpoint)},
        {.key = "bulkOutEndpoint", .label = "Bulk OUT", .description = "Bulk OUT endpoint address",
         .type = IO::DriverProperty::HexText, .value = hex2(m_bulkOutEndpoint)},
        {.key = "readPacketSize", .label = "Read Len", .description = "Maximum bulk read size",
         .type = IO::DriverProperty::IntField, .value = m_readPacketSize, .min = 64, .max = 65536},
        {.key = "timeoutMs", .label = "Timeout", .description = "Bulk transfer timeout in ms",
         .type = IO::DriverProperty::IntField, .value = m_timeoutMs, .min = 1, .max = 5000},
    };
}

QJsonObject UsbBulk::deviceIdentifier() const
{
    QJsonObject id;
    id["vid"] = m_vendorId;
    id["pid"] = m_productId;
    id["interface"] = m_interfaceNumber;
    id["bulkIn"] = m_bulkInEndpoint;
    id["bulkOut"] = m_bulkOutEndpoint;
    return id;
}

bool UsbBulk::selectByIdentifier(const QJsonObject &id)
{
    const int vid = id.value(QStringLiteral("vid")).toInt();
    const int pid = id.value(QStringLiteral("pid")).toInt();
    if (vid <= 0 || pid <= 0)
        return false;
    setVendorId(vid);
    setProductId(pid);
    setInterfaceNumber(id.value(QStringLiteral("interface")).toInt(m_interfaceNumber));
    setBulkInEndpoint(id.value(QStringLiteral("bulkIn")).toInt(m_bulkInEndpoint));
    setBulkOutEndpoint(id.value(QStringLiteral("bulkOut")).toInt(m_bulkOutEndpoint));
    return true;
}

void UsbBulk::setVendorId(int value)
{
    value = std::max(0, std::min(0xFFFF, value));
    if (m_vendorId == value) return;
    m_vendorId = value;
    emit configChanged();
}

void UsbBulk::setProductId(int value)
{
    value = std::max(0, std::min(0xFFFF, value));
    if (m_productId == value) return;
    m_productId = value;
    emit configChanged();
}

void UsbBulk::setInterfaceNumber(int value)
{
    value = std::max(0, std::min(32, value));
    if (m_interfaceNumber == value) return;
    m_interfaceNumber = value;
    emit configChanged();
}

void UsbBulk::setBulkInEndpoint(int value)
{
    value = clampByte(value);
    if (m_bulkInEndpoint == value) return;
    m_bulkInEndpoint = value;
    emit configChanged();
}

void UsbBulk::setBulkOutEndpoint(int value)
{
    value = clampByte(value);
    if (m_bulkOutEndpoint == value) return;
    m_bulkOutEndpoint = value;
    emit configChanged();
}

void UsbBulk::setReadPacketSize(int value)
{
    value = clampPositive(value, 512, 65536);
    if (m_readPacketSize == value) return;
    m_readPacketSize = value;
    emit configChanged();
}

void UsbBulk::setTimeoutMs(int value)
{
    value = clampPositive(value, 20, 5000);
    if (m_timeoutMs == value) return;
    m_timeoutMs = value;
    emit configChanged();
}

void UsbBulk::setReadIntervalMs(int value)
{
    value = clampPositive(value, 2, 1000);
    if (m_readIntervalMs == value) return;
    m_readIntervalMs = value;
    if (m_readTimer.isActive())
        m_readTimer.start(m_readIntervalMs);
    emit configChanged();
}

void UsbBulk::setSuppressOpenErrors(bool value)
{
    m_suppressOpenErrors = value;
}

void UsbBulk::readPending()
{
    if (isOpen())
        readPlatform();
}

QString UsbBulk::formatVidPid() const
{
    return QStringLiteral("VID %1 PID %2").arg(hex4(m_vendorId), hex4(m_productId));
}

#ifdef Q_OS_WIN

bool UsbBulk::openPlatform()
{
    const QString vidNeedle = QStringLiteral("vid_%1").arg(m_vendorId, 4, 16, QLatin1Char('0')).toLower();
    const QString pidNeedle = QStringLiteral("pid_%1").arg(m_productId, 4, 16, QLatin1Char('0')).toLower();
    const QString miNeedle = QStringLiteral("mi_%1").arg(m_interfaceNumber, 2, 16, QLatin1Char('0')).toLower();

    const auto findDevicePath = [&](const GUID &interfaceGuid, QString *instanceId) -> QString {
        HDEVINFO deviceInfo = SetupDiGetClassDevsW(&interfaceGuid, nullptr, nullptr,
                                                   DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
        if (deviceInfo == INVALID_HANDLE_VALUE)
            return {};

        SP_DEVICE_INTERFACE_DATA interfaceData = {};
        interfaceData.cbSize = sizeof(interfaceData);
        QString devicePath;

        for (DWORD index = 0; SetupDiEnumDeviceInterfaces(deviceInfo, nullptr,
                                                           &interfaceGuid,
                                                           index, &interfaceData); ++index) {
            DWORD required = 0;
            SetupDiGetDeviceInterfaceDetailW(deviceInfo, &interfaceData, nullptr, 0, &required, nullptr);
            if (required == 0)
                continue;

            QByteArray storage(static_cast<int>(required), 0);
            auto *detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W *>(storage.data());
            detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

            SP_DEVINFO_DATA devInfoData = {};
            devInfoData.cbSize = sizeof(devInfoData);
            if (!SetupDiGetDeviceInterfaceDetailW(deviceInfo, &interfaceData, detail, required, nullptr, &devInfoData))
                continue;

            const QString path = QString::fromWCharArray(detail->DevicePath);
            if (!usbPathMatches(path, vidNeedle, pidNeedle, miNeedle))
                continue;

            if (instanceId) {
                wchar_t instanceBuffer[512] = {};
                if (SetupDiGetDeviceInstanceIdW(deviceInfo, &devInfoData, instanceBuffer,
                                                DWORD(std::size(instanceBuffer)), nullptr)) {
                    *instanceId = QString::fromWCharArray(instanceBuffer);
                }
            }
            devicePath = path;
            break;
        }

        SetupDiDestroyDeviceInfoList(deviceInfo);
        return devicePath;
    };

    QString instanceId;
    QString genericDevicePath = findDevicePath(GUID_DEVINTERFACE_USB_DEVICE_KPTOOLS, &instanceId);
    QStringList interfaceGuids = registryDeviceInterfaceGuids(instanceId);
    QString devicePath;
    DWORD lastOpenError = ERROR_SUCCESS;

    for (const QString &guidText : interfaceGuids) {
        GUID guid = {};
        if (!guidFromString(guidText, &guid))
            continue;
        const QString candidatePath = findDevicePath(guid, nullptr);
        if (!candidatePath.isEmpty()) {
            devicePath = candidatePath;
            break;
        }
    }

    if (devicePath.isEmpty()) {
        devicePath = genericDevicePath;
    }

    if (devicePath.isEmpty()) {
        if (!m_suppressOpenErrors)
            emit errorOccurred(QStringLiteral("WinUSB device not found: %1").arg(formatVidPid()));
        return false;
    }

    HANDLE device = CreateFileW(reinterpret_cast<LPCWSTR>(devicePath.utf16()),
                                GENERIC_WRITE | GENERIC_READ,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                                nullptr);
    if (device == INVALID_HANDLE_VALUE) {
        if (!m_suppressOpenErrors)
            emit errorOccurred(QStringLiteral("CreateFile WinUSB failed: %1").arg(GetLastError()));
        return false;
    }

    WINUSB_INTERFACE_HANDLE usb = nullptr;
    if (!WinUsb_Initialize(device, &usb)) {
        lastOpenError = GetLastError();
        CloseHandle(device);
        if (!m_suppressOpenErrors)
            emit errorOccurred(QStringLiteral("WinUsb_Initialize failed: %1").arg(lastOpenError));
        return false;
    }

    ULONG timeout = static_cast<ULONG>(m_timeoutMs);
    WinUsb_SetPipePolicy(usb, static_cast<UCHAR>(m_bulkInEndpoint),
                         PIPE_TRANSFER_TIMEOUT, sizeof(timeout), &timeout);
    WinUsb_SetPipePolicy(usb, static_cast<UCHAR>(m_bulkOutEndpoint),
                         PIPE_TRANSFER_TIMEOUT, sizeof(timeout), &timeout);

    m_deviceHandle = device;
    m_usbHandle = usb;
    return true;
}

void UsbBulk::closePlatform()
{
    if (m_usbHandle) {
        WinUsb_Free(reinterpret_cast<WINUSB_INTERFACE_HANDLE>(m_usbHandle));
        m_usbHandle = nullptr;
    }
    if (m_deviceHandle) {
        CloseHandle(reinterpret_cast<HANDLE>(m_deviceHandle));
        m_deviceHandle = nullptr;
    }
}

qint64 UsbBulk::writePlatform(const QByteArray &data)
{
    ULONG written = 0;
    const BOOL ok = WinUsb_WritePipe(reinterpret_cast<WINUSB_INTERFACE_HANDLE>(m_usbHandle),
                                     static_cast<UCHAR>(m_bulkOutEndpoint),
                                     reinterpret_cast<PUCHAR>(const_cast<char *>(data.constData())),
                                     static_cast<ULONG>(data.size()),
                                     &written,
                                     nullptr);
    if (!ok) {
        emit errorOccurred(QStringLiteral("WinUSB bulk write failed: %1").arg(GetLastError()));
        return -1;
    }
    notifyDataSent(data.left(static_cast<int>(written)));
    return static_cast<qint64>(written);
}

void UsbBulk::readPlatform()
{
    QByteArray buffer(m_readPacketSize, Qt::Uninitialized);
    ULONG received = 0;
    const BOOL ok = WinUsb_ReadPipe(reinterpret_cast<WINUSB_INTERFACE_HANDLE>(m_usbHandle),
                                    static_cast<UCHAR>(m_bulkInEndpoint),
                                    reinterpret_cast<PUCHAR>(buffer.data()),
                                    static_cast<ULONG>(buffer.size()),
                                    &received,
                                    nullptr);
    if (ok && received > 0) {
        buffer.resize(static_cast<int>(received));
        processReceivedData(buffer);
        return;
    }

    const DWORD error = GetLastError();
    if (error == ERROR_SEM_TIMEOUT || error == ERROR_IO_PENDING)
        return;

    if (isTransientUsbDisconnectError(error)) {
        m_readTimer.stop();
        closePlatform();
        if (m_open) {
            m_open = false;
            emit connectionChanged();
        }
        return;
    }

    emit errorOccurred(QStringLiteral("WinUSB bulk read failed: %1").arg(error));
}

#elif defined(HAS_LIBUSB)

bool UsbBulk::openPlatform()
{
    libusb_context *context = nullptr;
    const int initResult = libusb_init(&context);
    if (initResult != 0) {
        emit errorOccurred(QStringLiteral("libusb_init failed: %1").arg(initResult));
        return false;
    }

    libusb_device_handle *handle = libusb_open_device_with_vid_pid(context,
        static_cast<uint16_t>(m_vendorId), static_cast<uint16_t>(m_productId));
    if (!handle) {
        libusb_exit(context);
        emit errorOccurred(QStringLiteral("libusb device not found: %1").arg(formatVidPid()));
        return false;
    }

    libusb_set_auto_detach_kernel_driver(handle, 1);
    const int claimResult = libusb_claim_interface(handle, m_interfaceNumber);
    if (claimResult != 0) {
        libusb_close(handle);
        libusb_exit(context);
        emit errorOccurred(QStringLiteral("libusb_claim_interface failed: %1").arg(claimResult));
        return false;
    }

    m_deviceHandle = context;
    m_usbHandle = handle;
    return true;
}

void UsbBulk::closePlatform()
{
    auto *handle = reinterpret_cast<libusb_device_handle *>(m_usbHandle);
    auto *context = reinterpret_cast<libusb_context *>(m_deviceHandle);
    if (handle) {
        libusb_release_interface(handle, m_interfaceNumber);
        libusb_close(handle);
    }
    if (context)
        libusb_exit(context);
    m_usbHandle = nullptr;
    m_deviceHandle = nullptr;
}

qint64 UsbBulk::writePlatform(const QByteArray &data)
{
    int transferred = 0;
    const int result = libusb_bulk_transfer(reinterpret_cast<libusb_device_handle *>(m_usbHandle),
                                           static_cast<unsigned char>(m_bulkOutEndpoint),
                                           reinterpret_cast<unsigned char *>(const_cast<char *>(data.constData())),
                                           data.size(),
                                           &transferred,
                                           static_cast<unsigned int>(m_timeoutMs));
    if (result != 0) {
        emit errorOccurred(QStringLiteral("libusb bulk write failed: %1").arg(result));
        return -1;
    }
    notifyDataSent(data.left(transferred));
    return transferred;
}

void UsbBulk::readPlatform()
{
    QByteArray buffer(m_readPacketSize, Qt::Uninitialized);
    int transferred = 0;
    const int result = libusb_bulk_transfer(reinterpret_cast<libusb_device_handle *>(m_usbHandle),
                                           static_cast<unsigned char>(m_bulkInEndpoint),
                                           reinterpret_cast<unsigned char *>(buffer.data()),
                                           buffer.size(),
                                           &transferred,
                                           static_cast<unsigned int>(m_timeoutMs));
    if (result == 0 && transferred > 0) {
        buffer.resize(transferred);
        processReceivedData(buffer);
    } else if (result != 0 && result != LIBUSB_ERROR_TIMEOUT) {
        emit errorOccurred(QStringLiteral("libusb bulk read failed: %1").arg(result));
    }
}

#else

bool UsbBulk::openPlatform()
{
    emit errorOccurred(QStringLiteral("USB bulk backend is not available on this build"));
    return false;
}

void UsbBulk::closePlatform()
{
}

qint64 UsbBulk::writePlatform(const QByteArray &)
{
    return -1;
}

void UsbBulk::readPlatform()
{
}

#endif

} // namespace Drivers
} // namespace IO
