#include "protocol.h"

Protocol::Protocol()
{
}

QByteArray Protocol::packCommand(int16_t speed, int16_t steering, uint8_t mode)
{
    speed=qBound(int16_t(-2000),speed,int16_t(2000));
    steering=qBound(int16_t(-1000),steering,int16_t(1000));
    QByteArray frame;
    frame.resize(7);

    // 帧头
    frame[0] = 0xAA;

    // 速度 (大端序, 先转无符号避免负数右移问题)
    uint16_t uSpeed = static_cast<uint16_t>(speed);
    uint16_t uSteering = static_cast<uint16_t>(steering);
    frame[1] = (uSpeed >> 8) & 0xFF;
    frame[2] = uSpeed & 0xFF;

    // 转向 (大端序)
    frame[3] = (uSteering >> 8) & 0xFF;
    frame[4] = uSteering & 0xFF;

    // 模式
    frame[5] = mode;

    // 校验和
    frame[6] = calculateChecksum(reinterpret_cast<const uint8_t*>(frame.constData()), 6);

    return frame;
}

// ─── 校验和（重载：QByteArray 版本 → 委托给指针版本）───────────────
uint8_t Protocol::calculateChecksum(const QByteArray& data)
{
    return calculateChecksum(reinterpret_cast<const uint8_t*>(data.constData()), data.size());
}

// ─── 校验和（重载：指针+长度版本）──────────────────────────────────
uint8_t Protocol::calculateChecksum(const uint8_t* data, int length)
{
    uint8_t checksum = 0;
    for (int i = 0; i < length; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

// ─── 遥测帧解包 ───────────────────────────────────────────────────
bool Protocol::unpackFrame(const QByteArray& frame, TelemetryData& data)
{
    if (frame.size() < FRAME_LENGTH)
        return false;

    if (static_cast<uint8_t>(frame[0]) != FRAME_HEADER)
        return false;

    // 校验和验证
    uint8_t checksum = calculateChecksum(
        reinterpret_cast<const uint8_t*>(frame.constData()), 6);
    if (checksum != static_cast<uint8_t>(frame[6]))
        return false;

    // 大端序解析 (与 packCommand 保持一致)
    uint16_t uSpeed    = (static_cast<uint16_t>(static_cast<uint8_t>(frame[1])) << 8)
                        | static_cast<uint8_t>(frame[2]);
    uint16_t uSteering = (static_cast<uint16_t>(static_cast<uint8_t>(frame[3])) << 8)
                        | static_cast<uint8_t>(frame[4]);

    data.speed    = static_cast<int16_t>(uSpeed);
    data.steering = static_cast<int16_t>(uSteering);
    data.mode     = static_cast<uint8_t>(frame[5]);

    return true;
}

QString Protocol::frameToHexString(const QByteArray& frame)
{
    QString hexStr;
    for (int i = 0; i < frame.size(); i++) {
        hexStr += QString("%1 ").arg(static_cast<uint8_t>(frame[i]), 2, 16, QChar('0')).toUpper();
    }
    return hexStr.trimmed();
}
