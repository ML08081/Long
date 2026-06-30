#ifndef PATROL_MODULES_CONFIG_CONFIGMANAGER_H
#define PATROL_MODULES_CONFIG_CONFIGMANAGER_H

#include <string>
#include <cstdint>

namespace patrol {

// 从 JSON 配置文件加载运行参数。未出现的键保留默认值。
class ConfigManager {
public:
    struct CameraConf {
        std::string device = "/dev/video0";
        int         width  = 1280;
        int         height = 720;
        int         fps    = 30;
    };

    struct NetworkConf {
        std::string bind = "0.0.0.0";
        uint16_t    port = 8080;
    };

    struct SerialConf {
        std::string device = "/dev/ttyS1";
        int         baud   = 115200;
    };

    // 加载 JSON 文件。失败时保留默认值，返回 false。
    bool load(const std::string& path);

    // 将当前配置写回文件
    bool save(const std::string& path) const;

    const CameraConf&  camera()  const { return camera_; }
    const NetworkConf& network() const { return network_; }
    const SerialConf&  serial()  const { return serial_; }

    // 供 CLI 参数覆盖
    CameraConf&  camera()  { return camera_; }
    NetworkConf& network() { return network_; }
    SerialConf&  serial()  { return serial_; }

private:
    CameraConf  camera_;
    NetworkConf network_;
    SerialConf  serial_;
};

} // namespace patrol

#endif // PATROL_MODULES_CONFIG_CONFIGMANAGER_H