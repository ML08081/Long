#include "modules/thermal/ThermalSolver.h"
#include "modules/logger/Logger.h"

namespace patrol {

bool ThermalSolver::setEeprom(const uint16_t* ee832) {
    // MLX90640_ExtractParameters 会就地写 params_；成功返回 0。
    // 传入非 const 需拷贝一份（API 签名要求可写指针）。
    uint16_t ee[EE_WORDS];
    for (int i = 0; i < EE_WORDS; ++i) ee[i] = ee832[i];
    int rc = MLX90640_ExtractParameters(ee, &params_);
    ready_ = (rc == 0);
    if (!ready_)
        LOG_WARN("热成像: EEPROM 参数提取失败 rc=%d", rc);
    else
        LOG_INFO("热成像: 标定参数就绪（龙芯解算）");
    return ready_;
}

bool ThermalSolver::solve(const uint16_t* raw834, int16_t* out768) {
    if (!ready_) return false;

    // MLX90640 API 需要可写的 frameData（内部会用 statusReg 等）。拷贝一份。
    uint16_t frame[FRAME_WORDS];
    for (int i = 0; i < FRAME_WORDS; ++i) frame[i] = raw834[i];

    float ta = MLX90640_GetTa(frame, &params_);
    float tr = ta - trOffset_;                 // 反射温度经验值
    MLX90640_CalculateTo(frame, &params_, emissivity_, tr, to_);

    for (int i = 0; i < PIXELS; ++i) {
        float v = to_[i] * 100.0f;             // °C -> 0.01°C
        if (v >  32767.0f) v =  32767.0f;
        if (v < -32768.0f) v = -32768.0f;
        out768[i] = static_cast<int16_t>(v);
    }
    return true;
}

} // namespace patrol
