#include "app/Application.h"
#include "modules/logger/Logger.h"
#include "modules/network/FrameProtocol.h"

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

    bool healthy = camOk && netOk;
    LOG_INFO("[健康] 总体状态: %s", healthy ? "就绪 (HEALTHY)" : "降级 (DEGRADED)");
    LOG_INFO("----------------------------------");
    return healthy;
}

int Application::run() {
    LOG_INFO("==== PatrolSystem 启动 ====");

    // 打开摄像头：开机时 USB 可能尚未枚举完成，采用可中断重试而非直接退出。
    int attempt = 0;
    while (running_.load() &&
           !camera_.open(cfg_.device, cfg_.width, cfg_.height, cfg_.fps)) {
        ++attempt;
        LOG_WARN("摄像头 %s 打开失败（第 %d 次），3 秒后重试...",
                 cfg_.device.c_str(), attempt);
        if (!interruptibleSleep(3000)) {
            LOG_INFO("启动阶段收到退出请求");
            return 0;
        }
    }
    if (!running_.load()) return 0;

    // 启动 TCP 监听（绑定 0.0.0.0 不依赖网卡是否已分配 IP）。
    if (!server_.listen(cfg_.port, cfg_.bindAddr)) {
        LOG_ERROR("TCP 监听失败");
        camera_.close();
        return 1;
    }

    // 启动健康检测
    healthCheck();

    while (running_.load()) {
        std::string peer;
        int clientFd = server_.acceptClient(/*timeoutMs=*/500, &peer);
        if (clientFd == -1) continue;
        if (clientFd == -2) break;

        serveClient(clientFd);
        TcpServer::closeClient(clientFd);
        LOG_INFO("上位机 %s 已断开，等待重连...", peer.c_str());
    }

    camera_.close();
    server_.close();
    LOG_INFO("==== PatrolSystem 已退出 ====");
    return 0;
}

void Application::serveClient(int clientFd) {
    if (!camera_.startStreaming()) {
        LOG_ERROR("启动取流失败");
        return;
    }

    TcpServer::sendText(clientFd,
        "PatrolSystem ready " +
        std::to_string(camera_.width()) + "x" + std::to_string(camera_.height()) +
        (camera_.isMjpeg() ? " MJPEG" : " RAW"));

    std::vector<uint8_t> rxBuf;
    std::vector<net::Command> cmds;
    uint32_t frameCount   = 0;
    uint32_t lastStatMs   = nowMs();
    uint32_t lastSensorMs = 0;
    uint32_t grabFailCnt  = 0;

    while (running_.load()) {
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
            if (++grabFailCnt >= 5) {
                LOG_WARN("连续 %u 次取帧失败，疑似摄像头掉线，断开本次连接", grabFailCnt);
                break;
            }
        }

        uint32_t t = nowMs();

        if (t - lastSensorMs >= 1000) {
            lastSensorMs = t;
            net::SensorData s;
            s.timestamp_ms = t;
            s.voltage_mV   = 12000;
            s.mode         = 3;
            s.risk_level   = 0;
            auto pl = net::packSensor(s);
            if (!TcpServer::sendFrame(clientFd, net::FRAME_SENSOR, pl.data(), pl.size()))
                break;
        }

        cmds.clear();
        int rc = TcpServer::pollCommands(clientFd, rxBuf, cmds);
        if (rc < 0) break;
        for (const auto& c : cmds) {
            LOG_INFO("下行命令: %s (0x%02X value=%u)", cmdName(c.cmdId), c.cmdId, c.value);
        }

        if (t - lastStatMs >= 1000) {
            LOG_INFO("推流: %u fps  %dx%d", frameCount, camera_.width(), camera_.height());
            frameCount = 0;
            lastStatMs = t;
        }
    }

    camera_.stopStreaming();
}

} // namespace patrol