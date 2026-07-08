#ifndef PATROL_MODULES_THERMAL_THERMALSOLVER_H
#define PATROL_MODULES_THERMAL_THERMALSOLVER_H

#include <cstdint>

// MLX90640 官方 API（C）。放在 third_party/mlx90640。
extern "C" {
#include "MLX90640_API.h"
}

namespace patrol {

// =============================================================================
//  ThermalSolver — 热成像解算（从 F4 下放到龙芯）
//
//  原方案 F4 用 M4 跑 Melexis CalculateTo（每帧 300~400ms，两个子页各算一次），
//  严重占用 F4 且拖慢帧率/实时性。改为：F4 只读原始帧(834 字)并转发，龙芯用同一套
//  Melexis 纯计算 API 解算——龙芯算力远强，帧率/延迟大幅改善，且不再抢 F4 的 CPU。
//
//  用法：
//    setEeprom(ee832)  —— F4 上电时发一次 EEPROM，据此提取标定参数（一次）。
//    solve(raw834, out768) —— 每收到一个原始子页帧，解算出 768 个 int16 温度(0.01°C)。
//      返回 true 表示已就绪并成功解算；参数未就绪(未收到 EEPROM)时返回 false。
// =============================================================================
class ThermalSolver {
public:
    static constexpr int PIXELS = 768;   // 32x24
    static constexpr int EE_WORDS    = 832;
    static constexpr int FRAME_WORDS = 834;

    // 提取标定参数（收到完整 EEPROM 后调用一次）。成功返回 true。
    bool setEeprom(const uint16_t* ee832);

    bool ready() const { return ready_; }

    // 解算一帧原始数据 -> 768 个 int16 温度(0.01°C, 行主序)。out 至少 PIXELS 个。
    bool solve(const uint16_t* raw834, int16_t* out768);

    // 发射率 / 反射温度偏移可按需调整（默认与原 F4 一致）
    void setEmissivity(float e) { emissivity_ = e; }
    void setTrOffset(float d)   { trOffset_ = d; }

private:
    paramsMLX90640 params_{};
    bool  ready_      = false;
    float emissivity_ = 0.95f;   // 与原 F4 thermal.c 一致
    float trOffset_   = 8.0f;    // 反射温度 = Ta - 8°C
    float to_[PIXELS] = {0};     // 解算结果暂存（避免大栈占用）
};

} // namespace patrol

#endif // PATROL_MODULES_THERMAL_THERMALSOLVER_H
