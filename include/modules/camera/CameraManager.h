#ifndef PATROL_MODULES_CAMERA_CAMERAMANAGER_H
#define PATROL_MODULES_CAMERA_CAMERAMANAGER_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

// =============================================================================
//  CameraManager — 基于 V4L2 的 USB 摄像头采集（仅 Linux）
//
//  设计要点（视频链路测试）：
//   - 优先请求 MJPEG 像素格式：绝大多数 USB 摄像头原生支持，每个采集到的
//     buffer 本身就是一张完整 JPEG，可直接通过网络发给上位机，
//     **无需 OpenCV / libjpeg 再编码**，交叉编译到 LoongArch 最省依赖。
//   - 使用 mmap 流式 I/O，零拷贝读取帧数据。
//   - grabFrame() 返回的指针指向内部 mmap 缓冲区，仅在下次 grabFrame()
//     或 stopStreaming() 之前有效，调用方需尽快用完（如立即发送）。
//
//  若摄像头不支持 MJPEG（只有 YUYV 等原始格式），本测试阶段会打印告警，
//  后续如需支持需引入 JPEG 软编码（留作扩展）。
// =============================================================================

namespace patrol {

class CameraManager {
public:
    CameraManager() = default;
    ~CameraManager();

    CameraManager(const CameraManager&) = delete;
    CameraManager& operator=(const CameraManager&) = delete;

    // 打开设备并协商格式。成功返回 true。
    //   device : 设备节点，如 "/dev/video0"
    //   width/height : 期望分辨率（驱动可能返回最接近的实际值）
    //   fps    : 期望帧率
    bool open(const std::string& device, int width, int height, int fps);

    // 关闭设备，释放所有缓冲区
    void close();

    bool isOpen() const { return fd_ >= 0; }

    // 启动 / 停止取流
    bool startStreaming();
    void stopStreaming();

    // 阻塞获取一帧（最多等待 timeoutMs 毫秒）。
    //   成功：*data 指向帧数据，*size 为字节数，返回 true。
    //   超时/出错：返回 false。
    // 返回的数据有效期至下次 grabFrame()/stopStreaming()。
    bool grabFrame(const uint8_t** data, size_t* size, int timeoutMs = 1000);

    // 实际协商到的参数
    int  width()  const { return width_; }
    int  height() const { return height_; }
    bool isMjpeg() const { return isMjpeg_; }
    const std::string& device() const { return device_; }

private:
    struct MappedBuffer {
        void*  start  = nullptr;
        size_t length = 0;
    };

    bool requeue(int index);          // 把缓冲区放回队列
    void unmapBuffers();

    int          fd_       = -1;
    std::string  device_;
    int          width_    = 0;
    int          height_   = 0;
    int          fps_      = 0;
    bool         isMjpeg_  = false;
    bool         streaming_ = false;
    int          lastIndex_ = -1;     // 上一帧占用的缓冲区，下次取帧前需归还
    std::vector<MappedBuffer> buffers_;
};

} // namespace patrol

#endif // PATROL_MODULES_CAMERA_CAMERAMANAGER_H
