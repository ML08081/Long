#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QByteArray>
#include <QString>

#define FRAME_HEADER    0xAA
#define FRAME_LENGTH    7

// 遥测回传数据（STM32 → PC）
struct TelemetryData {
    int16_t speed;      // -1500~1500
    int16_t steering;   // -1000~1000（已由脉宽转换）
    uint8_t mode;       // 0-3
};

class Protocol
{
public:
    Protocol();

    QByteArray packCommand(int16_t speed, int16_t steering, uint8_t mode);
    QString     frameToHexString(const QByteArray& frame);

    // 遥测帧解包（静态方法，无需实例化）
    static bool unpackFrame(const QByteArray& frame, TelemetryData& data);
    static uint8_t calculateChecksum(const uint8_t* data, int length);

private:
    static uint8_t calculateChecksum(const QByteArray& data);
};

#endif // PROTOCOL_H
