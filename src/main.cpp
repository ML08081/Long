// =============================================================================
//  main.cpp -- PatrolSystem 入口
//
//  用法：
//     ./patrol_system [选项]
//  选项：
//     -c, --config <路径>   配置文件路径 (默认 /etc/patrol/config.json)
//     -d, --device <节点>   摄像头设备 (覆盖配置文件)
//     -W, --width  <像素>   宽 (覆盖配置文件)
//     -H, --height <像素>   高 (覆盖配置文件)
//     -f, --fps    <帧率>   期望帧率 (覆盖配置文件)
//     -p, --port   <端口>   TCP 监听端口 (覆盖配置文件)
//     -b, --bind   <地址>   监听地址 (覆盖配置文件)
//     -v, --verbose         输出调试日志
//     -h, --help            显示帮助
// =============================================================================

#include "app/Application.h"
#include "modules/logger/Logger.h"
#include "modules/config/ConfigManager.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace patrol;

namespace {
Application* g_app = nullptr;

void onSignal(int) { if (g_app) g_app->requestStop(); }

void printUsage(const char* prog) {
    std::printf(
        "用法: %s [选项]\n"
        "  -c, --config <路径>   配置文件 (默认 /etc/patrol/config.json)\n"
        "  -d, --device <节点>   摄像头设备\n"
        "  -W, --width  <像素>   分辨率宽\n"
        "  -H, --height <像素>   分辨率高\n"
        "  -f, --fps    <帧率>   期望帧率\n"
        "  -p, --port   <端口>   TCP 监听端口\n"
        "  -b, --bind   <地址>   监听地址\n"
        "  -v, --verbose         调试日志\n"
        "  -h, --help            本帮助\n",
        prog);
}

const char* needValue(int argc, char** argv, int& i, const char* opt) {
    if (i + 1 >= argc) {
        std::fprintf(stderr, "选项 %s 缺少参数\n", opt);
        std::exit(2);
    }
    return argv[++i];
}
} // namespace

int main(int argc, char** argv) {
    // 第一遍：仅找 -c/--config 路径
    std::string configPath = "/etc/patrol/config.json";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if ((a == "-c" || a == "--config") && i + 1 < argc)
            configPath = argv[++i];
    }

    // 加载配置文件（失败则使用内置默认值）
    ConfigManager cfgmgr;
    cfgmgr.load(configPath);

    // 将配置文件值作为初始值
    AppConfig cfg;
    cfg.device   = cfgmgr.camera().device;
    cfg.width    = cfgmgr.camera().width;
    cfg.height   = cfgmgr.camera().height;
    cfg.fps      = cfgmgr.camera().fps;
    cfg.port     = cfgmgr.network().port;
    cfg.bindAddr = cfgmgr.network().bind;

    // 第二遍：命令行参数覆盖配置文件
    bool verbose = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-c" || a == "--config") {
            ++i;  // 已在第一遍处理，跳过参数值
        } else if (a == "-d" || a == "--device") {
            cfg.device = needValue(argc, argv, i, a.c_str());
        } else if (a == "-W" || a == "--width") {
            cfg.width = std::atoi(needValue(argc, argv, i, a.c_str()));
        } else if (a == "-H" || a == "--height") {
            cfg.height = std::atoi(needValue(argc, argv, i, a.c_str()));
        } else if (a == "-f" || a == "--fps") {
            cfg.fps = std::atoi(needValue(argc, argv, i, a.c_str()));
        } else if (a == "-p" || a == "--port") {
            cfg.port = static_cast<uint16_t>(std::atoi(needValue(argc, argv, i, a.c_str())));
        } else if (a == "-b" || a == "--bind") {
            cfg.bindAddr = needValue(argc, argv, i, a.c_str());
        } else if (a == "-v" || a == "--verbose") {
            verbose = true;
        } else if (a == "-h" || a == "--help") {
            printUsage(argv[0]);
            return 0;
        } else {
            std::fprintf(stderr, "未知选项: %s\n", a.c_str());
            printUsage(argv[0]);
            return 2;
        }
    }

    Logger::setLevel(verbose ? LogLevel::Debug : LogLevel::Info);

    std::signal(SIGPIPE, SIG_IGN);
    Application app(cfg);
    g_app = &app;
    std::signal(SIGINT,  onSignal);
    std::signal(SIGTERM, onSignal);

    int rc = app.run();
    g_app = nullptr;
    return rc;
}