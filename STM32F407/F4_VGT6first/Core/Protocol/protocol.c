// Core/Protocol/protocol.c
#include "protocol.h"

uint8_t Protocol_CalculateChecksum(const uint8_t* data, uint8_t length)
{
    uint8_t checksum = 0;
    for (uint8_t i = 0; i < length; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

void Protocol_PackFrame(const ControlData* data, uint8_t* frame)
{
    frame[0] = FRAME_HEADER;
    // 大端序打包 (high byte first)
    frame[1] = (uint8_t)((data->speed >> 8) & 0xFF);
    frame[2] = (uint8_t)(data->speed & 0xFF);
    frame[3] = (uint8_t)((data->steering >> 8) & 0xFF);
    frame[4] = (uint8_t)(data->steering & 0xFF);
    // 遥测帧标记：mode 字节 bit7 置 1，PC 端用此区分命令回环与遥测
    // 旧 PC 端无此标记时也能正常解析（仅丢掉 bit7 不影响 mode 值）
    frame[5] = data->mode | TELEMETRY_FLAG;
    frame[6] = Protocol_CalculateChecksum(frame, 6);  // XOR bytes 0~5
}

uint8_t Protocol_UnpackCommandFrame(const uint8_t* frame, ControlData* data)
{
    if (frame[0] != FRAME_HEADER) return 0;

    /* 拒绝遥测帧（bit7=1）：可能是 USART3 TX 经 BT 模块回环 */
    if (frame[5] & TELEMETRY_FLAG) return 0;

    uint8_t checksum = Protocol_CalculateChecksum(frame, 6);
    if (checksum != frame[6]) return 0;

    data->speed = (int16_t)((frame[1] << 8) | frame[2]);
    data->steering = (int16_t)((frame[3] << 8) | frame[4]);
    data->mode = frame[5] & 0x03;

    return 1;
}

uint8_t Protocol_UnpackFrame(const uint8_t* frame, ControlData* data)
{
    return Protocol_UnpackCommandFrame(frame, data);
}

uint8_t Protocol_PackTelemetryEx(int16_t speed, int16_t steering, uint8_t mode,
                                 uint16_t dist_cm, int32_t enc1, int32_t enc2,
                                 uint8_t* frame)
{
    frame[0] = FRAME_HEADER;
    frame[1] = FRAME_EXT_MARKER;
    frame[2] = FRAME_EXT_PAYLOAD;                       // LEN = 15
    // payload（大端序，high byte first）
    frame[3]  = (uint8_t)((speed    >> 8) & 0xFF);
    frame[4]  = (uint8_t)( speed          & 0xFF);
    frame[5]  = (uint8_t)((steering >> 8) & 0xFF);
    frame[6]  = (uint8_t)( steering       & 0xFF);
    frame[7]  = (uint8_t)(mode & 0x03);
    frame[8]  = (uint8_t)((dist_cm  >> 8) & 0xFF);
    frame[9]  = (uint8_t)( dist_cm        & 0xFF);
    frame[10] = (uint8_t)(((uint32_t)enc1 >> 24) & 0xFF);
    frame[11] = (uint8_t)(((uint32_t)enc1 >> 16) & 0xFF);
    frame[12] = (uint8_t)(((uint32_t)enc1 >>  8) & 0xFF);
    frame[13] = (uint8_t)(((uint32_t)enc1      ) & 0xFF);
    frame[14] = (uint8_t)(((uint32_t)enc2 >> 24) & 0xFF);
    frame[15] = (uint8_t)(((uint32_t)enc2 >> 16) & 0xFF);
    frame[16] = (uint8_t)(((uint32_t)enc2 >>  8) & 0xFF);
    frame[17] = (uint8_t)(((uint32_t)enc2      ) & 0xFF);
    frame[18] = Protocol_CalculateChecksum(frame, 18);  // XOR bytes 0~17
    return FRAME_EXT_LENGTH;                             // = 19
}

uint8_t Protocol_PackEnvFrame(uint16_t gas_raw, uint16_t vl53_mm,
                              int8_t temp, uint8_t humi,
                              uint8_t flags, uint8_t alarm, uint8_t* frame)
{
    frame[0] = FRAME_HEADER;
    frame[1] = FRAME_ENV_MARKER;
    frame[2] = FRAME_ENV_PAYLOAD;                       // LEN = 8
    // payload（大端序，high byte first）
    frame[3]  = (uint8_t)((gas_raw >> 8) & 0xFF);
    frame[4]  = (uint8_t)( gas_raw       & 0xFF);
    frame[5]  = (uint8_t)((vl53_mm >> 8) & 0xFF);
    frame[6]  = (uint8_t)( vl53_mm       & 0xFF);
    frame[7]  = (uint8_t)temp;                          // int8（-40~80°C 范围内）
    frame[8]  = humi;
    frame[9]  = flags;
    frame[10] = alarm;
    frame[11] = Protocol_CalculateChecksum(frame, 11);  // XOR bytes 0~10
    return FRAME_ENV_LENGTH;                             // = 12
}

uint8_t Protocol_PackThermalRow(uint8_t rowIdx, const int16_t* rowTemps, uint8_t* frame)
{
    frame[0] = FRAME_HEADER;            // 0xAA
    frame[1] = FRAME_THERMAL_MARKER;    // 0x5B
    frame[2] = rowIdx;                  // 0~23
    for (int i = 0; i < FRAME_THERMAL_COLS; i++) {
        frame[3 + i * 2]     = (uint8_t)((rowTemps[i] >> 8) & 0xFF);  // 大端
        frame[3 + i * 2 + 1] = (uint8_t)( rowTemps[i]       & 0xFF);
    }
    frame[FRAME_THERMAL_ROW_LEN - 1] =
        Protocol_CalculateChecksum(frame, FRAME_THERMAL_ROW_LEN - 1);
    return FRAME_THERMAL_ROW_LEN;       // = 68
}

/* ---------- PID 在线调试帧 ---------- */

uint8_t Protocol_UnpackPidParam(const uint8_t* frame, PidParams* p)
{
    if (frame[0] != FRAME_HEADER || frame[1] != FRAME_PIDPARAM_MARKER ||
        frame[2] != FRAME_PIDPARAM_PAYLOAD)
        return 0;
    /* 大端 u32 ×1000 -> float */
    uint32_t kp = ((uint32_t)frame[3]  << 24) | ((uint32_t)frame[4]  << 16) |
                  ((uint32_t)frame[5]  <<  8) |  (uint32_t)frame[6];
    uint32_t ki = ((uint32_t)frame[7]  << 24) | ((uint32_t)frame[8]  << 16) |
                  ((uint32_t)frame[9]  <<  8) |  (uint32_t)frame[10];
    uint32_t kd = ((uint32_t)frame[11] << 24) | ((uint32_t)frame[12] << 16) |
                  ((uint32_t)frame[13] <<  8) |  (uint32_t)frame[14];
    uint16_t md = (uint16_t)((frame[15] << 8) | frame[16]);
    p->kp = (float)kp / 1000.0f;
    p->ki = (float)ki / 1000.0f;
    p->kd = (float)kd / 1000.0f;
    if (md > 0) p->max_delta = (float)md;   /* 0 = 保持现值 */
    p->closed_loop = (frame[17] & PID_FLAG_CLOSED_LOOP) ? 1 : 0;
    return 1;
}

uint8_t Protocol_UnpackPidTest(const uint8_t* frame, PidTestCmd* t)
{
    if (frame[0] != FRAME_HEADER || frame[1] != FRAME_PIDTEST_MARKER ||
        frame[2] != FRAME_PIDTEST_PAYLOAD)
        return 0;
    t->mode  = frame[3];
    t->left  = (int16_t)((frame[4] << 8) | frame[5]);
    t->right = (int16_t)((frame[6] << 8) | frame[7]);
    uint16_t dur = (uint16_t)((frame[8] << 8) | frame[9]);
    if (dur == 0)               dur = PID_TEST_DEFAULT_MS;
    if (dur > PID_TEST_MAX_MS)  dur = PID_TEST_MAX_MS;   /* 安全上限 */
    t->duration_ms = dur;
    if (t->mode > PID_TEST_CLOSED) return 0;
    return 1;
}

uint8_t Protocol_PackPidTele(uint16_t seq, uint8_t flags,
                             int16_t targL, int16_t measL, int16_t outL,
                             int16_t targR, int16_t measR, int16_t outR,
                             int32_t enc1, int32_t enc2, uint8_t* frame)
{
    frame[0] = FRAME_HEADER;
    frame[1] = FRAME_PIDTELE_MARKER;
    frame[2] = FRAME_PIDTELE_PAYLOAD;                    /* LEN = 23 */
    frame[3]  = (uint8_t)((seq   >> 8) & 0xFF);
    frame[4]  = (uint8_t)( seq         & 0xFF);
    frame[5]  = flags;
    frame[6]  = (uint8_t)((targL >> 8) & 0xFF); frame[7]  = (uint8_t)(targL & 0xFF);
    frame[8]  = (uint8_t)((measL >> 8) & 0xFF); frame[9]  = (uint8_t)(measL & 0xFF);
    frame[10] = (uint8_t)((outL  >> 8) & 0xFF); frame[11] = (uint8_t)(outL  & 0xFF);
    frame[12] = (uint8_t)((targR >> 8) & 0xFF); frame[13] = (uint8_t)(targR & 0xFF);
    frame[14] = (uint8_t)((measR >> 8) & 0xFF); frame[15] = (uint8_t)(measR & 0xFF);
    frame[16] = (uint8_t)((outR  >> 8) & 0xFF); frame[17] = (uint8_t)(outR  & 0xFF);
    frame[18] = (uint8_t)(((uint32_t)enc1 >> 24) & 0xFF);
    frame[19] = (uint8_t)(((uint32_t)enc1 >> 16) & 0xFF);
    frame[20] = (uint8_t)(((uint32_t)enc1 >>  8) & 0xFF);
    frame[21] = (uint8_t)(((uint32_t)enc1      ) & 0xFF);
    frame[22] = (uint8_t)(((uint32_t)enc2 >> 24) & 0xFF);
    frame[23] = (uint8_t)(((uint32_t)enc2 >> 16) & 0xFF);
    frame[24] = (uint8_t)(((uint32_t)enc2 >>  8) & 0xFF);
    frame[25] = (uint8_t)(((uint32_t)enc2      ) & 0xFF);
    frame[26] = Protocol_CalculateChecksum(frame, 26);   /* XOR bytes 0~25 */
    return FRAME_PIDTELE_LENGTH;                          /* = 27 */
}

uint8_t Protocol_PackMlxChunk(uint8_t marker, uint8_t idx,
                              const uint16_t* words, uint8_t cnt, uint8_t* frame)
{
    frame[0] = FRAME_HEADER;            // 0xAA
    frame[1] = marker;                  // 0x5D 原始帧 / 0x5E EEPROM
    frame[2] = idx;                     // 分块序号
    frame[3] = cnt;                     // 本块字数
    for (int i = 0; i < cnt; i++) {
        frame[4 + i * 2]     = (uint8_t)((words[i] >> 8) & 0xFF);   // 大端
        frame[4 + i * 2 + 1] = (uint8_t)( words[i]       & 0xFF);
    }
    uint8_t len = (uint8_t)(4 + cnt * 2);
    frame[len] = Protocol_CalculateChecksum(frame, len);
    return (uint8_t)(len + 1);
}