#include "modules/can/CanManager.h"
#include "modules/logger/Logger.h"

#include <algorithm>
#include <chrono>
#include <cstring>

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/error.h>
#include <linux/can/raw.h>

namespace patrol {

namespace {
uint32_t nowMs() {
    using namespace std::chrono;
    return static_cast<uint32_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

// 当前单调毫秒到某截止时刻的剩余（用于 SDO 超时循环）
uint32_t remainMs(uint32_t deadline) {
    uint32_t now = nowMs();
    return (deadline > now) ? (deadline - now) : 0;
}
} // namespace

CanManager::~CanManager() { close(); }

bool CanManager::open(const std::string& ifname) {
    close();
    ifname_ = ifname;

    fd_ = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (fd_ < 0) {
        LOG_ERROR("CanManager: 创建 CAN socket 失败(%s)——内核缺 can-raw？", std::strerror(errno));
        return false;
    }

    struct ifreq ifr{};
    std::strncpy(ifr.ifr_name, ifname.c_str(), IFNAMSIZ - 1);
    if (::ioctl(fd_, SIOCGIFINDEX, &ifr) < 0) {
        LOG_ERROR("CanManager: 找不到 CAN 接口 %s(%s)——can0 是否已 up？",
                  ifname.c_str(), std::strerror(errno));
        ::close(fd_); fd_ = -1;
        return false;
    }

    struct sockaddr_can addr{};
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (::bind(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        LOG_ERROR("CanManager: bind %s 失败(%s)", ifname.c_str(), std::strerror(errno));
        ::close(fd_); fd_ = -1;
        return false;
    }

    // 只收 F4 上行的 6 个 COB-ID，其余硬件/内核层丢弃，减轻 poll 负担。
    struct can_filter rf[6] = {
        { can_proto::COBID_TELE_CORE, CAN_SFF_MASK },
        { can_proto::COBID_TELE_ENC,  CAN_SFF_MASK },
        { can_proto::COBID_ENV,       CAN_SFF_MASK },
        { can_proto::COBID_EVT,       CAN_SFF_MASK },
        { can_proto::COBID_SDO_TX,    CAN_SFF_MASK },
        { can_proto::COBID_HEARTBEAT, CAN_SFF_MASK },
    };
    if (::setsockopt(fd_, SOL_CAN_RAW, CAN_RAW_FILTER, rf, sizeof(rf)) < 0)
        LOG_WARN("CanManager: 设置接收过滤器失败(%s)，将收到全部帧", std::strerror(errno));

    // ★ 同时收错误帧 —— 量化 EMI 改善的核心：TEC/REC/bus-off 全靠它。
    can_err_mask_t emask = CAN_ERR_TX_TIMEOUT | CAN_ERR_BUSOFF |
                           CAN_ERR_CRTL | CAN_ERR_PROT;
    if (::setsockopt(fd_, SOL_CAN_RAW, CAN_RAW_ERR_FILTER, &emask, sizeof(emask)) < 0)
        LOG_WARN("CanManager: 设置错误帧过滤失败(%s)，EMI 诊断不可用", std::strerror(errno));

    // 非阻塞：绝不阻塞 ~30Hz 控制线程。
    int fl = ::fcntl(fd_, F_GETFL, 0);
    ::fcntl(fd_, F_SETFL, fl | O_NONBLOCK);

    teleAsm_ = serial_proto::Telemetry{};
    haveEnc_ = false;
    LOG_INFO("CanManager: %s 就绪（SocketCAN, 500kbps, 收错误帧诊断已开）", ifname.c_str());
    return true;
}

void CanManager::close() {
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
}

bool CanManager::reopen() {
    LOG_WARN("CanManager: %s 上行断流，重建 socket 自恢复...", ifname_.c_str());
    std::string ifn = ifname_;
    close();
    return open(ifn);
}

// ---- 底层发送 -----------------------------------------------------------
bool CanManager::sendRaw(uint32_t cobId, const uint8_t* d, uint8_t len) {
    if (fd_ < 0) return false;
    if (len > 8) len = 8;
    struct can_frame fr{};
    fr.can_id  = cobId & CAN_SFF_MASK;   // 标准帧 11-bit
    fr.can_dlc = len;
    if (d && len) std::memcpy(fr.data, d, len);
    ssize_t n = ::write(fd_, &fr, sizeof(fr));
    if (n != static_cast<ssize_t>(sizeof(fr))) {
        // 邮箱满/总线无 ACK 时会 EAGAIN；命令是周期帧，丢一帧靠下周期补，不阻塞。
        return false;
    }
    ++h_.tx;
    return true;
}

bool CanManager::sendCommand(int16_t speed, int16_t steering, uint8_t mode) {
    speed    = std::max<int16_t>(-1000, std::min<int16_t>(1000, speed));
    steering = std::max<int16_t>(-1000, std::min<int16_t>(1000, steering));
    // 载荷大端（与 F4 can_link 一致）：spd i16, str i16, mode u8
    uint8_t p[5];
    p[0] = static_cast<uint8_t>((static_cast<uint16_t>(speed)    >> 8) & 0xFF);
    p[1] = static_cast<uint8_t>( static_cast<uint16_t>(speed)          & 0xFF);
    p[2] = static_cast<uint8_t>((static_cast<uint16_t>(steering) >> 8) & 0xFF);
    p[3] = static_cast<uint8_t>( static_cast<uint16_t>(steering)       & 0xFF);
    p[4] = static_cast<uint8_t>(mode & 0x03);
    return sendRaw(can_proto::COBID_CMD, p, 5);
}

bool CanManager::sendPatrolCmd(uint8_t cmd, uint8_t flags) {
    uint8_t p[3] = { can_proto::SUB_PATROL_CMD, cmd, flags };
    return sendRaw(can_proto::COBID_PATROL_RX, p, 3);
}

bool CanManager::sendPatrolAct(uint8_t pointId, uint8_t action, uint8_t resume) {
    uint8_t p[4] = { can_proto::SUB_PATROL_ACT, pointId, action, resume };
    return sendRaw(can_proto::COBID_PATROL_RX, p, 4);
}

bool CanManager::sendAvoidPolicy(uint8_t policy, uint8_t obsClass) {
    uint8_t p[3] = { can_proto::SUB_AVOID_POLICY, policy, obsClass };
    return sendRaw(can_proto::COBID_PATROL_RX, p, 3);
}

bool CanManager::sendPidTest(uint8_t mode, int16_t left, int16_t right, uint16_t durationMs) {
    uint8_t p[8];
    p[0] = can_proto::SUB_PID_TEST;
    p[1] = mode;
    p[2] = static_cast<uint8_t>((static_cast<uint16_t>(left)  >> 8) & 0xFF);
    p[3] = static_cast<uint8_t>( static_cast<uint16_t>(left)        & 0xFF);
    p[4] = static_cast<uint8_t>((static_cast<uint16_t>(right) >> 8) & 0xFF);
    p[5] = static_cast<uint8_t>( static_cast<uint16_t>(right)       & 0xFF);
    p[6] = static_cast<uint8_t>((durationMs >> 8) & 0xFF);
    p[7] = static_cast<uint8_t>( durationMs       & 0xFF);
    return sendRaw(can_proto::COBID_PATROL_RX, p, 8);
}

// ---- 上行帧分发 ---------------------------------------------------------
void CanManager::dispatch(uint32_t cobId, const uint8_t* d, uint8_t dlc) {
    using namespace serial_proto;
    h_.lastRxMs = nowMs();
    ++h_.rx;

    switch (cobId) {
        case can_proto::COBID_TELE_CORE: {          // 0x181: spd i16, str i16, mode u8, dist u16, line u8
            if (dlc < 7) break;
            teleAsm_.speed    = rdI16(d);
            teleAsm_.steering = rdI16(d + 2);
            teleAsm_.mode     = d[4];
            uint16_t dist = rdU16(d + 5);
            // 无目标哨兵归一为 0（下游用 "0=无效/超量程" 语义）
            teleAsm_.dist_cm = (dist == can_proto::TELE_NO_TARGET_CM) ? 0 : dist;
            teleAsm_.hasDistance = true;              // CAN 遥测始终携带距离/编码器
            // byte7 = 循迹位域（本轮龙芯不消费，F4 本地快环用；预留给上位机显示）
            if (cb_) cb_(teleAsm_);                   // 用最近一次 enc 组装回调
            break;
        }
        case can_proto::COBID_TELE_ENC: {           // 0x281: enc1 i32, enc2 i32
            if (dlc < 8) break;
            teleAsm_.enc1 = rdI32(d);
            teleAsm_.enc2 = rdI32(d + 4);
            haveEnc_ = true;
            break;
        }
        case can_proto::COBID_ENV: {                // 0x381: gas u16, vl53 u16, temp i8, humi u8, flags u8, alarm u8
            if (dlc < 8) break;
            EnvData e;
            e.gas_raw = rdU16(d);
            e.vl53_mm = rdU16(d + 2);
            e.temp_c  = static_cast<int8_t>(d[4]);
            e.humi    = d[5];
            e.flags   = d[6];
            e.alarm   = d[7];
            e.valid   = true;
            if (envCb_) envCb_(e);
            break;
        }
        case can_proto::COBID_EVT: {                // 0x481: byte0=sub_id 复用
            if (dlc < 1) break;
            uint8_t sub = d[0];
            if (sub == can_proto::SUB_PATROL_EVT && dlc >= 4) {
                can_proto::PatrolEvent ev;
                ev.event = d[1]; ev.pointSeq = d[2]; ev.state = d[3];
                if (patrolEvtCb_) patrolEvtCb_(ev);
            } else if (sub == can_proto::SUB_AVOID_PROFILE && dlc >= 7) {
                can_proto::AvoidProfile pr;
                pr.distL = rdU16(d + 1); pr.distC = rdU16(d + 3); pr.distR = rdU16(d + 5);
                if (avoidProfCb_) avoidProfCb_(pr);
            } else if (sub == can_proto::SUB_PIDT && dlc >= 3) {
                uint8_t part = d[1];
                if (part < 4) {
                    if (part == 0) pidMask_ = 0;
                    size_t off = static_cast<size_t>(part) * 6;
                    size_t cnt = std::min<size_t>(dlc - 2, serial_proto::PID_TELE_PAYLOAD - off);
                    std::memcpy(pidAsm_ + off, d + 2, cnt);
                    pidMask_ |= static_cast<uint8_t>(1u << part);
                    if (pidMask_ == 0x0F && pidTeleCb_) {
                        serial_proto::PidTele t;
                        const uint8_t* p = pidAsm_;
                        t.seq   = rdU16(p + 0);
                        t.flags = p[2];
                        t.targL = rdI16(p + 3);  t.measL = rdI16(p + 5);  t.outL = rdI16(p + 7);
                        t.targR = rdI16(p + 9);  t.measR = rdI16(p + 11); t.outR = rdI16(p + 13);
                        t.enc1  = rdI32(p + 15);
                        t.enc2  = rdI32(p + 19);
                        pidTeleCb_(t);
                        pidMask_ = 0;
                    }
                }
            }
            break;
        }
        case can_proto::COBID_HEARTBEAT: {          // 0x701: state u8
            if (dlc < 1) break;
            can_proto::Heartbeat hb;
            hb.state = d[0];
            if (hbCb_) hbCb_(hb);
            break;
        }
        default:
            break;   // 0x581 SDO 响应由 SDO 内部读循环处理，不进这里
    }
}

void CanManager::handleErrorFrame(const uint8_t* d, uint8_t dlc) {
    ++h_.errFrames;
    // data[6] = TEC, data[7] = REC（Linux SocketCAN 错误帧约定）
    if (dlc >= 8) { h_.tec = d[6]; h_.rec = d[7]; }
}

// ---- 轮询 ---------------------------------------------------------------
void CanManager::poll() {
    if (fd_ < 0) return;
    struct can_frame fr;
    // 取尽当前所有帧（非阻塞，EAGAIN 即退出）
    for (int guard = 0; guard < 256; ++guard) {
        ssize_t n = ::read(fd_, &fr, sizeof(fr));
        if (n < static_cast<ssize_t>(sizeof(fr))) break;   // 无更多帧 / 半帧
        if (fr.can_id & CAN_ERR_FLAG) {                    // 错误帧
            handleErrorFrame(fr.data, fr.can_dlc);
            if (fr.can_id & CAN_ERR_BUSOFF) ++h_.busOff;
            continue;
        }
        uint32_t cobId = fr.can_id & CAN_SFF_MASK;
        if (cobId == can_proto::COBID_SDO_TX) continue;    // 无挂起 SDO 时的迟到响应，丢弃
        dispatch(cobId, fr.data, fr.can_dlc);
    }
}

// ==========================================================================
//  类 SDO 客户端（同步，只在配置期调用）
// ==========================================================================

// 等一帧 0x581 响应；期间收到的过程数据/事件帧照常 dispatch（不丢遥测）。
bool CanManager::waitSdoResponse(uint8_t out[8], uint32_t timeoutMs) {
    uint32_t deadline = nowMs() + timeoutMs;
    struct can_frame fr;
    for (;;) {
        ssize_t n = ::read(fd_, &fr, sizeof(fr));
        if (n == static_cast<ssize_t>(sizeof(fr))) {
            if (fr.can_id & CAN_ERR_FLAG) {
                handleErrorFrame(fr.data, fr.can_dlc);
                if (fr.can_id & CAN_ERR_BUSOFF) ++h_.busOff;
                continue;
            }
            uint32_t cobId = fr.can_id & CAN_SFF_MASK;
            if (cobId == can_proto::COBID_SDO_TX) {
                std::memcpy(out, fr.data, 8);
                return true;
            }
            dispatch(cobId, fr.data, fr.can_dlc);   // 顺带处理遥测，避免 SDO 期间丢帧
            continue;
        }
        // 无帧：poll 阻塞等到可读或超时（去掉 1ms 忙等轮询，省 CPU、响应更快）。
        uint32_t rem = remainMs(deadline);
        if (rem == 0) return false;
        struct pollfd pfd{ fd_, POLLIN, 0 };
        if (::poll(&pfd, 1, static_cast<int>(rem)) <= 0) return false;   // 超时/错误
    }
}

bool CanManager::sdoWrite(uint16_t idx, uint8_t sub, const void* d, size_t len,
                          uint32_t* abortCode) {
    if (abortCode) *abortCode = 0;
    if (fd_ < 0 || len == 0 || len > 4) return false;   // 本函数只做 expedited(≤4B)
    static const uint8_t ccs[5] = { 0, can_proto::SDO_CCS_WRITE_1B,
                                    can_proto::SDO_CCS_WRITE_2B,
                                    can_proto::SDO_CCS_WRITE_3B,
                                    can_proto::SDO_CCS_WRITE_4B };
    uint8_t req[8] = {0};
    req[0] = ccs[len];
    can_proto::wrU16le(req + 1, idx);   // index 小端
    req[3] = sub;
    std::memcpy(req + 4, d, len);       // 数据小端（调用方按 F4 端序准备）
    if (!sendRaw(can_proto::COBID_SDO_RX, req, 8)) return false;

    uint8_t rsp[8];
    if (!waitSdoResponse(rsp, 500)) {
        if (abortCode) *abortCode = can_proto::SDO_ABT_TIMEOUT;
        LOG_WARN("SDO 写超时: idx=0x%04X sub=%u", idx, sub);
        return false;
    }
    if (rsp[0] == can_proto::SDO_ABORT) {
        if (abortCode) *abortCode = can_proto::rdU32le(rsp + 4);
        LOG_WARN("SDO 写 abort: idx=0x%04X sub=%u code=0x%08X", idx, sub,
                 can_proto::rdU32le(rsp + 4));
        return false;
    }
    return rsp[0] == can_proto::SDO_SCS_WRITE_ACK;
}

bool CanManager::sdoRead(uint16_t idx, uint8_t sub, void* out, size_t* len,
                         uint32_t* abortCode) {
    if (abortCode) *abortCode = 0;
    if (fd_ < 0) return false;
    uint8_t req[8] = {0};
    req[0] = can_proto::SDO_CCS_READ;
    can_proto::wrU16le(req + 1, idx);
    req[3] = sub;
    if (!sendRaw(can_proto::COBID_SDO_RX, req, 8)) return false;

    uint8_t rsp[8];
    if (!waitSdoResponse(rsp, 500)) {
        if (abortCode) *abortCode = can_proto::SDO_ABT_TIMEOUT;
        return false;
    }
    if (rsp[0] == can_proto::SDO_ABORT) {
        if (abortCode) *abortCode = can_proto::rdU32le(rsp + 4);
        return false;
    }
    // expedited 读响应 0x4F/0x4B/0x47/0x43 → 数据字节数 = 4 - ((cs>>2)&0x3)
    if ((rsp[0] & 0xE0) != 0x40) return false;
    size_t n = 4 - ((rsp[0] >> 2) & 0x03);
    if (len) { if (*len < n) n = *len; *len = n; }
    if (out && n) std::memcpy(out, rsp + 4, n);
    return true;
}

// 分段下发（写初始化 0x21 + 若干写分段 0x00/0x10）。用于路线表 domain。
bool CanManager::sdoWriteDomain(uint16_t idx, uint8_t sub,
                                const std::vector<uint8_t>& blob, uint32_t* abortCode) {
    if (abortCode) *abortCode = 0;
    if (fd_ < 0) return false;

    // 1) 初始化：命令 0x21，byte4-7 = 总长度 u32(小端)
    uint8_t req[8] = {0};
    req[0] = can_proto::SDO_CCS_WRITE_INIT;
    can_proto::wrU16le(req + 1, idx);
    req[3] = sub;
    can_proto::wrU32le(req + 4, static_cast<uint32_t>(blob.size()));
    if (!sendRaw(can_proto::COBID_SDO_RX, req, 8)) return false;
    uint8_t rsp[8];
    if (!waitSdoResponse(rsp, 500)) {
        if (abortCode) *abortCode = can_proto::SDO_ABT_TIMEOUT;
        return false;
    }
    if (rsp[0] == can_proto::SDO_ABORT) {
        if (abortCode) *abortCode = can_proto::rdU32le(rsp + 4);
        return false;
    }

    // 2) 逐段发：每段最多 7 字节数据；bit4=toggle, bit1-3=未用字节数, bit0=末段。
    size_t off = 0;
    bool   toggle = false;
    while (off < blob.size()) {
        size_t chunk = std::min<size_t>(7, blob.size() - off);
        bool   last  = (off + chunk >= blob.size());
        uint8_t seg[8] = {0};
        uint8_t cs = 0;
        if (toggle) cs |= 0x10;
        cs |= static_cast<uint8_t>((7 - chunk) << 1);   // 未用字节数
        if (last) cs |= 0x01;
        seg[0] = cs;
        std::memcpy(seg + 1, blob.data() + off, chunk);
        if (!sendRaw(can_proto::COBID_SDO_RX, seg, 8)) return false;
        if (!waitSdoResponse(rsp, 500)) {
            if (abortCode) *abortCode = can_proto::SDO_ABT_TIMEOUT;
            return false;
        }
        if (rsp[0] == can_proto::SDO_ABORT) {
            if (abortCode) *abortCode = can_proto::rdU32le(rsp + 4);
            return false;
        }
        // 服务端分段写确认 0x20/0x30（toggle 在 bit4）
        if ((rsp[0] & 0xE0) != 0x20) return false;
        off += chunk;
        toggle = !toggle;
    }
    return true;
}

} // namespace patrol
