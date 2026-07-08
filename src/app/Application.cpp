#include "app/Application.h"
#include "modules/logger/Logger.h"
#include "modules/network/FrameProtocol.h"
#include "version.h"

#include <chrono>
#include <thread>
#include <vector>

namespace patrol {

namespace {
uint32_t nowMs() {
    using namespace std::chrono;
    return static_cast<uint32_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
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

    bool serialOk = robot_.isOpen();
    LOG_INFO("[健康] 下位机串口: %s  (%s @ %d)",
             serialOk ? "正常" : "未接入", cfg_.serialDevice.c_str(), cfg_.serialBaud);
    if (!serialOk)
        LOG_WARN("[健康] F4 串口未打开，运动/避障不可用（仅视频链路可用）");

    // 串口未接入不算致命（视频链路仍可用），仅摄像头+网络决定 HEALTHY
    bool healthy = camOk && netOk;
    LOG_INFO("[健康] 总体状态: %s", healthy ? "就绪 (HEALTHY)" : "降级 (DEGRADED)");
    LOG_INFO("----------------------------------");
    return healthy;
}

int Application::run() {
    LOG_INFO("==== PatrolSystem 启动 (v%s) ====", versionString());

    // ★网络优先：先启动 TCP 监听，再开摄像头。
    //   （旧逻辑先阻塞式重试打开摄像头，一旦板子开机时 UVC 未枚举好，会永远卡在这里
    //     导致 TCP 从不监听、上位机"连不上"。网络是最关键链路，必须最先就绪。）
    if (!server_.listen(cfg_.port, cfg_.bindAddr)) {
        LOG_ERROR("TCP 监听失败");
        return 1;
    }

    // 打开下位机 F4 串口（失败不致命，仅运动/避障不可用）
    robot_.init(cfg_.serialDevice, cfg_.serialBaud,
                cfg_.serialThermalDevice, cfg_.serialThermalBaud);

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

    while (running_.load()) {
        std::string peer;
        int clientFd = server_.acceptClient(/*timeoutMs=*/500, &peer);
        if (clientFd == -1) continue;
        if (clientFd == -2) break;

        serveClient(clientFd);
        TcpServer::closeClient(clientFd);
        LOG_INFO("上位机 %s 已断开，等待重连...", peer.c_str());
    }

    if (controlThread_.joinable()) controlThread_.join();
    if (displayThread_.joinable()) displayThread_.join();
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

// 显示线程：~1Hz 把最新巡检状态渲染到 SPI 小屏（独立于上位机连接）。
void Application::displayLoop() {
    LOG_INFO("显示线程启动（ST7789 SPI 小屏 @ ~1Hz）");
    while (running_.load()) {
        net::SensorData s;
        robot_.fillSensorData(s);
        display_.showStatus(s, robot_.statusLine());
        if (!interruptibleSleep(1000)) break;
    }
    LOG_INFO("显示线程退出");
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

    std::vector<uint8_t> rxBuf;
    std::vector<net::Command> cmds;
    std::vector<net::DriveCommand> drives;
    std::vector<net::VisionResult> visions;
    std::vector<int16_t> thermalBuf;   // 复用，减少分配
    uint32_t frameCount   = 0;
    uint32_t lastStatMs   = nowMs();
    uint32_t lastSensorMs = 0;
    uint32_t lastStatusMs = 0;
    uint32_t grabFailCnt  = 0;

    // 连接建立即发一次状态行，随后每 2s 一次 —— 上位机"龙芯日志"栏据此看链路/传感器/版本。
    TcpServer::sendText(clientFd, robot_.statusLine());

    while (running_.load()) {
        if (haveCam) {
            const uint8_t* data = nullptr;
            size_t size = 0;
            if (camera_.grabFrame(&data, &size, /*timeoutMs=*/1000)) {
                grabFailCnt = 0;
                if (size > 0) {
                    if (!TcpServer::sendFrame(clientFd, net::FRAME_VIDEO, data, size))
                        break;
                    ++frameCount;
                }
            } else {
                // 摄像头掉线：不再断开连接，转为"仅传感器/状态"模式继续服务上位机
                if (++grabFailCnt >= 5) {
                    LOG_WARN("连续取帧失败，疑似摄像头掉线，转为仅传感器/状态模式（保持连接）");
                    camera_.stopStreaming();
                    haveCam = false;
                }
            }
        } else {
            // 无摄像头：小睡维持 ~50Hz 节奏，不空转占满 CPU；传感器/状态照常下发
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        uint32_t t = nowMs();

        if (t - lastSensorMs >= 200) {   // 5Hz 遥测，含 F4 距离/编码器/速度/模式
            lastSensorMs = t;
            net::SensorData s;
            robot_.fillSensorData(s);    // 真实下位机数据
            auto pl = net::packSensor(s);
            if (!TcpServer::sendFrame(clientFd, net::FRAME_SENSOR, pl.data(), pl.size()))
                break;
        }

        // 转发热成像帧（F4 -> 龙芯 组装完成后 -> LongLook 0x20）：
        //   payload = [w u16 LE][h u16 LE][w*h int16 LE 温度(0.01°C)]
        {
            int tCols = 0, tRows = 0;
            if (robot_.takeThermal(thermalBuf, tCols, tRows) && tCols > 0 && tRows > 0) {
                std::vector<uint8_t> pl;
                pl.reserve(4 + thermalBuf.size() * 2);
                pl.push_back(static_cast<uint8_t>(tCols & 0xFF));
                pl.push_back(static_cast<uint8_t>((tCols >> 8) & 0xFF));
                pl.push_back(static_cast<uint8_t>(tRows & 0xFF));
                pl.push_back(static_cast<uint8_t>((tRows >> 8) & 0xFF));
                for (int16_t v : thermalBuf) {
                    uint16_t u = static_cast<uint16_t>(v);
                    pl.push_back(static_cast<uint8_t>(u & 0xFF));
                    pl.push_back(static_cast<uint8_t>((u >> 8) & 0xFF));
                }
                if (!TcpServer::sendFrame(clientFd, net::FRAME_THERMAL, pl.data(), pl.size()))
                    break;
            }
        }

        // 周期回发精简状态行给上位机的"龙芯日志"栏（含链路计数/传感器/风险/视觉）
        if (t - lastStatusMs >= 2000) {
            lastStatusMs = t;
            if (!TcpServer::sendText(clientFd, robot_.statusLine())) break;
        }

        cmds.clear();
        drives.clear();
        visions.clear();
        int rc = TcpServer::pollCommands(clientFd, rxBuf, cmds, drives, visions);
        if (rc < 0) break;
        for (const auto& c : cmds) {
            LOG_INFO("下行命令: %s (0x%02X value=%u)", cmdName(c.cmdId), c.cmdId, c.value);
            robot_.handleCommand(c);     // 路由到运动/避障控制
        }
        for (const auto& d : drives) {
            LOG_DEBUG("手动驱动: speed=%d steering=%d", d.speed, d.steering);
            robot_.driveManual(d.speed, d.steering);   // 上位机手动操控 -> 转发 F4
        }
        for (const auto& v : visions)
            robot_.setVision(v);         // 上位机视觉结果 -> 龙芯"大脑"纳入判断

        // 推流帧率日志降频到 5s，减少龙芯 console 刷屏
        if (haveCam && t - lastStatMs >= 5000) {
            LOG_INFO("推流: %.1f fps  %dx%d",
                     frameCount / 5.0, camera_.width(), camera_.height());
            frameCount = 0;
            lastStatMs = t;
        }
    }

    if (haveCam) camera_.stopStreaming();
}

} // namespace patrol