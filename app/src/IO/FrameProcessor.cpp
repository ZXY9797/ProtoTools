#include "IO/FrameProcessor.h"

#include <QDateTime>
#include <QMutexLocker>

namespace {

quint16 readLe16(const QByteArray &data, int offset)
{
    return static_cast<quint8>(data[offset])
         | (static_cast<quint8>(data[offset + 1]) << 8);
}

QString bytesToHex(const QByteArray &data)
{
    return data.toHex(' ').toUpper();
}

QString hexByte(quint8 value)
{
    return QStringLiteral("0x%1").arg(QStringLiteral("%1").arg(value, 2, 16, QChar('0')).toUpper());
}

QString hexWord(quint16 value)
{
    return QStringLiteral("0x%1").arg(QStringLiteral("%1").arg(value, 4, 16, QChar('0')).toUpper());
}

} // namespace

FrameProcessor::FrameProcessor(QObject *parent)
    : QObject(parent)
{
}

void FrameProcessor::feedData(const QByteArray &data)
{
    if (data.isEmpty())
        return;

    QList<QVariantMap> parsedFrames;
    {
        QMutexLocker lock(&m_mutex);
        m_buffer.append(data);
        m_bytesProcessed += data.size();

        if (m_buffer.size() > 10 * 1024 * 1024)
            m_buffer = m_buffer.mid(m_buffer.size() / 2);

        int pos = 0;
        while (pos <= m_buffer.size() - MIN_FRAME_SIZE) {
            if (static_cast<quint8>(m_buffer[pos]) != SOF) {
                ++pos;
                continue;
            }

            ParseResult result = tryParseFrame(pos);
            if (result.ok) {
                ++m_framesExtracted;
                pos += result.totalLength;
                parsedFrames.append(result.frame);
            } else if (result.totalLength > 0) {
                break;
            } else {
                ++pos;
            }
        }

        if (pos > 0)
            m_buffer = m_buffer.mid(pos);
    }

    for (const auto &frame : parsedFrames)
        emit frameReceived(frame);
}

void FrameProcessor::clear()
{
    QMutexLocker lock(&m_mutex);
    m_buffer.clear();
    m_framesExtracted = 0;
    m_bytesProcessed = 0;
}

FrameProcessor::ParseResult FrameProcessor::tryParseFrame(int pos)
{
    ParseResult result;
    if (pos + HEADER_SIZE > m_buffer.size())
        return result;

    const quint16 verLen = readLe16(m_buffer, pos + 1);
    const quint16 totalLen = verLen & VER_LEN_LEN_MASK;

    if (totalLen < MIN_FRAME_SIZE || pos + totalLen > m_buffer.size()) {
        result.totalLength = totalLen;
        return result;
    }

    const QByteArray packet = m_buffer.mid(pos, totalLen);

    const quint8 headCrcStored = static_cast<quint8>(packet[3]);
    const quint8 headCrcCalc = bcc(packet.left(3));
    if (headCrcStored != headCrcCalc)
        return result;

    const size_t crcAreaLen = totalLen - CRC16_SIZE;
    const quint16 storedCrc = static_cast<quint16>(packet[crcAreaLen])
                            | (static_cast<quint16>(packet[crcAreaLen + 1]) << 8);
    const quint16 calcCrc = crc16(packet.left(crcAreaLen));
    const bool crcValid = (headCrcStored == headCrcCalc) && (calcCrc == storedCrc);

    const quint8 cmdType = static_cast<quint8>(packet[4]);
    const quint8 sender = static_cast<quint8>(packet[5]);
    const quint8 receiver = static_cast<quint8>(packet[6]);
    const quint16 seq = readLe16(packet, 7);
    const quint8 cmdset = static_cast<quint8>(packet[9]);
    const quint8 cmdid = static_cast<quint8>(packet[10]);
    const size_t dataLen = totalLen - HEADER_SIZE - CRC16_SIZE;
    const QByteArray userData = (dataLen > 0) ? packet.mid(HEADER_SIZE, static_cast<int>(dataLen)) : QByteArray();
    const bool isAck = (cmdType & CMD_TYPE_IS_ACK_MASK) != 0;

    QVariantMap frame;
    frame["timestamp"] = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    frame["direction"] = QStringLiteral("RX");
    frame["sender"] = hexByte(sender);
    frame["receiver"] = hexByte(receiver);
    frame["seq"] = QStringLiteral("%1").arg(seq, 4, 10, QChar('0'));
    frame["type"] = isAck ? QStringLiteral("ACK") : QStringLiteral("REQ");
    frame["cmd"] = hexWord(static_cast<quint16>((cmdset << 8) | cmdid));
    frame["cmdSet"] = hexByte(cmdset);
    frame["cmdId"] = hexByte(cmdid);
    frame["len"] = QString::number(totalLen);
    frame["payloadLen"] = QString::number(dataLen);
    frame["data"] = bytesToHex(userData);
    frame["cmdType"] = hexByte(cmdType);
    frame["headCrc"] = hexByte(headCrcStored);
    frame["headCrcCalc"] = hexByte(headCrcCalc);
    frame["crc"] = hexWord(storedCrc);
    frame["crc16"] = hexWord(storedCrc);
    frame["crc16Calc"] = hexWord(calcCrc);
    frame["crcValid"] = crcValid;
    frame["rawHex"] = bytesToHex(packet);

    result.frame = frame;
    result.totalLength = totalLen;
    result.ok = true;
    result.crcValid = crcValid;
    return result;
}

quint8 FrameProcessor::bcc(const QByteArray &data)
{
    quint8 sum = 0;
    for (char ch : data)
        sum ^= static_cast<quint8>(ch);
    return sum;
}

quint16 FrameProcessor::crc16(const QByteArray &data)
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
