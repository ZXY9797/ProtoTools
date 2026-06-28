#ifndef PROTOCOLENGINE_H
#define PROTOCOLENGINE_H

#include <QByteArray>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class ProtocolEngine : public QObject
{
    Q_OBJECT

public:
    explicit ProtocolEngine(QObject *parent = nullptr);

    Q_INVOKABLE QVariantMap parseFrame(const QByteArray &rawData);
    Q_INVOKABLE QVariantMap parseHexFrame(const QString &hex);
    Q_INVOKABLE QVariantMap parseFrameDetailed(const QByteArray &rawData);

    Q_INVOKABLE QByteArray buildFrame(quint8 sender, quint8 receiver,
                                      quint8 cmdType, quint16 seq,
                                      quint8 cmdset, quint8 cmdid,
                                      const QByteArray &data);

    static quint8 bcc(const QByteArray &data);
    static quint16 crc16(const QByteArray &data);

    Q_INVOKABLE QByteArray hexToBytes(const QString &hex);
    Q_INVOKABLE int hexByteCount(const QString &hex);
    Q_INVOKABLE QString bytesToHex(const QByteArray &data);
    Q_INVOKABLE int getByte(const QByteArray &data, int index);
    Q_INVOKABLE int frameLength(const QByteArray &data);

private:
    static constexpr quint8 SOF = 0xAA;
    static constexpr int HEADER_SIZE = 11;
    static constexpr int CRC16_SIZE = 2;
    static constexpr int MIN_FRAME_SIZE = HEADER_SIZE + CRC16_SIZE;

    static constexpr quint16 VER_LEN_VER_MASK  = 0x03;
    static constexpr quint16 VER_LEN_LEN_MASK  = 0x3FF;
    static constexpr int     VER_LEN_VER_SHIFT = 10;
    static constexpr int     VER_LEN_LEN_SHIFT = 0;

    static constexpr quint8 CMD_TYPE_IS_ACK_MASK     = 0x01;
    static constexpr quint8 CMD_TYPE_ACK_MODE_MASK   = 0x03;
    static constexpr quint8 CMD_TYPE_ENC_MODE_MASK   = 0x03;
    static constexpr quint8 CMD_TYPE_PRIORITY_MASK   = 0x01;
    static constexpr quint8 CMD_TYPE_RETRANSMIT_MASK = 0x01;

    static constexpr int CMD_TYPE_ACK_MODE_SHIFT   = 1;
    static constexpr int CMD_TYPE_ENC_MODE_SHIFT   = 3;
    static constexpr int CMD_TYPE_PRIORITY_SHIFT   = 5;
    static constexpr int CMD_TYPE_RETRANSMIT_SHIFT = 6;

    static quint16 packVerLen(quint8 ver, quint16 totalLen);
    static quint8 unpackVer(quint16 verLen);
    static quint16 unpackLen(quint16 verLen);
    static quint8 packCmdType(bool isAck, quint8 ackMode, quint8 encMode,
                              quint8 priority, bool retransmit);
};

#endif // PROTOCOLENGINE_H
