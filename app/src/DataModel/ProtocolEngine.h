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

    Q_INVOKABLE QByteArray buildFrame(quint16 sender, quint16 receiver,
                                      quint8 attr, quint16 seq,
                                      quint8 cmdset, quint8 cmdid,
                                      const QByteArray &data);

    static quint8 crc8(const QByteArray &data);
    static quint16 crc16(const QByteArray &data);

    Q_INVOKABLE QByteArray hexToBytes(const QString &hex);
    Q_INVOKABLE int hexByteCount(const QString &hex);
    Q_INVOKABLE QString bytesToHex(const QByteArray &data);
    Q_INVOKABLE int getByte(const QByteArray &data, int index);
    Q_INVOKABLE int frameLength(const QByteArray &data);

private:
    static constexpr quint8 SOP = 0xAA;
    static constexpr quint8 PROTO_VER = 0x01;
    static constexpr int HEADER_SIZE = 10;
    static constexpr int CRC16_SIZE = 2;
    static constexpr quint8 NEXT_HEADER_GENERIC = 0x01;
    static constexpr int GENERIC_HEADER_SIZE = 6;
    static constexpr quint16 FLAG_RESPONSE = 1u << 0;
    static constexpr quint16 FLAG_ACK_REQUIRED = 1u << 1;
    static constexpr int MIN_FRAME_SIZE = HEADER_SIZE + GENERIC_HEADER_SIZE + CRC16_SIZE;
};

#endif // PROTOCOLENGINE_H
