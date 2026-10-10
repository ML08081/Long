#include "modules/thermal/ThermalSolver.h"
#include "modules/logger/Logger.h"
#include <cmath>

namespace patrol {

bool ThermalSolver::setEeprom(const uint16_t* ee832) {
    uint32_t wireHash = 2166136261u;
    int wireBrokenCount = 0;
    int wireOutlierCount = 0;
    for (int i = 0; i < EE_WORDS; ++i) {
        wireHash = (wireHash ^ static_cast<uint8_t>(ee832[i] >> 8)) * 16777619u;
        wireHash = (wireHash ^ static_cast<uint8_t>(ee832[i])) * 16777619u;
        if (i >= 64) {
            if (ee832[i] == 0) ++wireBrokenCount;
            else if ((ee832[i] & 1u) != 0u) ++wireOutlierCount;
        }
    }

    if (ready_ && wireHash==eepromHash_) return true;
    // 先在临时参数上验证。后续重发若损坏，保留上一份已经工作过的标定，
    // 避免一个坏 EEPROM 把正在输出的热成像永久打回未就绪状态。
    uint16_t ee[EE_WORDS];
    for (int i = 0; i < EE_WORDS; ++i) ee[i] = ee832[i];
    paramsMLX90640 candidate{};
    int rc = MLX90640_ExtractParameters(ee, &candidate);
    if (rc != 0) {
        // 早期 F4 热成像传输版本曾出现 16-bit 字内字节互换。不依靠版本号
        // 猜测，而是只在原方向被原厂解析拒绝时，尝试互换后再走完整校验。
        uint32_t correctedHash = 2166136261u;
        int correctedBrokenCount = 0;
        int correctedOutlierCount = 0;
        for (int i = 0; i < EE_WORDS; ++i) {
            ee[i] = static_cast<uint16_t>((ee832[i] >> 8) | (ee832[i] << 8));
            correctedHash = (correctedHash ^ static_cast<uint8_t>(ee[i] >> 8)) * 16777619u;
            correctedHash = (correctedHash ^ static_cast<uint8_t>(ee[i])) * 16777619u;
            if (i >= 64) {
                if (ee[i] == 0) ++correctedBrokenCount;
                else if ((ee[i] & 1u) != 0u) ++correctedOutlierCount;
            }
        }

        paramsMLX90640 corrected{};
        const int correctedRc = MLX90640_ExtractParameters(ee, &corrected);
        if (correctedRc != 0) {
            LOG_WARN("热成像: EEPROM 参数提取失败 wire(rc=%d hash=%08x broken=%d outlier=%d) "
                     "swap(rc=%d hash=%08x broken=%d outlier=%d)，保留既有标定=%s",
                     rc, wireHash, wireBrokenCount, wireOutlierCount,
                     correctedRc, correctedHash, correctedBrokenCount, correctedOutlierCount,
                     ready_ ? "是" : "否");
            return false;
        }

        params_ = corrected;
        ready_ = true; eepromHash_=wireHash; resetFrame();
        LOG_WARN("热成像: EEPROM 字节序已自动校正 wire=%08x corrected=%08x broken=%d outlier=%d",
                 wireHash, correctedHash, correctedBrokenCount, correctedOutlierCount);
        return true;
    }
    params_ = candidate;
    ready_ = true; eepromHash_=wireHash; resetFrame();
    LOG_INFO("热成像: 标定参数就绪（龙芯解算）hash=%08x broken=%d outlier=%d",
             wireHash, wireBrokenCount, wireOutlierCount);
    return true;
}

bool ThermalSolver::solve(const uint16_t* raw834, int16_t* out768) {
    if (!ready_ || raw834[833]>1) return false;

    // MLX90640 API 需要可写的 frameData（内部会用 statusReg 等）。拷贝一份。
    uint16_t frame[FRAME_WORDS];
    for (int i = 0; i < FRAME_WORDS; ++i) frame[i] = raw834[i];

    float ta = MLX90640_GetTa(frame, &params_);
    if (!std::isfinite(ta)) { resetFrame(); return false; }
    float tr = ta - trOffset_;                 // 反射温度经验值
    MLX90640_CalculateTo(frame, &params_, emissivity_, tr, to_);

    pages_ |= static_cast<uint8_t>(1u << raw834[833]);
    if (pages_!=3) return false;
    pages_=0; // every emitted full image contains two new subpages
    // Apply the vendor correction before rejecting non-finite damaged-pixel results.
    int mode=(frame[832]&0x1000U)?1:0;
    MLX90640_BadPixelsCorrection(params_.brokenPixels,to_,mode,&params_);
    MLX90640_BadPixelsCorrection(params_.outlierPixels,to_,mode,&params_);
    for (int i = 0; i < PIXELS; ++i) {
        if (!std::isfinite(to_[i])) return false;
        float v = to_[i] * 100.0f;             // °C -> 0.01°C
        if (v >  32767.0f) v =  32767.0f;
        if (v < -32768.0f) v = -32768.0f;
        out768[i] = static_cast<int16_t>(v);
    }
    return true;
}

} // namespace patrol
