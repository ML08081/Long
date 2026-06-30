#include "modules/serial/SerialManager.h"
#include "modules/serial/Protocol.h"
#include "modules/logger/Logger.h"

#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/select.h>

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
    ::cfmakeraw(&tty);
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 0;

    if (::tcsetattr(fd_, TCSANOW, &tty) != 0) {
        LOG_ERROR("tcsetattr 失败: %s", std::strerror(errno));
        close(); return false;
    }

    LOG_INFO("串口已打开: %s @ %d baud", device.c_str(), baud);
    return true;
}

void SerialManager::close() {
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
}

bool SerialManager::sendCmd(uint8_t cmd, const uint8_t* payload, uint8_t payloadLen) {
    if (fd_ < 0) return false;
    auto frame = serial_proto::buildFrame(cmd, payload, payloadLen);
    ssize_t n = ::write(fd_, frame.data(), frame.size());
    return n == static_cast<ssize_t>(frame.size());
}

void SerialManager::poll() {
    if (fd_ < 0) return;
    uint8_t tmp[256];
    ssize_t n = ::read(fd_, tmp, sizeof(tmp));
    if (n <= 0) return;
    rxBuf_.insert(rxBuf_.end(), tmp, tmp + n);

    // 解帧：[AA][55][LEN][CMD]...[CRC]
    size_t off = 0;
    while (rxBuf_.size() - off >= 5) {
        if (rxBuf_[off] != serial_proto::SOF1 || rxBuf_[off+1] != serial_proto::SOF2) {
            ++off; continue;
        }
        uint8_t len = rxBuf_[off + 2];
        size_t  need = static_cast<size_t>(len) + 4;   // SOF1+SOF2+LEN + len + CRC
        if (rxBuf_.size() - off < need) break;

        uint8_t crcCalc = serial_proto::crc8(rxBuf_.data() + off + 2, need - 3);
        uint8_t crcRecv = rxBuf_[off + need - 1];
        if (crcCalc != crcRecv) { ++off; continue; }

        uint8_t  cmdByte    = rxBuf_[off + 3];
        const uint8_t* pl   = rxBuf_.data() + off + 4;
        size_t   plLen      = static_cast<size_t>(len) - 1;
        if (rxCb_) rxCb_(cmdByte, pl, plLen);
        off += need;
    }
    if (off > 0) rxBuf_.erase(rxBuf_.begin(), rxBuf_.begin() + static_cast<long>(off));
}

} // namespace patrol