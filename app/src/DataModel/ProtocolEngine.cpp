#include "DataModel/ProtocolEngine.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QStringList>

namespace {

quint16 readLe16(const QByteArray &data, int offset)
{
    return static_cast<quint8>(data[offset])
         | (static_cast<quint8>(data[offset + 1]) << 8);
}

void appendLe16(QByteArray &data, quint16 value)
{
    data.append(static_cast<char>(value & 0xFF));
    data.append(static_cast<char>((value >> 8) & 0xFF));
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

ProtocolEngine::ProtocolEngine(QObject *parent)
    : QObject(parent)
{
}

QVariantMap ProtocolEngine::parseHexFrame(const QString &hex)
{
    QVariantMap result;
    QString cleaned = hex.trimmed();
    cleaned.remove(QStringLiteral("0x"), Qt::CaseInsensitive);
    cleaned.remove(QRegularExpression(QStringLiteral("[\\s,;:_-]+")));

    if (cleaned.isEmpty()) {
        result["valid"] = false;
        result["error"] = QStringLiteral("请输入协议二进制数据");
        return result;
    }

    const QRegularExpression invalidRe(QStringLiteral("[^0-9A-Fa-f]"));
    const auto invalidMatch = invalidRe.match(cleaned);
    if (invalidMatch.hasMatch()) {
        result["valid"] = false;
        result["error"] = QStringLiteral("包含非法十六进制字符: %1").arg(invalidMatch.captured(0));
        return result;
    }

    if ((cleaned.size() % 2) != 0) {
        result["valid"] = false;
        result["error"] = QStringLiteral("十六进制字符数量不是偶数");
        return result;
    }

    return parseFrameDetailed(QByteArray::fromHex(cleaned.toLatin1()));
}

QVariantMap ProtocolEngine::parseFrameDetailed(const QByteArray &rawData)
{
    QVariantMap frame;
    frame["valid"] = false;
    frame["inputLength"] = rawData.size();
    frame["rawInputHex"] = bytesToHex(rawData);

    const auto fail = [&frame](const QString &message) {
        frame["error"] = message;
        return frame;
    };

    if (rawData.size() < MIN_FRAME_SIZE) {
        return fail(QStringLiteral("帧长度不足: 实际 %1 字节, 最小 %2 字节")
                        .arg(rawData.size())
                        .arg(MIN_FRAME_SIZE));
    }

    const quint8 sop = static_cast<quint8>(rawData[0]);
    if (sop != SOP) {
        return fail(QStringLiteral("SOP错误: 实际 %1, 应为 0xAA").arg(hexByte(sop)));
    }

    const quint8 version = static_cast<quint8>(rawData[1]);
    if (version != PROTO_VER) {
        return fail(QStringLiteral("协议版本错误: 实际 %1, 应为 0x01").arg(hexByte(version)));
    }

    const quint16 sender = readLe16(rawData, 2);
    const quint16 receiver = readLe16(rawData, 4);
    const quint16 payloadLen = readLe16(rawData, 6);
    const quint8 nextHeader = static_cast<quint8>(rawData[8]);
    const quint8 headerCrc = static_cast<quint8>(rawData[9]);
    const int totalLength = HEADER_SIZE + payloadLen + CRC16_SIZE;
    frame["totalLength"] = totalLength;
    frame["payloadLen"] = QString::number(payloadLen);

    if (payloadLen < GENERIC_HEADER_SIZE) {
        return fail(QStringLiteral("Payload长度错误: 实际 %1 字节, Generic Header 至少 %2 字节")
                        .arg(payloadLen)
                        .arg(GENERIC_HEADER_SIZE));
    }

    if (rawData.size() < totalLength) {
        return fail(QStringLiteral("帧长度不足: 头部声明需要 %1 字节, 实际 %2 字节")
                        .arg(totalLength)
                        .arg(rawData.size()));
    }

    if (rawData.size() > totalLength) {
        return fail(QStringLiteral("帧长度不匹配: 头部声明 %1 字节, 实际输入 %2 字节")
                        .arg(totalLength)
                        .arg(rawData.size()));
    }

    if (nextHeader != NEXT_HEADER_GENERIC) {
        return fail(QStringLiteral("NextHeader错误: 实际 %1, 应为 0x01").arg(hexByte(nextHeader)));
    }

    const QByteArray header = rawData.left(HEADER_SIZE);
    const QByteArray payload = rawData.mid(HEADER_SIZE, payloadLen);
    const quint16 storedCrc = readLe16(rawData, HEADER_SIZE + payloadLen);
    const quint8 calcHeaderCrc = crc8(header.left(HEADER_SIZE - 1));
    const quint16 calcPayloadCrc = crc16(payload);

    const quint16 seq = readLe16(payload, 0);
    const quint16 cmd = readLe16(payload, 2);
    const quint16 flags = readLe16(payload, 4);
    const QByteArray data = payload.mid(GENERIC_HEADER_SIZE);
    const quint8 cmdset = static_cast<quint8>((cmd >> 8) & 0xFF);
    const quint8 cmdid = static_cast<quint8>(cmd & 0xFF);
    const bool isResponse = (flags & FLAG_RESPONSE) != 0;

    frame["sender"] = hexWord(sender);
    frame["receiver"] = hexWord(receiver);
    frame["seq"] = QStringLiteral("%1").arg(seq, 4, 10, QChar('0'));
    frame["type"] = isResponse ? QStringLiteral("ACK") : QStringLiteral("REQ");
    frame["cmd"] = hexWord(cmd);
    frame["cmdSet"] = hexByte(cmdset);
    frame["cmdId"] = hexByte(cmdid);
    frame["len"] = QString::number(totalLength);
    frame["data"] = bytesToHex(data);
    frame["dataLen"] = data.size();
    frame["attr"] = hexWord(flags);
    frame["flags"] = hexWord(flags);
    frame["nextHeader"] = hexByte(nextHeader);
    frame["crc8"] = hexByte(headerCrc);
    frame["crc8Calc"] = hexByte(calcHeaderCrc);
    frame["headerCrc"] = hexByte(headerCrc);
    frame["headerCrcCalc"] = hexByte(calcHeaderCrc);
    frame["crc"] = hexWord(storedCrc);
    frame["crc16"] = hexWord(storedCrc);
    frame["crc16Calc"] = hexWord(calcPayloadCrc);
    frame["crcCalc"] = hexWord(calcPayloadCrc);
    frame["crcValid"] = (calcHeaderCrc == headerCrc) && (calcPayloadCrc == storedCrc);
    frame["rawHex"] = bytesToHex(rawData);

    if (calcHeaderCrc != headerCrc) {
        return fail(QStringLiteral("CRC8校验失败: 帧内 %1, 计算 %2")
                        .arg(hexByte(headerCrc), hexByte(calcHeaderCrc)));
    }

    if (calcPayloadCrc != storedCrc) {
        return fail(QStringLiteral("CRC16校验失败: 帧内 %1, 计算 %2")
                        .arg(hexWord(storedCrc), hexWord(calcPayloadCrc)));
    }

    frame["valid"] = true;
    frame["error"] = QString();
    return frame;
}

QVariantMap ProtocolEngine::parseFrame(const QByteArray &rawData)
{
    QVariantMap frame;
    if (rawData.size() < MIN_FRAME_SIZE)
        return frame;

    if (static_cast<quint8>(rawData[0]) != SOP)
        return frame;

    const quint8 version = static_cast<quint8>(rawData[1]);
    if (version != PROTO_VER)
        return frame;

    const quint16 sender = readLe16(rawData, 2);
    const quint16 receiver = readLe16(rawData, 4);
    const quint16 payloadLen = readLe16(rawData, 6);
    const quint8 nextHeader = static_cast<quint8>(rawData[8]);
    const quint8 headerCrc = static_cast<quint8>(rawData[9]);
    const int totalLength = HEADER_SIZE + payloadLen + CRC16_SIZE;

    if (payloadLen < GENERIC_HEADER_SIZE || rawData.size() < totalLength)
        return frame;

    const QByteArray packet = rawData.left(totalLength);
    const QByteArray header = packet.left(HEADER_SIZE);
    const QByteArray payload = packet.mid(HEADER_SIZE, payloadLen);
    const quint16 storedCrc = readLe16(packet, HEADER_SIZE + payloadLen);
    const quint8 calcHeaderCrc = crc8(header.left(HEADER_SIZE - 1));
    const quint16 calcPayloadCrc = crc16(payload);

    if (nextHeader != NEXT_HEADER_GENERIC)
        return frame;

    const quint16 seq = readLe16(payload, 0);
    const quint16 cmd = readLe16(payload, 2);
    const quint16 flags = readLe16(payload, 4);
    const QByteArray data = payload.mid(GENERIC_HEADER_SIZE);
    const quint8 cmdset = static_cast<quint8>((cmd >> 8) & 0xFF);
    const quint8 cmdid = static_cast<quint8>(cmd & 0xFF);
    const bool isResponse = (flags & FLAG_RESPONSE) != 0;

    frame["timestamp"] = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    frame["sender"] = hexWord(sender);
    frame["receiver"] = hexWord(receiver);
    frame["seq"] = QStringLiteral("%1").arg(seq, 4, 10, QChar('0'));
    frame["type"] = isResponse ? QStringLiteral("ACK") : QStringLiteral("REQ");
    frame["cmd"] = hexWord(cmd);
    frame["cmdSet"] = hexByte(cmdset);
    frame["cmdId"] = hexByte(cmdid);
    frame["len"] = QString::number(totalLength);
    frame["payloadLen"] = QString::number(payloadLen);
    frame["data"] = bytesToHex(data);
    frame["attr"] = hexWord(flags);
    frame["flags"] = hexWord(flags);
    frame["nextHeader"] = hexByte(nextHeader);
    frame["crc8"] = hexByte(headerCrc);
    frame["crc8Calc"] = hexByte(calcHeaderCrc);
    frame["crc"] = hexWord(storedCrc);
    frame["crc16"] = hexWord(storedCrc);
    frame["crc16Calc"] = hexWord(calcPayloadCrc);
    frame["crcValid"] = (calcHeaderCrc == headerCrc) && (calcPayloadCrc == storedCrc);
    frame["rawHex"] = bytesToHex(packet);

    return frame;
}

QByteArray ProtocolEngine::buildFrame(quint16 sender, quint16 receiver,
                                      quint8 attr, quint16 seq,
                                      quint8 cmdset, quint8 cmdid,
                                      const QByteArray &data)
{
    QByteArray payload;
    appendLe16(payload, seq);
    appendLe16(payload, static_cast<quint16>((cmdset << 8) | cmdid));

    quint16 flags = static_cast<quint16>(attr & 0x03);
    if ((flags & FLAG_RESPONSE) == 0)
        flags |= FLAG_ACK_REQUIRED;
    appendLe16(payload, flags);
    payload.append(data);

    QByteArray frame;
    frame.append(static_cast<char>(SOP));
    frame.append(static_cast<char>(PROTO_VER));
    appendLe16(frame, sender);
    appendLe16(frame, receiver);
    appendLe16(frame, static_cast<quint16>(payload.size()));
    frame.append(static_cast<char>(NEXT_HEADER_GENERIC));
    frame.append(static_cast<char>(crc8(frame)));
    frame.append(payload);
    appendLe16(frame, crc16(payload));

    return frame;
}

quint8 ProtocolEngine::crc8(const QByteArray &data)
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

quint16 ProtocolEngine::crc16(const QByteArray &data)
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

QByteArray ProtocolEngine::hexToBytes(const QString &hex)
{
    QString cleaned = hex;
    cleaned.remove(QLatin1String("0x"), Qt::CaseInsensitive);
    cleaned.remove(QLatin1Char(' '));
    cleaned.remove(QLatin1Char('\n'));
    cleaned.remove(QLatin1Char('\r'));
    cleaned.remove(QLatin1Char('\t'));
    return QByteArray::fromHex(cleaned.toLatin1());
}

int ProtocolEngine::hexByteCount(const QString &hex)
{
    return hexToBytes(hex).size();
}

QString ProtocolEngine::bytesToHex(const QByteArray &data)
{
    return data.toHex(' ').toUpper();
}

int ProtocolEngine::getByte(const QByteArray &data, int index)
{
    if (index < 0 || index >= data.size())
        return 0;
    return static_cast<quint8>(data.at(index));
}

int ProtocolEngine::frameLength(const QByteArray &data)
{
    return data.size();
}
