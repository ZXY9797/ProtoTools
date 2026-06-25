#include "IO/Drivers/UART.h"

namespace IO {
namespace Drivers {

UART::UART(QObject *parent)
    : HAL_Driver(parent)
{
#ifdef HAS_SERIALPORT
    m_port = new QSerialPort(this);
    connect(m_port, &QSerialPort::readyRead, this, &UART::onReadyRead);
    connect(m_port, &QSerialPort::errorOccurred, this,
            [this](QSerialPort::SerialPortError error) {
        if (error != QSerialPort::NoError && error != QSerialPort::TimeoutError) {
            emit errorOccurred(m_port->errorString());
        }
    });
#endif
}

UART::~UART()
{
    close();
}

bool UART::open()
{
#ifdef HAS_SERIALPORT
    if (!m_port) return false;
    const QStringList ports = availablePorts();
    if (m_portName.trimmed().isEmpty()) {
        if (ports.isEmpty()) {
            emit errorOccurred(QStringLiteral("No UART port available"));
            return false;
        }
        setPortName(ports.first());
    } else if (!ports.contains(m_portName)) {
        emit errorOccurred(QStringLiteral("UART port not found: %1").arg(m_portName));
        return false;
    }
    applyConfig();
    if (m_port->open(QIODevice::ReadWrite)) {
        emit connectionChanged();
        return true;
    }
    emit errorOccurred(m_port->errorString());
    return false;
#else
    emit errorOccurred("Qt SerialPort 未编译");
    return false;
#endif
}

void UART::close()
{
#ifdef HAS_SERIALPORT
    if (m_port && m_port->isOpen()) {
        m_port->close();
        emit connectionChanged();
    }
#endif
}

bool UART::isOpen() const
{
#ifdef HAS_SERIALPORT
    return m_port && m_port->isOpen();
#else
    return false;
#endif
}

qint64 UART::write(const QByteArray &data)
{
#ifdef HAS_SERIALPORT
    if (m_port && m_port->isOpen()) {
        return m_port->write(data);
    }
#endif
    return -1;
}

void UART::setProperty(const QString &key, const QVariant &value)
{
    if (key == "portName") setPortName(value.toString());
    else if (key == "baudRate") setBaudRate(value.toInt());
    else if (key == "dataBits") setDataBits(value.toInt());
    else if (key == "stopBits") setStopBits(value.toInt());
    else if (key == "parity") setParity(value.toString());
}

QVariant UART::getProperty(const QString &key) const
{
    if (key == "portName") return m_portName;
    if (key == "baudRate") return m_baudRate;
    if (key == "dataBits") return m_dataBits;
    if (key == "stopBits") return m_stopBits;
    if (key == "parity") return m_parity;
    return {};
}

void UART::setPortName(const QString &name)
{
    if (m_portName != name) {
        m_portName = name;
        emit configChanged();
    }
}

void UART::setBaudRate(int rate)
{
    if (m_baudRate != rate) {
        m_baudRate = rate;
        emit configChanged();
    }
}

void UART::setDataBits(int bits)
{
    if (m_dataBits != bits) {
        m_dataBits = bits;
        emit configChanged();
    }
}

void UART::setStopBits(int bits)
{
    if (m_stopBits != bits) {
        m_stopBits = bits;
        emit configChanged();
    }
}

void UART::setParity(const QString &p)
{
    if (m_parity != p) {
        m_parity = p;
        emit configChanged();
    }
}

QStringList UART::availablePorts() const
{
    QStringList ports;
#ifdef HAS_SERIALPORT
    const auto infos = QSerialPortInfo::availablePorts();
    for (const auto &info : infos) {
        // Filter out tty.* devices on macOS, only use cu.*
#ifdef Q_OS_MACOS
        if (info.portName().toLower().startsWith("tty."))
            continue;
#endif
        ports.append(info.portName());
    }
#else
    ports << "COM1" << "COM3" << "/dev/ttyUSB0";
#endif
    return ports;
}

bool UART::configurationOk() const noexcept
{
    return !m_portName.isEmpty();
}

QList<IO::DriverProperty> UART::driverProperties() const
{
    return {
        {.key = "portName", .label = "端口", .description = "选择串口设备",
         .type = IO::DriverProperty::ComboBox, .value = m_portName,
         .options = availablePorts()},
        {.key = "baudRate", .label = "波特率", .description = "通信速率",
         .type = IO::DriverProperty::ComboBox, .value = QString::number(m_baudRate),
         .options = {"9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600", "1000000", "2000000"}},
        {.key = "dataBits", .label = "数据位", .description = "每帧数据位数",
         .type = IO::DriverProperty::ComboBox, .value = QString::number(m_dataBits),
         .options = {"8", "7"}},
        {.key = "stopBits", .label = "停止位", .description = "每帧停止位数",
         .type = IO::DriverProperty::ComboBox, .value = QString::number(m_stopBits),
         .options = {"1", "2"}},
        {.key = "parity", .label = "校验位", .description = "奇偶校验模式",
         .type = IO::DriverProperty::ComboBox, .value = m_parity,
         .options = {"N", "E", "O"}},
    };
}

QJsonObject UART::deviceIdentifier() const
{
    QJsonObject id;
#ifdef HAS_SERIALPORT
    const auto infos = QSerialPortInfo::availablePorts();
    for (const auto &info : infos) {
        if (info.portName() == m_portName) {
            id["portName"] = info.portName();
            id["systemLocation"] = info.systemLocation();
            if (info.hasVendorIdentifier())
                id["vid"] = static_cast<int>(info.vendorIdentifier());
            if (info.hasProductIdentifier())
                id["pid"] = static_cast<int>(info.productIdentifier());
            if (!info.serialNumber().isEmpty())
                id["serialNumber"] = info.serialNumber();
            break;
        }
    }
#else
    id["portName"] = m_portName;
#endif
    return id;
}

bool UART::selectByIdentifier(const QJsonObject &id)
{
#ifdef HAS_SERIALPORT
    QString targetName = id.value("portName").toString();
    if (targetName.isEmpty()) return false;

    const auto infos = QSerialPortInfo::availablePorts();
    for (const auto &info : infos) {
        if (info.portName() == targetName) {
            setPortName(targetName);
            return true;
        }
    }
#endif
    return false;
}

void UART::onReadyRead()
{
#ifdef HAS_SERIALPORT
    if (m_port) {
        QByteArray data = m_port->readAll();
        processReceivedData(data);
    }
#endif
}

void UART::applyConfig()
{
#ifdef HAS_SERIALPORT
    if (!m_port) return;

    m_port->setPortName(m_portName);
    m_port->setBaudRate(m_baudRate);

    switch (m_dataBits) {
    case 7: m_port->setDataBits(QSerialPort::Data7); break;
    default: m_port->setDataBits(QSerialPort::Data8); break;
    }

    switch (m_stopBits) {
    case 2: m_port->setStopBits(QSerialPort::TwoStop); break;
    default: m_port->setStopBits(QSerialPort::OneStop); break;
    }

    if (m_parity == "E") m_port->setParity(QSerialPort::EvenParity);
    else if (m_parity == "O") m_port->setParity(QSerialPort::OddParity);
    else m_port->setParity(QSerialPort::NoParity);
#endif
}

} // namespace Drivers
} // namespace IO
