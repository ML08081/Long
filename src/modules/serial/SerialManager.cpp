#include "modules/serial/SerialManager.h"
#include "modules/serial/Protocol.h"
#include "modules/logger/Logger.h"

#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <poll.h>

namespace patrol {

SerialManager::~SerialManager() { close(); }

static speed_t toSpeed(int baud) {
    switch (baud) {
        case 9600:   return B9600;
        case 19200:  return B19200;
        case 38400:  return B38400;
        case 57600:  return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
        default:     return B115200;
    }
}

bool SerialManager::open(const std::string& device, int baud) {
    close();
    fd_ = ::open(device.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        LOG_ERROR("串口打开失败 %s: %s", device.c_str(), std::strerror(errno));
        return false;
    }

    struct termios tty{};
    if (::tcgetattr(fd_, &tty) != 0) {
        LOG_ERROR("tcgetattr 失败: %s", std::strerror(errno));
        close(); return false;
    }

    speed_t sp = toSpeed(baud);
    ::cfsetispeed(&tty, sp);
    ::cfsetospeed(&tty, sp);
    ::cfmakeraw(&tty);       // 8N1、无回显、无流控、原始模式
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 0;

    if (::tcsetattr(fd_, TCSANOW, &tty) != 0) {
        LOG_ERROR("tcsetattr 失败: %s", std::strerror(errno));
        close(); return false;
    }

    rxBuf_.clear();
    LOG_INFO("串口已打开: %s @ %d baud (F4 协议)", device.c_str(), baud);
    return true;
}

void SerialManager::close() {
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
    rxBuf_.clear();
}

bool SerialManager::sendCommand(int16_t speed, int16_t steering, uint8_t mode) {
    auto frame = serial_proto::buildCommand(speed, steering, mode);
    return sendFrame(frame.data(), frame.size());
}

bool SerialManager::sendFrame(const uint8_t* data, size_t len) {
    if (fd_ < 0) return false;
    size_t sent = 0;
    int stalls = 0;
    while (sent < len) {
        ssize_t n = ::write(fd_, data + sent, len - sent);
        if (n > 0) { sent += static_cast<size_t>(n); stalls = 0; continue; }
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            // 输出缓冲暂满：poll 等待可写(≤50ms)。连续多次仍写不进 → 判串口异常，
            // 放弃本帧返回 false，绝不忙等卡死 30Hz 控制线程(丢一两帧命令有心跳窗口容忍)。
            struct pollfd pfd; pfd.fd = fd_; pfd.events = POLLOUT; pfd.revents = 0;
            ::poll(&pfd, 1, 50);
            if (++stalls > 4) {
                LOG_WARN("串口发送阻塞超时(已发 %zu/%zu)，丢弃本帧", sent, len);
                return false;
            }
            continue;
        }
        LOG_WARN("串口发送失败 (已发 %zu/%zu): %s", sent, len, std::strerror(errno));
        return false;
    }
    return true;
}

void SerialManager::poll() {
    if (fd_ < 0) return;
    uint8_t tmp[256];
    for (;;) {
        ssize_t n = ::read(fd_, tmp, sizeof(tmp));
        if (n > 0) { rxBuf_.insert(rxBuf_.end(), tmp, tmp + n); if (n == sizeof(tmp)) continue; }
        break;   // n<=0 或已读尽
    }
    if (rxBuf_.empty()) return;

    // 防御：持续失步/线路噪声可能让半包无限滞留使 rxBuf_ 无界增长。超上限则丢最旧，
    //   只保留尾部一小段（最大帧长的数倍，足够后续重新对齐），避免内存与延迟膨胀。
    constexpr size_t kRxBufCap = 8192, kRxKeep = 1024;
    if (rxBuf_.size() > kRxBufCap) {
        LOG_WARN("串口 rxBuf 超 %zu 字节（疑似持续失步/噪声），丢弃最旧，保留尾部 %zu",
                 kRxBufCap, kRxKeep);
        rxBuf_.erase(rxBuf_.begin(), rxBuf_.end() - static_cast<long>(kRxKeep));
    }

    using namespace serial_proto;
    size_t off = 0;
    const size_t size = rxBuf_.size();

    while (size - off >= 2) {
        const uint8_t* base = rxBuf_.data() + off;
        if (base[0] != HEADER) { ++off; continue; }

        if (base[1] == THERMAL_MARKER) {
            // ---- 热成像行帧: [AA][5B][row][32*int16 BE][XOR], 固定 68 字节 ----
            if (size - off < static_cast<size_t>(THERMAL_ROW_LEN)) break;   // 半包
            uint8_t crc = xorChecksum(base, THERMAL_ROW_LEN - 1);
            if (crc != base[THERMAL_ROW_LEN - 1]) { ++off; continue; }      // 失步
            uint8_t row = base[2];
            if (row < THERMAL_ROWS) {
                const uint8_t* p = base + 3;
                for (int c = 0; c < THERMAL_COLS; ++c)
                    thermalBuf_[row * THERMAL_COLS + c] = rdI16(p + c * 2);
                // 收到最后一行 -> 一整幅组装完成
                if (row == THERMAL_ROWS - 1 && thermalCb_)
                    thermalCb_(thermalBuf_, THERMAL_COLS, THERMAL_ROWS);
            }
            off += THERMAL_ROW_LEN;
        } else if (base[1] == EXT_MARKER) {
            // ---- 扩展遥测帧: [AA][5A][LEN][payload...][XOR] ----
            if (size - off < 3) break;                 // 需要 LEN
            uint8_t len  = base[2];
            size_t  need = static_cast<size_t>(3) + len + 1;
            if (size - off < need) break;              // 半包
            uint8_t crc = xorChecksum(base, 3 + len);
            if (crc != base[need - 1]) { ++off; continue; }  // 失步

            if (len >= EXT_PAYLOAD_LEN && cb_) {
                const uint8_t* p = base + 3;
                Telemetry t;
                t.speed       = rdI16(p + 0);
                t.steering    = rdI16(p + 2);
                t.mode        = p[4] & 0x03;
                t.dist_cm     = rdU16(p + 5);
                t.enc1        = rdI32(p + 7);
                t.enc2        = rdI32(p + 11);
                t.hasDistance = true;
                cb_(t);
            }
            off += need;
        } else if (base[1] == RAW_THERM_MARKER || base[1] == EEPROM_MARKER) {
            // ---- 原始热成像 / EEPROM 分块: [AA][MARK][idx][cnt][cnt*2 BE][XOR] ----
            if (size - off < 4) break;                 // 需要 idx,cnt
            uint8_t idx = base[2];
            uint8_t cnt = base[3];
            size_t  need = static_cast<size_t>(4) + static_cast<size_t>(cnt) * 2 + 1;
            if (size - off < need) break;              // 半包
            uint8_t crc = xorChecksum(base, 4 + cnt * 2);
            if (crc != base[need - 1]) { ++tdiag_.crcDrop; ++off; continue; }  // 失步

            const bool isEe = (base[1] == EEPROM_MARKER);
            const int total = isEe ? MLX_EE_WORDS : MLX_FRAME_WORDS;
            uint16_t* dst   = isEe ? eeAsm_ : rawAsm_;
            const uint8_t* p = base + 3 + 1;           // 跳过 idx,cnt
            int startWord = static_cast<int>(idx) * MLX_CHUNK_WORDS;
            for (int i = 0; i < cnt; ++i) {
                int w = startWord + i;
                if (w < total) dst[w] = rdU16(p + i * 2);
            }

            if (isEe) {
                ++tdiag_.eeChunks;
                // EEPROM 832 字正好 26 块整除，末块必然触达总长，沿用原判定即可
                if (startWord + cnt >= total) {
                    ++tdiag_.eeFrames;
                    if (eepromCb_) eepromCb_(eeAsm_, MLX_EE_WORDS);
                }
            } else {
                ++tdiag_.rawChunks;
                tdiag_.rawLastIdx = idx;
                if (idx > tdiag_.rawMaxIdx) tdiag_.rawMaxIdx = idx;

                // 新一帧开始：先结算上一帧是否凑齐（没齐就计数，便于定位丢的是哪一块）
                if (idx == 0) {
                    if (rawMask_ != 0 && !rawFired_) ++tdiag_.rawIncomplete;
                    rawMask_ = 0;
                    rawFired_ = false;
                }
                if (idx < 32) rawMask_ |= (1u << idx);

                // 834 字需要 ceil(834/32)=27 块全部到齐才算一整帧（末块只有 2 字）
                constexpr int kNeedChunks = (MLX_FRAME_WORDS + MLX_CHUNK_WORDS - 1) / MLX_CHUNK_WORDS;
                constexpr uint32_t kFullMask = (1u << kNeedChunks) - 1u;
                if (!rawFired_ && (rawMask_ & kFullMask) == kFullMask) {
                    rawFired_ = true;
                    ++tdiag_.rawFrames;
                    if (rawThermalCb_) rawThermalCb_(rawAsm_, MLX_FRAME_WORDS);
                }
            }
            off += need;
        } else if (base[1] == ENV_MARKER) {
            // ---- 环境/安全遥测帧: [AA][5C][LEN][payload...][XOR] ----
            if (size - off < 3) break;                 // 需要 LEN
            uint8_t len  = base[2];
            size_t  need = static_cast<size_t>(3) + len + 1;
            if (size - off < need) break;              // 半包
            uint8_t crc = xorChecksum(base, 3 + len);
            if (crc != base[need - 1]) { ++off; continue; }  // 失步

            if (len >= ENV_PAYLOAD_LEN && envCb_) {
                const uint8_t* p = base + 3;
                EnvData e;
                e.gas_raw = rdU16(p + 0);
                e.vl53_mm = rdU16(p + 2);
                e.temp_c  = static_cast<int8_t>(p[4]);
                e.humi    = p[5];
                e.flags   = p[6];
                e.alarm   = p[7];
                e.valid   = true;
                envCb_(e);
            }
            off += need;
        } else {
            // ---- 旧 7 字节遥测帧: [AA][spd][str][mode|0x80][XOR] ----
            if (size - off < CMD_FRAME_LEN) break;     // 半包
            uint8_t crc = xorChecksum(base, 6);
            if (crc != base[6]) { ++off; continue; }   // 失步
            if (cb_) {
                Telemetry t;
                t.speed       = rdI16(base + 1);
                t.steering    = rdI16(base + 3);
                t.mode        = base[5] & 0x03;
                t.hasDistance = false;
                cb_(t);
            }
            off += CMD_FRAME_LEN;
        }
    }
    if (off > 0) rxBuf_.erase(rxBuf_.begin(), rxBuf_.begin() + static_cast<long>(off));
}

} // namespace patrol
