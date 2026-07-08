// =============================================================================
//  main.cpp -- PatrolSystem 入口
//
//  用法：
//     ./patrol_system [选项]
//     -c, --config  <路径>   配置文件 (默认 /etc/patrol/config.json)
//     -d, --device  <节点>   摄像头设备
//     -W, --width   <像素>   分辨率宽
//     -H, --height  <像素>   分辨率高
//     -f, --fps     <帧率>   期望帧率
//     -p, --port    <端口>   TCP 监听端口
//     -b, --bind    <地址>   监听地址
//         --log-dir <目录>   日志目录（自动生成 patrol-v<版本>-<时间>.log）
//         --log-file<路径>   指定日志文件（追加写入）
//     -v, --verbose          调试日志
//     -V, --version          显示版本号
//     -h, --help             显示帮助
// =============================================================================

#include "app/Application.h"
#include "modules/logger/Logger.h"
#include "modules/config/ConfigManager.h"
#include "utils/TimeUtil.h"
#include "version.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace patrol;

namespace {
Application* g_app = nullptr;

void onSignal(int) { if (g_app) g_app->requestStop(); }

void printVersion() {
    std::printf("PatrolSystem v%s (git %s, built %s)\n",
                versionString(), gitHash(), buildDate());
}

void printUsage(const char* prog) {
    std::printf(
        "PatrolSystem v%s -- 龙芯 2K0300 智能巡检系统（下位机）\n"
        "用法: %s [选项]\n"
        "  -c, --config  <路径>   配置文件 (默认 /etc/patrol/config.json)\n"
        "  -d, --device  <节点>   摄像头设备\n"
        "  -W, --width   <像素>   分辨率宽\n"
        "  -H, --height  <像素>   分辨率高\n"
        "  -f, --fps     <帧率>   期望帧率\n"
        "  -p, --port    <端口>   TCP 监听端口\n"
        "  -b, --bind    <地址>   监听地址\n"
        "      --log-dir <目录>   日志目录（自动命名 patrol-v版本-时间.log）\n"
        "      --log-file<路径>   指定日志文件（追加写入）\n"
        "  -v, --verbose          调试日志\n"
        "  -V, --version          显示版本号\n"
        "  -h, --help             显示本帮助\n",
        versionString(), prog);
}

const char* needValue(int argc, char** argv, int& i, const char* opt) {
    if (i + 1 >= argc) {
        std::fprintf(stderr, "选项 %s 缺少参数\n", opt);
        std::exit(2);
    }
    return argv[++i];
}

// 将 "YYYY-MM-DD HH:MM:SS" 转为文件名友好的 "YYYYMMDD-HHMMSS"
std::string compactTime() {
    std::string s = time_util::nowStr();   // 2026-07-01 09:35:07
    std::string out;
    for (char c : s) {
        if (c == '-' || c == ':') continue;
        out += (c == ' ') ? '-' : c;
    }
    return out;   // 20260701-093507
}
} // namespace

int main(int argc, char** argv) {
    // 预扫描：-V/--version、-c/--config（供第二遍使用）
    std::string configPath = "/etc/patrol/config.json";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-V" || a == "--version") { printVersion(); return 0; }
        if ((a == "-c" || a == "--config") && i + 1 < argc) configPath = argv[++i];
    }

    ConfigManager cfgmgr;
    cfgmgr.load(configPath);

    AppConfig cfg;
    cfg.device   = cfgmgr.camera().device;
    cfg.width    = cfgmgr.camera().width;
    cfg.height   = cfgmgr.camera().height;
    cfg.fps      = cfgmgr.camera().fps;
    cfg.port     = cfgmgr.network().port;
    cfg.bindAddr = cfgmgr.network().bind;
    cfg.serialDevice = cfgmgr.serial().device;
    cfg.serialBaud   = cfgmgr.serial().baud;
    cfg.serialThermalDevice = cfgmgr.serial().thermalDevice;
    cfg.serialThermalBaud   = cfgmgr.serial().thermalBaud;
    // SPI 小屏配置
    const auto& dc = cfgmgr.display();
    cfg.display.enabled = dc.enabled;
    cfg.display.spiDev  = dc.spiDev;
    cfg.display.gpioDC  = dc.gpioDC;
    cfg.display.gpioRST = dc.gpioRST;
    cfg.display.gpioBL  = dc.gpioBL;
    cfg.display.width   = dc.width;
    cfg.display.height  = dc.height;
    cfg.display.spiHz   = static_cast<uint32_t>(dc.spiHz);
    cfg.display.rotation = dc.rotation;

    bool        verbose = false;
    std::string logDir, logFile;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-c" || a == "--config") { ++i; }
        else if (a == "-d" || a == "--device") cfg.device = needValue(argc, argv, i, a.c_str());
        else if (a == "-W" || a == "--width")  cfg.width  = std::atoi(needValue(argc, argv, i, a.c_str()));
        else if (a == "-H" || a == "--height") cfg.height = std::atoi(needValue(argc, argv, i, a.c_str()));
        else if (a == "-f" || a == "--fps")    cfg.fps    = std::atoi(needValue(argc, argv, i, a.c_str()));
        else if (a == "-p" || a == "--port")   cfg.port   = static_cast<uint16_t>(std::atoi(needValue(argc, argv, i, a.c_str())));
        else if (a == "-b" || a == "--bind")   cfg.bindAddr = needValue(argc, argv, i, a.c_str());
        else if (a == "--log-dir")             logDir  = needValue(argc, argv, i, a.c_str());
        else if (a == "--log-file")            logFile = needValue(argc, argv, i, a.c_str());
        else if (a == "-v" || a == "--verbose") verbose = true;
        else if (a == "-V" || a == "--version") { printVersion(); return 0; }
        else if (a == "-h" || a == "--help")    { printUsage(argv[0]); return 0; }
        else {
            std::fprintf(stderr, "未知选项: %s\n", a.c_str());
            printUsage(argv[0]);
            return 2;
        }
    }

    Logger::setLevel(verbose ? LogLevel::Debug : LogLevel::Info);

    // 配置日志文件：--log-file 优先；否则 --log-dir 自动按版本+时间命名，便于归档整理
    if (!logFile.empty()) {
        if (!Logger::setLogFile(logFile))
            std::fprintf(stderr, "警告: 无法打开日志文件 %s\n", logFile.c_str());
    } else if (!logDir.empty()) {
        std::string path = logDir + "/patrol-v" + versionString() + "-" + compactTime() + ".log";
        if (!Logger::setLogFile(path))
            std::fprintf(stderr, "警告: 无法在 %s 创建日志文件\n", logDir.c_str());
    }

    // 启动横幅：版本号写入日志，方便按版本整理
    LOG_INFO("================================================");
    LOG_INFO("PatrolSystem v%s  (git %s, built %s)",
             versionString(), gitHash(), buildDate());
    LOG_INFO("================================================");

    std::signal(SIGPIPE, SIG_IGN);
    Application app(cfg);
    g_app = &app;
    std::signal(SIGINT,  onSignal);
    std::signal(SIGTERM, onSignal);

    int rc = app.run();
    g_app = nullptr;
    Logger::closeLogFile();
    return rc;
}