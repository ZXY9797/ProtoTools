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
    return QStringLiteral("0x%1").arg(value, 2, 16, QChar('0')).toUpper();
}

QString hexWord(quint16 value)
{
    return QStringLiteral("0x%1").arg(value, 4, 16, QChar('0')).toUpper();
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
            if (static_cast<quint8>(m_buffer[pos]) != SOP) {
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

    if (static_cast<quint8>(m_buffer[pos + 1]) != PROTO_VER)
        return result;

    const quint16 payloadLen = readLe16(m_buffer, pos + 6);
    const int totalLength = HEADER_SIZE + payloadLen + CRC16_SIZE;
    if (payloadLen < GENERIC_HEADER_SIZE)
        return result;

    if (pos + totalLength > m_buffer.size()) {
        result.totalLength = totalLength;
        return result;
    }

    const QByteArray packet = m_buffer.mid(pos, totalLength);
    const QByteArray header = packet.left(HEADER_SIZE);
    const QByteArray payload = packet.mid(HEADER_SIZE, payloadLen);
    const quint8 nextHeader = static_cast<quint8>(packet[8]);
    if (nextHeader != NEXT_HEADER_GENERIC)
        return result;

    const quint8 headerCrc = static_cast<quint8>(packet[9]);
    const quint16 storedCrc = readLe16(packet, HEADER_SIZE + payloadLen);
    const quint8 calcHeaderCrc = crc8(header.left(HEADER_SIZE - 1));
    const quint16 calcPayloadCrc = crc16(payload);
    const bool crcValid = (calcHeaderCrc == headerCrc) && (calcPayloadCrc == storedCrc);

    const quint16 sender = readLe16(packet, 2);
    const quint16 receiver = readLe16(packet, 4);
    const quint16 seq = readLe16(payload, 0);
    const quint16 cmd = readLe16(payload, 2);
    const quint16 flags = readLe16(payload, 4);
    const QByteArray userData = payload.mid(GENERIC_HEADER_SIZE);
    const quint8 cmdset = static_cast<quint8>((cmd >> 8) & 0xFF);
    const quint8 cmdid = static_cast<quint8>(cmd & 0xFF);
    const bool isResponse = (flags & FLAG_RESPONSE) != 0;

    QVariantMap frame;
    frame["timestamp"] = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    frame["direction"] = QStringLiteral("RX");
    frame["sender"] = hexWord(sender);
    frame["receiver"] = hexWord(receiver);
    frame["seq"] = QStringLiteral("%1").arg(seq, 4, 10, QChar('0'));
    frame["type"] = isResponse ? QStringLiteral("ACK") : QStringLiteral("REQ");
    frame["cmd"] = hexWord(cmd);
    frame["cmdSet"] = hexByte(cmdset);
    frame["cmdId"] = hexByte(cmdid);
    frame["len"] = QString::number(totalLength);
    frame["payloadLen"] = QString::number(payloadLen);
    frame["data"] = bytesToHex(userData);
    frame["attr"] = hexWord(flags);
    frame["flags"] = hexWord(flags);
    frame["nextHeader"] = hexByte(nextHeader);
    frame["crc8"] = hexByte(headerCrc);
    frame["crc8Calc"] = hexByte(calcHeaderCrc);
    frame["crc"] = hexWord(storedCrc);
    frame["crc16"] = hexWord(storedCrc);
    frame["crc16Calc"] = hexWord(calcPayloadCrc);
    frame["crcValid"] = crcValid;
    frame["rawHex"] = bytesToHex(packet);

    result.frame = frame;
    result.totalLength = totalLength;
    result.ok = true;
    result.crcValid = crcValid;
    return result;
}

quint8 FrameProcessor::crc8(const QByteArray &data)
{
    quint8 crc = 0x00;
    for (char ch : data) {
        crc ^= static_cast<quint8>(ch);
        for (int i = 0; i < 8; ++i)
            crc = (crc & 0x01) ? static_cast<quint8>((crc >> 1) ^ 0x8C)
                               : static_cast<quint8>(crc >> 1);
    }
    return crc;
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
