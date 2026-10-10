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
#include <fcntl.h>
#include <cstdlib>
#include <cerrno>
#include <cstring>

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
struct TxFrame { uint8_t type; std::vector<uint8_t> data; uint32_t generation, queuedMs; };

struct ClientSession {
    int fd = -1;
    std::atomic<bool> alive{true};

    std::mutex mtx;
    std::condition_variable cv;
    std::deque<TxFrame> smallQ;
    std::vector<uint8_t> video;
    bool haveVideo = false;

    static constexpr size_t kSmallQMax = 128;

    void pushSmall(uint8_t type, const uint8_t* p, size_t n, uint32_t generation=0) {
        {
            std::lock_guard<std::mutex> lk(mtx);
            if (smallQ.size() >= kSmallQMax) smallQ.pop_front();   // 满则丢最旧(极少发生)
            smallQ.push_back(TxFrame{type, std::vector<uint8_t>(p, p + n),generation,nowMs()});
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

// A separate file keeps camera/CAN/display and unknown deployment keys intact.
bool saveSpeed(const std::string& path, int speed) {
    if(path.empty()) return false;
    std::string temp=path+".tmp.XXXXXX";
    std::vector<char> name(temp.begin(),temp.end()); name.push_back(0);
    int fd=::mkstemp(name.data());
    if(fd<0) return false;
    char b[16]; int n=std::snprintf(b,sizeof(b),"%d\n",speed);
    bool ok=::write(fd,b,n)==n && ::fsync(fd)==0;
    if(::close(fd)!=0) ok=false;
    if(ok) ok=::rename(name.data(),path.c_str())==0;
    if(!ok) ::unlink(name.data());
    if(ok) {
        size_t slash=path.find_last_of('/');
        std::string dir=slash==std::string::npos ? "." : path.substr(0,slash);
        int d=::open(dir.c_str(),O_RDONLY|O_DIRECTORY);
        if(d>=0) { ok=::fsync(d)==0; ::close(d); } else ok=false;
    }
    return ok;
}

const char* cmdName(uint8_t id) {
    switch (id) {
        case net::CMD_FAN:    return "风扇";
        case net::CMD_BUZZER: return "蜂鸣器";
        case net::CMD_RELAY:  return "继电器";
        case net::CMD_LED:    return "LED";
        case net::CMD_MODE:   return "模式";
        case net::CMD_ESTOP:  return "急停";
        case net::CMD_THERMAL:return "热像预览";
        case net::CMD_SPEED:  return "速度档位";
        default:              return "未知";
    }
}
} // namespace

bool Application::interruptibleSleep(int totalMs) {
    // ★ 2026-07-23：粒度由 100ms 收到 20ms。
    //   原实现按 100ms 步进且条件是 slept < totalMs，导致 interruptibleSleep(250)
    //   实际睡 300ms —— 所有基于它的周期（尤其显示线程 4Hz 刷新）都比标称慢一截，
    //   这是小屏"刷新不够灵敏"的原因之一。20ms 粒度下最大超出仅 19ms。
    constexpr int kStepMs = 20;
    int slept = 0;
    while (slept < totalMs) {
        if (!running_.load()) return false;
        int step = totalMs - slept;
        if (step > kStepMs) step = kStepMs;
        std::this_thread::sleep_for(std::chrono::milliseconds(step));
        slept += step;
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
    if(!cfg_.runtimeSpeedPath.empty()) {
        std::FILE* f=std::fopen(cfg_.runtimeSpeedPath.c_str(),"r");
        if(f) {
            int value=0; char tail=0;
            int fields=std::fscanf(f,"%d %c",&value,&tail); std::fclose(f);
            if(fields==1 && value>=100 && value<=2000 && (value-100)%100==0)
                robot_.setSpeedGear(static_cast<uint8_t>((value-100)/100));
            else LOG_WARN("忽略无效速度配置: %s",cfg_.runtimeSpeedPath.c_str());
        }
    }

    // Control and retries run immediately; camera enumeration never delays them.
    controlThread_ = std::thread([this] { controlLoop(); });
    healthCheck();

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

        robot_.setUpperConnected(true);
        serveClient(clientFd);
        robot_.setUpperConnected(false);

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

// 显示线程：大屏双页，小屏将中转信息拆分，避免超出128x160。
// 上电即绘制首帧（不依赖上位机连接），独立运行。
void Application::displayLoop() {
    const int pageCount = 1 + display_.relayPageCount();
    LOG_INFO("显示线程启动（%s %d页轮播；巡检8s，其余每页4s）", cfg_.display.controller.c_str(), pageCount);
    // ★ 2026-07-23 提升灵敏度：刷新 250ms->150ms(≈6.7Hz)；
    //   传感页是现场主要盯的页面，停留时间加长，中转页缩短。
    const uint32_t kRefreshMs      = 150;   // 刷新周期 ≈ 6.7Hz（fb_ 整帧推送，无闪烁）
    const uint32_t kDwellSensorMs  = 8000;  // 传感页停留 8s
    const uint32_t kDwellRelayMs   = 4000;  // 中转页停留 4s
    int page = 0;
    uint32_t pageSinceMs = nowMs();
    while (running_.load()) {
        if (page == 0) {
            net::SensorData s;
            robot_.fillSensorData(s);
            display_.showSensorPage(s);
        } else {
            display_.showRelayPage(buildRelayInfo(), page - 1);
        }
        if (!interruptibleSleep(kRefreshMs)) break;
        const uint32_t dwell = (page == 0) ? kDwellSensorMs : kDwellRelayMs;
        if (nowMs() - pageSinceMs >= dwell) { pageSinceMs = nowMs(); page = (page + 1) % pageCount; }
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
    r.thermalFresh = ls.thermalFresh;
    r.hotspot    = ls.hotspot;
    return r;
}

void Application::serveClient(int clientFd) {
    TcpServer::sendText(clientFd,"PatrolSystem ready; camera connects in background");

    // ==== 方案B 第一阶段：四线程解耦（发送/视频采集/上报/接收），单 socket 只由发送线程写 ====
    ClientSession session;
    session.fd = clientFd;

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
                if(f.type==net::FRAME_THERMAL && (!robot_.thermalAllowed() ||
                    f.generation!=robot_.thermalGeneration() || nowMs()-f.queuedMs>3000)) {
                    lk.lock(); continue;
                }
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
    std::thread videoThread([this,&session] {
        uint32_t retry=0, failures=0;
        while(session.alive.load() && running_.load()) {
            if(!camera_.isOpen()) {
                uint32_t now=nowMs();
                if(retry && now-retry<2000) { std::this_thread::sleep_for(std::chrono::milliseconds(50)); continue; }
                retry=now;
                if(!camera_.open(cfg_.device,cfg_.width,cfg_.height,cfg_.fps) || !camera_.startStreaming()) {
                    camera_.close(); continue;
                }
                failures=0;
            }
            const uint8_t* data=nullptr; size_t size=0;
            if(camera_.grabFrame(&data,&size,250)) {
                failures=0; if(size) session.pushVideo(data,size);
            } else if(++failures>=5) {
                camera_.close(); retry=nowMs();
            }
        }
        camera_.close(); // only this thread operates camera during a session
    });

    // ---- 上报线程：按节拍把 传感器/状态/热成像 投递到发送队列。----
    std::thread reportThread([this, &session] {
        uint32_t lastSensor = 0, lastStatus = 0;
        std::vector<int16_t> thermalBuf;
        while (session.alive.load() && running_.load()) {
            uint32_t t = nowMs();
            if (t - lastSensor >= 200) {          // 5Hz 传感器
                lastSensor = t;
                net::SensorData s; robot_.fillSensorData(s);
                auto pl = net::packSensor(s);
                session.pushSmall(net::FRAME_SENSOR, pl.data(), pl.size());
                auto status=robot_.deviceStatus();
                session.pushSmall(net::FRAME_DEVICE_STATUS,status.data(),status.size());
            }
            if (t - lastStatus >= 2000) {         // 0.5Hz 状态行
                lastStatus = t;
                std::string line = robot_.statusLine();
                session.pushSmall(net::FRAME_TEXT,
                                  reinterpret_cast<const uint8_t*>(line.data()), line.size());
            }
            {                                     // 热成像(有新帧就投)
                int c = 0, r = 0; uint32_t generation=0;
                if (robot_.takeThermal(thermalBuf, c, r, &generation) && c > 0 && r > 0) {
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
                    session.pushSmall(net::FRAME_THERMAL, pl.data(), pl.size(),generation);
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    // ---- 接收线程：独立处理下行 命令/驱动/视觉，不被发送拖慢。----
    std::thread rxThread([this, &session] {
        std::vector<uint8_t> rxBuf;
        std::vector<net::Command> cmds;
        std::vector<net::DriveCommand> drives;
        std::vector<net::VisionResult> visions;
        while (session.alive.load() && running_.load()) {
            struct pollfd pfd; pfd.fd = session.fd; pfd.events = POLLIN; pfd.revents = 0;
            if (::poll(&pfd, 1, 100) <= 0) continue;   // 超时/被打断 → 再查退出标志
            cmds.clear(); drives.clear(); visions.clear();
            int rc = TcpServer::pollCommands(session.fd, rxBuf, cmds, drives, visions);
            if (rc < 0) { session.alive.store(false); break; }   // 对端关闭/错误
            for (const auto& c : cmds) {
                LOG_INFO("下行命令: %s (0x%02X value=%u)", cmdName(c.cmdId), c.cmdId, c.value);
                robot_.handleCommand(c);
                if(c.cmdId==net::CMD_SPEED && c.value<=19) {
                    bool saved=saveSpeed(cfg_.runtimeSpeedPath,100+100*c.value);
                    robot_.setSpeedPersisted(saved);
                    if(!saved) LOG_WARN("速度已请求，但持久化失败: %s",cfg_.runtimeSpeedPath.c_str());
                }
            }
            for (const auto& d : drives) robot_.driveManual(d.speed, d.steering);
            for (const auto& v : visions) robot_.setVision(v);
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


}

} // namespace patrol
