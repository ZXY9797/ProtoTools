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

// ── ver_len 编解码 ──

quint16 ProtocolEngine::packVerLen(quint8 ver, quint16 totalLen)
{
    return (totalLen & VER_LEN_LEN_MASK)
         | (static_cast<quint16>(ver & VER_LEN_VER_MASK) << VER_LEN_VER_SHIFT);
}

quint8 ProtocolEngine::unpackVer(quint16 verLen)
{
    return static_cast<quint8>((verLen >> VER_LEN_VER_SHIFT) & VER_LEN_VER_MASK);
}

quint16 ProtocolEngine::unpackLen(quint16 verLen)
{
    return verLen & VER_LEN_LEN_MASK;
}

// ── cmd_type 编码 ──

quint8 ProtocolEngine::packCmdType(bool isAck, quint8 ackMode, quint8 encMode,
                                    quint8 priority, bool retransmit)
{
    return (isAck ? 1 : 0)
         | ((ackMode & CMD_TYPE_ACK_MODE_MASK) << CMD_TYPE_ACK_MODE_SHIFT)
         | ((encMode & CMD_TYPE_ENC_MODE_MASK) << CMD_TYPE_ENC_MODE_SHIFT)
         | ((priority & CMD_TYPE_PRIORITY_MASK) << CMD_TYPE_PRIORITY_SHIFT)
         | ((retransmit ? 1 : 0) << CMD_TYPE_RETRANSMIT_SHIFT);
}

// ── 帧解析 ──

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

    const quint8 sof = static_cast<quint8>(rawData[0]);
    if (sof != SOF) {
        return fail(QStringLiteral("SOF错误: 实际 %1, 应为 0xAA").arg(hexByte(sof)));
    }

    const quint16 verLen = readLe16(rawData, 1);
    const quint8 version = unpackVer(verLen);
    const quint16 totalLen = unpackLen(verLen);

    if (totalLen < MIN_FRAME_SIZE || totalLen > rawData.size()) {
        return fail(QStringLiteral("VerLen帧长错误: 声明 %1 字节, 实际输入 %2 字节")
                        .arg(totalLen).arg(rawData.size()));
    }

    if (rawData.size() > totalLen) {
        return fail(QStringLiteral("帧长度不匹配: 头部声明 %1 字节, 实际输入 %2 字节")
                        .arg(totalLen).arg(rawData.size()));
    }

    const QByteArray packet = rawData.left(totalLen);

    // BCC 校验前 3 字节
    const quint8 headCrcStored = static_cast<quint8>(packet[3]);
    const quint8 headCrcCalc = bcc(packet.left(3));
    if (headCrcStored != headCrcCalc) {
        return fail(QStringLiteral("HeadCRC(BCC)校验失败: 帧内 %1, 计算 %2")
                        .arg(hexByte(headCrcStored), hexByte(headCrcCalc)));
    }

    // CRC-16/CCITT 校验
    const size_t crcAreaLen = totalLen - CRC16_SIZE;
    const quint16 storedCrc = static_cast<quint16>(packet[crcAreaLen])
                            | (static_cast<quint16>(packet[crcAreaLen + 1]) << 8);
    const quint16 calcCrc = crc16(packet.left(crcAreaLen));

    const quint8 cmdType = static_cast<quint8>(packet[4]);
    const quint8 sender = static_cast<quint8>(packet[5]);
    const quint8 receiver = static_cast<quint8>(packet[6]);
    const quint16 seq = readLe16(packet, 7);
    const quint8 cmdset = static_cast<quint8>(packet[9]);
    const quint8 cmdid = static_cast<quint8>(packet[10]);

    const size_t dataLen = totalLen - HEADER_SIZE - CRC16_SIZE;
    const QByteArray data = (dataLen > 0) ? packet.mid(HEADER_SIZE, static_cast<int>(dataLen)) : QByteArray();

    const bool isAck = (cmdType & CMD_TYPE_IS_ACK_MASK) != 0;
    const quint8 ackMode = (cmdType >> CMD_TYPE_ACK_MODE_SHIFT) & CMD_TYPE_ACK_MODE_MASK;
    const quint8 encMode = (cmdType >> CMD_TYPE_ENC_MODE_SHIFT) & CMD_TYPE_ENC_MODE_MASK;
    const quint8 priority = (cmdType >> CMD_TYPE_PRIORITY_SHIFT) & CMD_TYPE_PRIORITY_MASK;
    const bool retransmit = (cmdType >> CMD_TYPE_RETRANSMIT_SHIFT) & CMD_TYPE_RETRANSMIT_MASK;

    frame["totalLength"] = totalLen;
    frame["version"] = QString::number(version);
    frame["sender"] = hexByte(sender);
    frame["receiver"] = hexByte(receiver);
    frame["seq"] = QStringLiteral("%1").arg(seq, 4, 10, QChar('0'));
    frame["type"] = isAck ? QStringLiteral("ACK") : QStringLiteral("REQ");
    frame["cmd"] = hexWord(static_cast<quint16>((cmdset << 8) | cmdid));
    frame["cmdSet"] = hexByte(cmdset);
    frame["cmdId"] = hexByte(cmdid);
    frame["len"] = QString::number(totalLen);
    frame["data"] = bytesToHex(data);
    frame["dataLen"] = data.size();
    frame["cmdType"] = hexByte(cmdType);
    frame["ackMode"] = QString::number(ackMode);
    frame["encMode"] = QString::number(encMode);
    frame["priority"] = QString::number(priority);
    frame["retransmit"] = retransmit;
    frame["headCrc"] = hexByte(headCrcStored);
    frame["headCrcCalc"] = hexByte(headCrcCalc);
    frame["crc"] = hexWord(storedCrc);
    frame["crc16"] = hexWord(storedCrc);
    frame["crc16Calc"] = hexWord(calcCrc);
    frame["crcCalc"] = hexWord(calcCrc);
    frame["crcValid"] = (headCrcStored == headCrcCalc) && (calcCrc == storedCrc);
    frame["rawHex"] = bytesToHex(packet);

    frame["valid"] = true;
    frame["error"] = QString();
    return frame;
}

QVariantMap ProtocolEngine::parseFrame(const QByteArray &rawData)
{
    QVariantMap frame;
    if (rawData.size() < MIN_FRAME_SIZE)
        return frame;

    if (static_cast<quint8>(rawData[0]) != SOF)
        return frame;

    const quint16 verLen = readLe16(rawData, 1);
    const quint16 totalLen = unpackLen(verLen);

    if (totalLen < MIN_FRAME_SIZE || rawData.size() < totalLen)
        return frame;

    const QByteArray packet = rawData.left(totalLen);

    const quint8 headCrcStored = static_cast<quint8>(packet[3]);
    const quint8 headCrcCalc = bcc(packet.left(3));
    if (headCrcStored != headCrcCalc)
        return frame;

    const size_t crcAreaLen = totalLen - CRC16_SIZE;
    const quint16 storedCrc = static_cast<quint16>(packet[crcAreaLen])
                            | (static_cast<quint16>(packet[crcAreaLen + 1]) << 8);
    const quint16 calcCrc = crc16(packet.left(crcAreaLen));
    if (calcCrc != storedCrc)
        return frame;

    const quint8 cmdType = static_cast<quint8>(packet[4]);
    const quint8 sender = static_cast<quint8>(packet[5]);
    const quint8 receiver = static_cast<quint8>(packet[6]);
    const quint16 seq = readLe16(packet, 7);
    const quint8 cmdset = static_cast<quint8>(packet[9]);
    const quint8 cmdid = static_cast<quint8>(packet[10]);
    const size_t dataLen = totalLen - HEADER_SIZE - CRC16_SIZE;
    const QByteArray data = (dataLen > 0) ? packet.mid(HEADER_SIZE, static_cast<int>(dataLen)) : QByteArray();
    const bool isAck = (cmdType & CMD_TYPE_IS_ACK_MASK) != 0;

    frame["timestamp"] = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    frame["sender"] = hexByte(sender);
    frame["receiver"] = hexByte(receiver);
    frame["seq"] = QStringLiteral("%1").arg(seq, 4, 10, QChar('0'));
    frame["type"] = isAck ? QStringLiteral("ACK") : QStringLiteral("REQ");
    frame["cmd"] = hexWord(static_cast<quint16>((cmdset << 8) | cmdid));
    frame["cmdSet"] = hexByte(cmdset);
    frame["cmdId"] = hexByte(cmdid);
    frame["len"] = QString::number(totalLen);
    frame["payloadLen"] = QString::number(dataLen);
    frame["data"] = bytesToHex(data);
    frame["cmdType"] = hexByte(cmdType);
    frame["headCrc"] = hexByte(headCrcStored);
    frame["headCrcCalc"] = hexByte(headCrcCalc);
    frame["crc"] = hexWord(storedCrc);
    frame["crc16"] = hexWord(storedCrc);
    frame["crc16Calc"] = hexWord(calcCrc);
    frame["crcValid"] = (headCrcStored == headCrcCalc) && (calcCrc == storedCrc);
    frame["rawHex"] = bytesToHex(packet);

    return frame;
}

QByteArray ProtocolEngine::buildFrame(quint8 sender, quint8 receiver,
                                      quint8 cmdType, quint16 seq,
                                      quint8 cmdset, quint8 cmdid,
                                      const QByteArray &data)
{
    const quint16 totalLen = HEADER_SIZE + data.size() + CRC16_SIZE;

    QByteArray frame;
    frame.append(static_cast<char>(SOF));

    const quint16 verLen = packVerLen(1, totalLen);
    frame.append(static_cast<char>(verLen & 0xFF));
    frame.append(static_cast<char>((verLen >> 8) & 0xFF));

    frame.append(static_cast<char>(bcc(frame)));

    frame.append(static_cast<char>(cmdType));
    frame.append(static_cast<char>(sender));
    frame.append(static_cast<char>(receiver));

    frame.append(static_cast<char>(seq & 0xFF));
    frame.append(static_cast<char>((seq >> 8) & 0xFF));

    frame.append(static_cast<char>(cmdset));
    frame.append(static_cast<char>(cmdid));

    frame.append(data);

    appendLe16(frame, crc16(frame));

    return frame;
}

quint8 ProtocolEngine::bcc(const QByteArray &data)
{
    quint8 sum = 0;
    for (char ch : data)
        sum ^= static_cast<quint8>(ch);
    return sum;
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
