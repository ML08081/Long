// =============================================================================
//  link_test.cpp — 链路/帧协议集成自测（不依赖摄像头）
//
//  复用生产代码 TcpServer + FrameProtocol，向连接上来的客户端发送：
//     1) FRAME_TEXT   就绪文本
//     2) FRAME_SENSOR 40 字节遥测
//     3) FRAME_VIDEO  一段带 JPEG SOI/EOI 标记的伪视频帧（×N）
//  用于在没有真实摄像头时，验证“龙芯端发出的字节”能被上位机解析器正确解帧。
//
//  配套 test/frame_parser_check.py 复刻了 LongLook 的解析逻辑做收端校验。
//
//  单独编译（WSL）：
//     g++ -std=c++11 -Iinclude test/link_test.cpp src/modules/network/TcpServer.cpp src/modules/logger/Logger.cpp -lpthread -o /tmp/link_test
// =============================================================================
#include "modules/network/TcpServer.h"
#include "modules/network/FrameProtocol.h"
#include "modules/logger/Logger.h"

#include <cstdlib>
#include <string>
#include <vector>

using namespace patrol;

int main(int argc, char** argv) {
    uint16_t port   = (argc > 1) ? static_cast<uint16_t>(std::atoi(argv[1])) : 18080;
    int      frames = (argc > 2) ? std::atoi(argv[2]) : 5;

    TcpServer server;
    if (!server.listen(port)) return 1;

    std::string peer;
    int fd = server.acceptClient(/*timeoutMs=*/10000, &peer);
    if (fd < 0) { LOG_ERROR("等待客户端超时/出错"); return 1; }

    // 1) 文本帧
    TcpServer::sendText(fd, "link_test 就绪");

    // 2) 传感器帧（40 字节）
    net::SensorData s;
    s.timestamp_ms = 123456;
    s.temperature_01c = 256;   // 25.6 °C
    s.humidity_01     = 488;   // 48.8 %
    s.voltage_mV      = 12000; // 12.00 V
    s.mode = 3; s.risk_level = 0;
    auto sp = net::packSensor(s);
    TcpServer::sendFrame(fd, net::FRAME_SENSOR, sp.data(), sp.size());

    // 3) 视频帧（伪 JPEG：SOI 0xFFD8 ... EOI 0xFFD9）×N
    for (int i = 0; i < frames; ++i) {
        std::vector<uint8_t> jpg = { 0xFF, 0xD8, 0xFF, 0xE0 };
        for (int k = 0; k < 64; ++k) jpg.push_back(static_cast<uint8_t>(i + k));
        jpg.push_back(0xFF); jpg.push_back(0xD9);
        if (!TcpServer::sendFrame(fd, net::FRAME_VIDEO, jpg.data(), jpg.size())) break;
    }

    LOG_INFO("已向 %s 发送 文本/传感器/视频(%d 帧)，测试结束", peer.c_str(), frames);
    TcpServer::closeClient(fd);
    return 0;
}
