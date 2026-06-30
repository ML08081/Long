#include "app/Application.h"
#include "modules/logger/Logger.h"
#include "modules/network/FrameProtocol.h"

#include <chrono>
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

int Application::run() {
    LOG_INFO("==== PatrolSystem 启动 ====");

    if (!camera_.open(cfg_.device, cfg_.width, cfg_.height, cfg_.fps)) {
        LOG_ERROR("摄像头打开失败，请确认设备 %s 存在", cfg_.device.c_str());
        return 1;
    }

    if (!server_.listen(cfg_.port, cfg_.bindAddr)) {
        LOG_ERROR("TCP 监听失败");
        return 1;
    }

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
    uint32_t frameCount  = 0;
    uint32_t lastStatMs  = nowMs();
    uint32_t lastSensorMs = 0;

    while (running_.load()) {
        // ── 采集并推送一帧视频 ──
        const uint8_t* data = nullptr;
        size_t size = 0;
        if (camera_.grabFrame(&data, &size, /*timeoutMs=*/1000) && size > 0) {
            if (!TcpServer::sendFrame(clientFd, net::FRAME_VIDEO, data, size))
                break;
            ++frameCount;
        }

        uint32_t t = nowMs();

        // ── 每秒推送一次传感器占位数据 ──
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

        // ── 处理上位机下行命令 ──
        cmds.clear();
        int rc = TcpServer::pollCommands(clientFd, rxBuf, cmds);
        // rc < 0：-1 对端关闭，-2 读错误，均应退出推流循环
        if (rc < 0) break;
        for (const auto& c : cmds) {
            LOG_INFO("下行命令: %s (0x%02X value=%u)", cmdName(c.cmdId), c.cmdId, c.value);
        }

        // ── 每秒打印帧率 ──
        if (t - lastStatMs >= 1000) {
            LOG_INFO("推流: %u fps  %dx%d", frameCount, camera_.width(), camera_.height());
            frameCount = 0;
            lastStatMs = t;
        }
    }

    camera_.stopStreaming();
}

} // namespace patrol