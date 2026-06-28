#ifndef FRAMEPROCESSOR_H
#define FRAMEPROCESSOR_H

#include <QByteArray>
#include <QMutex>
#include <QObject>
#include <QVariantMap>

class FrameProcessor : public QObject
{
    Q_OBJECT

public:
    explicit FrameProcessor(QObject *parent = nullptr);

    void feedData(const QByteArray &data);
    void clear();

    int framesExtracted() const { return m_framesExtracted; }
    int bytesProcessed() const { return m_bytesProcessed; }
    int bufferUsed() const { return m_buffer.size(); }

signals:
    void frameReceived(const QVariantMap &frame);

private:
    struct ParseResult {
        QVariantMap frame;
        int totalLength = 0;
        bool ok = false;
        bool crcValid = false;
    };

    ParseResult tryParseFrame(int pos);

    static quint8 bcc(const QByteArray &data);
    static quint16 crc16(const QByteArray &data);

    QByteArray m_buffer;
    QMutex m_mutex;

    int m_framesExtracted = 0;
    int m_bytesProcessed = 0;

    static constexpr quint8 SOF = 0xAA;
    static constexpr int HEADER_SIZE = 11;
    static constexpr int CRC16_SIZE = 2;
    static constexpr int MIN_FRAME_SIZE = HEADER_SIZE + CRC16_SIZE;

    static constexpr quint16 VER_LEN_LEN_MASK  = 0x3FF;
    static constexpr quint8 CMD_TYPE_IS_ACK_MASK = 0x01;
};

#endif // FRAMEPROCESSOR_H
