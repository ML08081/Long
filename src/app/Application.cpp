#include "app/Application.h"
#include "modules/logger/Logger.h"
#include "modules/network/FrameProtocol.h"
#include "version.h"

#include <chrono>
#include <thread>
#include <vector>
#include <deque>
#include <condition_variable>
#include <mutex>
#include <atomic>
#include <string>

#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>
#include <cstdio>

namespace patrol {

namespace {
uint32_t nowMs() {
    using namespace std::chrono;
    return static_cast<uint32_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

// ── 单连接发送会话（方案B 第一阶段）───────────────────────────────────────────
//   生产者(视频采集/上报线程)投递帧 → 唯一发送线程消费并串行写 socket(线程安全)。
//   smallQ: 传感器/状态/PID遥测/热成像(保序、优先发)；video: 视频槽(新盖旧=拥塞天然丢旧帧)。
struct TxFrame { uint8_t type; std::vector<uint8_t> data; };

struct ClientSession {
    int fd = -1;
    std::atomic<bool> alive{true};

    std::mutex mtx;
    std::condition_variable cv;
    std::deque<TxFrame> smallQ;
    std::vector<uint8_t> video;
    bool haveVideo = false;

    static constexpr size_t kSmallQMax = 128;

    void pushSmall(uint8_t type, const uint8_t* p, size_t n) {
        {
            std::lock_guard<std::mutex> lk(mtx);
            if (smallQ.size() >= kSmallQMax) smallQ.pop_front();   // 满则丢最旧(极少发生)
            smallQ.push_back(TxFrame{type, std::vector<uint8_t>(p, p + n)});
        }
        cv.notify_one();
    }
    void pushVideo(const uint8_t* p, size_t n) {
        {
            std::lock_guard<std::mutex> lk(mtx);
            video.assign(p, p + n);        // 覆盖旧帧
            haveVideo = true;
        }
        cv.notify_one();
    }
    void stop() { alive.store(false); cv.notify_all(); }
};

// 取本机第一个非回环 IPv4 地址（龙芯板自身 IP，用于小屏"中转状态页"）
std::string firstIpv4() {
    struct ifaddrs* ifap = nullptr;
    std::string ip;
    if (getifaddrs(&ifap) == 0) {
        for (auto* p = ifap; p; p = p->ifa_next) {
            if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET) continue;
            if (p->ifa_flags & IFF_LOOPBACK) continue;
            char buf[INET_ADDRSTRLEN] = {0};
            auto* sin = reinterpret_cast<struct sockaddr_in*>(p->ifa_addr);
            inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf));
            std::string cand = buf;
            if (cand.rfind("127.", 0) == 0 || cand.empty()) continue;
            ip = cand; break;
        }
        freeifaddrs(ifap);
    }
    return ip;
}

const char* cmdName(uint8_t id) {
    switch (id) {
        case net::CMD_FAN:    return "风扇";
        case net::CMD_BUZZER: return "蜂鸣器";
        case net::CMD_RELAY:  return "继电器";
        case net::CMD_LED:    return "LED";
        case net::CMD_MODE:   return "模式";
        case net::CMD_ESTOP:  return "急停";
        default:              return "未知";
    }
}
} // namespace

bool Application::interruptibleSleep(int totalMs) {
    int slept = 0;
    while (slept < totalMs) {
        if (!running_.load()) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        slept += 100;
    }
    return running_.load();
}

// 启动健康检测：把关键子系统状态汇总打印，便于开机后一眼判断是否就绪
bool Application::healthCheck() {
    LOG_INFO("---------- 启动健康检测 ----------");
    bool camOk = camera_.isOpen();
    bool netOk = server_.isListening();

    LOG_INFO("[健康] 摄像头 : %s  (%s %dx%d %s)",
             camOk ? "正常" : "异常",
             cfg_.device.c_str(), camera_.width(), camera_.height(),
             camera_.isMjpeg() ? "MJPEG" : "非MJPEG");
    if (camOk && !camera_.isMjpeg())
        LOG_WARN("[健康] 摄像头非 MJPEG 输出，上位机可能无法解码，建议更换摄像头");

    LOG_INFO("[健康] 网络监听: %s  (%s:%u)",
             netOk ? "正常" : "异常", cfg_.bindAddr.c_str(), cfg_.port);

    bool linkOk = robot_.isOpen();
    bool isCan  = (cfg_.linkType == "can");
    LOG_INFO("[健康] 下位机链路: %s  (%s %s)",
             linkOk ? "正常" : "未接入",
             isCan ? "CAN" : "UART",
             isCan ? cfg_.linkCanIf.c_str() : cfg_.serialDevice.c_str());
    if (!linkOk)
        LOG_WARN("[健康] F4 %s 未打开，运动/避障不可用（仅视频链路可用）%s",
                 isCan ? "CAN" : "串口",
                 isCan ? "；检查 can0 是否 up、内核 CAN 子系统" : "");

    // 串口未接入不算致命（视频链路仍可用），仅摄像头+网络决定 HEALTHY
    bool healthy = camOk && netOk;
    LOG_INFO("[健康] 总体状态: %s", healthy ? "就绪 (HEALTHY)" : "降级 (DEGRADED)");
    LOG_INFO("----------------------------------");
    return healthy;
}

int Application::run() {
    LOG_INFO("==== PatrolSystem 启动 (v%s) ====", versionString());
    startMs_ = nowMs();

    // ★网络优先：先启动 TCP 监听，再开摄像头。
    //   （旧逻辑先阻塞式重试打开摄像头，一旦板子开机时 UVC 未枚举好，会永远卡在这里
    //     导致 TCP 从不监听、上位机"连不上"。网络是最关键链路，必须最先就绪。）
    if (!server_.listen(cfg_.port, cfg_.bindAddr)) {
        LOG_ERROR("TCP 监听失败");
        return 1;
    }

    // 打开下位机 F4 传输层（CAN / UART，失败不致命，仅运动/避障不可用）
    LinkConfig link;
    link.type   = cfg_.linkType;
    link.canIf  = cfg_.linkCanIf;
    link.device = cfg_.serialDevice;
    link.baud   = cfg_.serialBaud;
    link.thermalDevice = cfg_.serialThermalDevice;
    link.thermalBaud   = cfg_.serialThermalBaud;
    robot_.init(link);

    // 打开摄像头：有限次数尝试（不阻塞网络）。失败也继续——上位机仍可连上看传感器/日志，
    // serveClient 每次连接会再懒打开一次，摄像头晚就绪也能自动恢复视频。
    for (int attempt = 1; attempt <= 3 && running_.load(); ++attempt) {
        if (camera_.open(cfg_.device, cfg_.width, cfg_.height, cfg_.fps)) break;
        LOG_WARN("摄像头 %s 打开失败（第 %d/3 次）%s",
                 cfg_.device.c_str(), attempt,
                 attempt < 3 ? "，1 秒后重试..." : "，暂无视频，连接后按需重试");
        if (attempt < 3 && !interruptibleSleep(1000)) return 0;
    }
    if (!running_.load()) return 0;

    // 启动健康检测
    healthCheck();

    // 启动控制线程（串口遥测 + 避障 + 下发命令），独立于上位机连接
    if (robot_.isOpen())
        controlThread_ = std::thread([this] { controlLoop(); });

    // 启动 SPI 小屏显示线程（默认关闭；config.json display.enabled=true 开启）
    if (display_.init(cfg_.display))
        displayThread_ = std::thread([this] { displayLoop(); });

    // 启动 UDP 发现广播线程（周期广播龙芯当前 IP，供上位机自动发现/重连）
    discoveryThread_ = std::thread([this] { discoveryLoop(); });

    while (running_.load()) {
        std::string peer;
        int clientFd = server_.acceptClient(/*timeoutMs=*/500, &peer);
        if (clientFd == -1) continue;
        if (clientFd == -2) break;

        // 标记上位机在线（供小屏"中转状态页"）
        { std::lock_guard<std::mutex> lk(upperMtx_); upperPeer_ = peer; }
        upperSinceMs_.store(nowMs());
        upperOnline_.store(true);

        serveClient(clientFd);

        upperOnline_.store(false);
        TcpServer::closeClient(clientFd);
        LOG_INFO("上位机 %s 已断开，等待重连...", peer.c_str());
    }

    if (controlThread_.joinable()) controlThread_.join();
    if (displayThread_.joinable()) displayThread_.join();
    if (discoveryThread_.joinable()) discoveryThread_.join();
    display_.close();
    robot_.close();
    camera_.close();
    server_.close();
    LOG_INFO("==== PatrolSystem 已退出 ====");
    return 0;
}

// 控制线程：~30Hz 轮询串口遥测、按模式做避障、下发 F4 命令帧。
void Application::controlLoop() {
    LOG_INFO("控制线程启动（F4 串口 @ ~30Hz）");
    while (running_.load()) {
        robot_.tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }
    // 退出前发一帧停车，避免下位机保持最后速度
    robot_.emergencyStop();
    robot_.tick();
    LOG_INFO("控制线程退出");
}

// 显示线程：SPI 小屏双页轮播。fb_ 全帧缓冲 + flush() 整帧推送 = 无闪烁，
// 故用 ~4Hz(250ms) 高频重绘保证数据实时性；每页停留 kDwellMs 后自动切页。
// 上电即绘制首帧（不依赖上位机连接），独立运行。
void Application::displayLoop() {
    LOG_INFO("显示线程启动（ST7789 双页轮播 @ ~4Hz 实时刷新, 每页 5s）");
    const uint32_t kRefreshMs = 250;   // 刷新周期：250ms ≈ 4Hz，够实时又不过载 SPI
    const uint32_t kDwellMs   = 5000;  // 每页停留 5s
    int page = 0;
    uint32_t pageSinceMs = nowMs();
    while (running_.load()) {
        if (page == 0) {
            net::SensorData s;
            robot_.fillSensorData(s);
            display_.showSensorPage(s);
        } else {
            display_.showRelayPage(buildRelayInfo());
        }
        if (!interruptibleSleep(kRefreshMs)) break;
        if (nowMs() - pageSinceMs >= kDwellMs) { pageSinceMs = nowMs(); page ^= 1; }
    }
    LOG_INFO("显示线程退出");
}

// 发现线程：每 2s 向局域网广播龙芯身份(版本+当前IP+TCP端口)，上位机监听自动发现并连接。
//   包格式(文本): "PATROL|<version>|<ip>|<tcp_port>"。龙芯 IP 变化(如切到手机热点 DHCP)后，
//   上位机也能自动跟上，无需死抠固定 IP。
void Application::discoveryLoop() {
    constexpr uint16_t kDiscoveryPort = 8083;
    int sock = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { LOG_WARN("发现广播 socket 创建失败，跳过自动发现"); return; }
    int on = 1;
    ::setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));

    sockaddr_in dst{};
    dst.sin_family      = AF_INET;
    dst.sin_port        = htons(kDiscoveryPort);
    dst.sin_addr.s_addr = htonl(INADDR_BROADCAST);   // 255.255.255.255（同网段可达）

    LOG_INFO("发现线程启动：每 2s 广播龙芯身份到 UDP :%u（上位机自动发现）", kDiscoveryPort);
    while (running_.load()) {
        std::string ip = firstIpv4();
        char msg[160];
        int n = std::snprintf(msg, sizeof(msg), "PATROL|%s|%s|%u",
                              PATROL_VERSION,
                              ip.empty() ? "0.0.0.0" : ip.c_str(),
                              static_cast<unsigned>(cfg_.port));
        if (n > 0)
            ::sendto(sock, msg, static_cast<size_t>(n), 0,
                     reinterpret_cast<sockaddr*>(&dst), sizeof(dst));
        if (!interruptibleSleep(2000)) break;
    }
    ::close(sock);
    LOG_INFO("发现线程退出");
}

// 汇总"中转状态页"所需信息：上位机连接 + 下位机链路 + 视觉 + 版本 + 运行时长。
St7789Display::RelayInfo Application::buildRelayInfo() {
    St7789Display::RelayInfo r;
    r.localIp = firstIpv4();
    r.upperOnline = upperOnline_.load();
    { std::lock_guard<std::mutex> lk(upperMtx_); r.upperPeer = upperPeer_; }
    uint32_t now = nowMs();
    if (r.upperOnline) r.upperDurS = (now - upperSinceMs_.load()) / 1000;
    r.uptimeS = (now - startMs_) / 1000;
    r.version = versionString();

    auto ls = robot_.linkStatus();
    r.f4Open     = ls.f4Open;
    r.telemCnt   = ls.telemCnt;
    r.envCnt     = ls.envCnt;
    r.thermalCnt = ls.thermalCnt;
    r.telemFresh = ls.telemFresh;
    r.envFresh   = ls.envFresh;
    r.visionOn   = ls.visionActive;
    r.visionName = ls.visionName;
    r.visionConf = ls.visionConf;
    r.thermalMaxC10 = ls.thermalMaxC10;
    r.hotspot    = ls.hotspot;
    return r;
}

void Application::serveClient(int clientFd) {
    // 摄像头可缺失：未打开则懒打开一次（开机晚枚举也能恢复）。无摄像头也继续服务，
    // 仍下发传感器/状态/热成像/日志，保证上位机能连上、看到数据，绝不因摄像头而拒连。
    if (!camera_.isOpen())
        camera_.open(cfg_.device, cfg_.width, cfg_.height, cfg_.fps);
    bool haveCam = camera_.isOpen() && camera_.startStreaming();
    if (camera_.isOpen() && !haveCam)
        LOG_WARN("启动取流失败，本次连接仅提供传感器/状态数据（无视频）");

    TcpServer::sendText(clientFd,
        haveCam
            ? "PatrolSystem ready " + std::to_string(camera_.width()) + "x" +
              std::to_string(camera_.height()) + (camera_.isMjpeg() ? " MJPEG" : " RAW")
            : std::string("PatrolSystem ready (无摄像头，仅传感器/状态)"));

    // ==== 方案B 第一阶段：四线程解耦（发送/视频采集/上报/接收），单 socket 只由发送线程写 ====
    ClientSession session;
    session.fd = clientFd;
    pidHushThermalUntil_.store(0);

    // 连接建立即发一次状态行（此刻缓冲空，可靠发送）。
    TcpServer::sendText(clientFd, robot_.statusLine());

    // ---- 发送线程：唯一写 socket 者。小帧优先全发(实时)，再发视频(拥塞时整帧丢弃)。----
    std::thread txThread([this, &session] {
        std::unique_lock<std::mutex> lk(session.mtx);
        while (session.alive.load() && running_.load()) {
            session.cv.wait_for(lk, std::chrono::milliseconds(100), [&] {
                return !session.alive.load() || !running_.load()
                    || !session.smallQ.empty() || session.haveVideo;
            });
            while (!session.smallQ.empty() && session.alive.load()) {
                TxFrame f = std::move(session.smallQ.front());
                session.smallQ.pop_front();
                lk.unlock();
                auto st = TcpServer::sendFrameDroppable(session.fd, f.type,
                                                        f.data.data(), f.data.size());
                lk.lock();
                if (st == TcpServer::SendStatus::Error) session.alive.store(false);
            }
            if (session.haveVideo && session.alive.load()) {
                std::vector<uint8_t> vb = std::move(session.video);
                session.haveVideo = false;
                lk.unlock();
                auto st = TcpServer::sendFrameDroppable(session.fd, net::FRAME_VIDEO,
                                                        vb.data(), vb.size());
                lk.lock();
                if (st == TcpServer::SendStatus::Error) session.alive.store(false);
            }
        }
        session.alive.store(false);
    });

    // ---- 视频采集线程：grabFrame 阻塞只在本线程，不拖累发送/命令/上报。----
    std::thread videoThread;
    if (haveCam) {
        videoThread = std::thread([this, &session] {
            uint32_t grabFail = 0, frameCount = 0, lastStatMs = nowMs();
            while (session.alive.load() && running_.load()) {
                const uint8_t* data = nullptr; size_t size = 0;
                if (camera_.grabFrame(&data, &size, /*timeoutMs=*/1000)) {
                    grabFail = 0;
                    if (size > 0) { session.pushVideo(data, size); ++frameCount; }
                } else if (++grabFail >= 5) {
                    LOG_WARN("连续取帧失败，疑似摄像头掉线，转为仅传感器/状态模式（保持连接）");
                    break;   // 退出视频线程；其余数据照常，连接不断
                }
                uint32_t t = nowMs();
                if (t - lastStatMs >= 5000) {
                    LOG_INFO("推流: %.1f fps  %dx%d", frameCount / 5.0,
                             camera_.width(), camera_.height());
                    frameCount = 0; lastStatMs = t;
                }
            }
        });
    }

    // ---- 上报线程：按节拍把 传感器/状态/热成像/PID遥测 投递到发送队列。----
    std::thread reportThread([this, &session] {
        uint32_t lastSensor = 0, lastStatus = 0;
        std::vector<int16_t> thermalBuf;
        std::vector<serial_proto::PidTele> pidTele;
        while (session.alive.load() && running_.load()) {
            uint32_t t = nowMs();
            if (t - lastSensor >= 200) {          // 5Hz 传感器
                lastSensor = t;
                net::SensorData s; robot_.fillSensorData(s);
                auto pl = net::packSensor(s);
                session.pushSmall(net::FRAME_SENSOR, pl.data(), pl.size());
            }
            if (t - lastStatus >= 2000) {         // 0.5Hz 状态行
                lastStatus = t;
                std::string line = robot_.statusLine();
                session.pushSmall(net::FRAME_TEXT,
                                  reinterpret_cast<const uint8_t*>(line.data()), line.size());
            }
            {                                     // 热成像(有新帧就投；PID 测试期间静默)
                int c = 0, r = 0;
                bool pidHush = (t < pidHushThermalUntil_.load());
                if (robot_.takeThermal(thermalBuf, c, r) && c > 0 && r > 0 && !pidHush) {
                    std::vector<uint8_t> pl;
                    pl.reserve(4 + thermalBuf.size() * 2);
                    pl.push_back(static_cast<uint8_t>(c & 0xFF));
                    pl.push_back(static_cast<uint8_t>((c >> 8) & 0xFF));
                    pl.push_back(static_cast<uint8_t>(r & 0xFF));
                    pl.push_back(static_cast<uint8_t>((r >> 8) & 0xFF));
                    for (int16_t v : thermalBuf) {
                        uint16_t u = static_cast<uint16_t>(v);
                        pl.push_back(static_cast<uint8_t>(u & 0xFF));
                        pl.push_back(static_cast<uint8_t>((u >> 8) & 0xFF));
                    }
                    session.pushSmall(net::FRAME_THERMAL, pl.data(), pl.size());
                }
            }
            pidTele.clear();                      // PID 遥测(测试时 50Hz)
            if (robot_.takePidTele(pidTele)) {
                for (const auto& pt : pidTele) {
                    uint8_t pl[23];
                    auto putI16 = [&](int i, int16_t v) {
                        pl[i] = static_cast<uint8_t>(v & 0xFF);
                        pl[i+1] = static_cast<uint8_t>((v >> 8) & 0xFF); };
                    auto putI32 = [&](int i, int32_t v) {
                        uint32_t u = static_cast<uint32_t>(v);
                        pl[i]=u&0xFF; pl[i+1]=(u>>8)&0xFF; pl[i+2]=(u>>16)&0xFF; pl[i+3]=(u>>24)&0xFF; };
                    pl[0] = static_cast<uint8_t>(pt.seq & 0xFF);
                    pl[1] = static_cast<uint8_t>((pt.seq >> 8) & 0xFF);
                    pl[2] = pt.flags;
                    putI16(3,  pt.targL); putI16(5,  pt.measL); putI16(7,  pt.outL);
                    putI16(9,  pt.targR); putI16(11, pt.measR); putI16(13, pt.outR);
                    putI32(15, pt.enc1);  putI32(19, pt.enc2);
                    session.pushSmall(net::FRAME_PID_TELE, pl, sizeof(pl));
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    // ---- 接收线程：独立处理下行 命令/驱动/视觉/PID，不被发送拖慢。----
    std::thread rxThread([this, &session] {
        std::vector<uint8_t> rxBuf;
        std::vector<net::Command> cmds;
        std::vector<net::DriveCommand> drives;
        std::vector<net::VisionResult> visions;
        std::vector<net::PidCommand> pidCmds;
        while (session.alive.load() && running_.load()) {
            struct pollfd pfd; pfd.fd = session.fd; pfd.events = POLLIN; pfd.revents = 0;
            if (::poll(&pfd, 1, 100) <= 0) continue;   // 超时/被打断 → 再查退出标志
            cmds.clear(); drives.clear(); visions.clear(); pidCmds.clear();
            int rc = TcpServer::pollCommands(session.fd, rxBuf, cmds, drives, visions, pidCmds);
            if (rc < 0) { session.alive.store(false); break; }   // 对端关闭/错误
            for (const auto& c : cmds) {
                LOG_INFO("下行命令: %s (0x%02X value=%u)", cmdName(c.cmdId), c.cmdId, c.value);
                robot_.handleCommand(c);
            }
            for (const auto& d : drives) robot_.driveManual(d.speed, d.steering);
            for (const auto& v : visions) robot_.setVision(v);
            for (const auto& pc : pidCmds) {
                if (pc.sub == 2 && pc.testMode != 0)   // PID 测试激励 → 静默热成像腾带宽
                    pidHushThermalUntil_.store(nowMs() + pc.durationMs + 2000);
                robot_.setPidCommand(pc);
            }
        }
    });

    // ---- 主线程：等待任一线程判定连接结束或全局停止，然后收拢全部线程。----
    while (session.alive.load() && running_.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    session.stop();
    if (videoThread.joinable()) videoThread.join();
    reportThread.join();
    rxThread.join();
    txThread.join();

    if (haveCam) camera_.stopStreaming();
}

} // namespace patrol