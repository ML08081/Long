#include "modules/config/ConfigManager.h"
#include "utils/JsonHelper.h"
#include "utils/FileUtil.h"
#include "modules/logger/Logger.h"

#include <cstdio>

namespace patrol {

bool ConfigManager::load(const std::string& path) {
    std::string content;
    if (!fs::readFile(path, content)) {
        LOG_WARN("配置文件 %s 无法读取，使用默认值", path.c_str());
        return false;
    }

    std::string err;
    json::JsonNode root = json::parse(content, &err);
    if (!err.empty() || root.is_null()) {
        LOG_WARN("配置文件解析失败 [%s]: %s，使用默认值", path.c_str(), err.c_str());
        return false;
    }

    const json::JsonNode& cam = root["camera"];
    if (cam.is_object()) {
        camera_.device = cam["device"].string_or(camera_.device);
        camera_.width  = static_cast<int>(cam["width"].int_or(camera_.width));
        camera_.height = static_cast<int>(cam["height"].int_or(camera_.height));
        camera_.fps    = static_cast<int>(cam["fps"].int_or(camera_.fps));
    }

    const json::JsonNode& net = root["network"];
    if (net.is_object()) {
        network_.bind = net["bind"].string_or(network_.bind);
        network_.port = static_cast<uint16_t>(net["port"].int_or(network_.port));
    }

    const json::JsonNode& ser = root["serial"];
    if (ser.is_object()) {
        serial_.device = ser["device"].string_or(serial_.device);
        serial_.baud   = static_cast<int>(ser["baud"].int_or(serial_.baud));
    }

    LOG_INFO("配置加载完成: cam=%s %dx%d@%dfps | net=%s:%u | serial=%s@%d",
             camera_.device.c_str(), camera_.width, camera_.height, camera_.fps,
             network_.bind.c_str(), network_.port,
             serial_.device.c_str(), serial_.baud);
    return true;
}

bool ConfigManager::save(const std::string& path) const {
    std::FILE* f = std::fopen(path.c_str(), "w");
    if (!f) { LOG_ERROR("无法写入配置文件: %s", path.c_str()); return false; }
    std::fprintf(f,
        "{\n"
        "  \"camera\": {\n"
        "    \"device\": \"%s\",\n"
        "    \"width\": %d,\n"
        "    \"height\": %d,\n"
        "    \"fps\": %d\n"
        "  },\n"
        "  \"network\": {\n"
        "    \"bind\": \"%s\",\n"
        "    \"port\": %u\n"
        "  },\n"
        "  \"serial\": {\n"
        "    \"device\": \"%s\",\n"
        "    \"baud\": %d\n"
        "  }\n"
        "}\n",
        camera_.device.c_str(), camera_.width, camera_.height, camera_.fps,
        network_.bind.c_str(), static_cast<unsigned>(network_.port),
        serial_.device.c_str(), serial_.baud);
    std::fclose(f);
    return true;
}

} // namespace patrol