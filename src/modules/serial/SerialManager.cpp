#include "modules/serial/SerialManager.h"
#include "modules/serial/Protocol.h"
#include "modules/logger/Logger.h"

#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

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
    if (fd_ < 0) return false;
    auto frame = serial_proto::buildCommand(speed, steering, mode);
    size_t sent = 0;
    while (sent < frame.size()) {
        ssize_t n = ::write(fd_, frame.data() + sent, frame.size() - sent);
        if (n > 0) { sent += static_cast<size_t>(n); continue; }
        if (n < 0 && (errno == EINTR || errno == EAGAIN)) continue;
        LOG_WARN("串口发送失败 (已发 %zu/%zu): %s", sent, frame.size(), std::strerror(errno));
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

    using namespace serial_proto;
    size_t off = 0;
    const size_t size = rxBuf_.size();

    while (size - off >= 2) {
        const uint8_t* base = rxBuf_.data() + off;
        if (base[0] != HEADER) { ++off; continue; }

        if (base[1] == EXT_MARKER) {
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
