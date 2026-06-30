#include "modules/camera/CameraManager.h"
#include "modules/logger/Logger.h"

#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <linux/videodev2.h>

namespace patrol {

namespace {
int xioctl(int fd, unsigned long request, void* arg) {
    int r;
    do { r = ::ioctl(fd, request, arg); } while (r == -1 && errno == EINTR);
    return r;
}
constexpr int kBufferCount = 4;
} // namespace

CameraManager::~CameraManager() { close(); }

bool CameraManager::open(const std::string& device, int width, int height, int fps) {
    close();
    device_ = device;
    fd_ = ::open(device.c_str(), O_RDWR | O_NONBLOCK, 0);
    if (fd_ < 0) {
        LOG_ERROR("打开摄像头失败 %s: %s", device.c_str(), std::strerror(errno));
        return false;
    }

    // 1) 查询能力
    v4l2_capability cap{};
    if (xioctl(fd_, VIDIOC_QUERYCAP, &cap) < 0) {
        LOG_ERROR("VIDIOC_QUERYCAP 失败: %s", std::strerror(errno));
        close(); return false;
    }
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        LOG_ERROR("%s 不支持视频采集", device.c_str());
        close(); return false;
    }
    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        LOG_ERROR("%s 不支持流式 I/O", device.c_str());
        close(); return false;
    }
    LOG_INFO("摄像头: %s (driver=%s card=%s)",
             device.c_str(),
             reinterpret_cast<const char*>(cap.driver),
             reinterpret_cast<const char*>(cap.card));

    // 2) 设置像素格式：优先 MJPEG
    v4l2_format fmt{};
    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = static_cast<__u32>(width);
    fmt.fmt.pix.height      = static_cast<__u32>(height);
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix.field       = V4L2_FIELD_ANY;
    if (xioctl(fd_, VIDIOC_S_FMT, &fmt) < 0) {
        LOG_ERROR("VIDIOC_S_FMT 失败: %s", std::strerror(errno));
        close(); return false;
    }
    width_   = static_cast<int>(fmt.fmt.pix.width);
    height_  = static_cast<int>(fmt.fmt.pix.height);
    isMjpeg_ = (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_MJPEG);

    if (!isMjpeg_) {
        char fourcc[5] = {
            char( fmt.fmt.pix.pixelformat        & 0xFF),
            char((fmt.fmt.pix.pixelformat >>  8)  & 0xFF),
            char((fmt.fmt.pix.pixelformat >> 16) & 0xFF),
            char((fmt.fmt.pix.pixelformat >> 24) & 0xFF), 0 };
        LOG_WARN("驱动未接受 MJPEG，实际格式 '%s'，上位机可能无法解码", fourcc);
    }
    LOG_INFO("协商分辨率: %dx%d  格式: %s", width_, height_, isMjpeg_ ? "MJPEG" : "非MJPEG");

    // 3) 设置帧率（先记录请求值，再尝试写入驱动）
    fps_ = fps;   // 始终保留请求帧率作为基准值
    v4l2_streamparm parm{};
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(fd_, VIDIOC_G_PARM, &parm) == 0 &&
        (parm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME)) {
        parm.parm.capture.timeperframe.numerator   = 1;
        parm.parm.capture.timeperframe.denominator = static_cast<__u32>(fps);
        if (xioctl(fd_, VIDIOC_S_PARM, &parm) < 0) {
            LOG_WARN("设置帧率 %d 失败: %s（使用驱动默认帧率）", fps, std::strerror(errno));
        } else {
            // 读回驱动实际接受的帧率
            fps_ = static_cast<int>(parm.parm.capture.timeperframe.denominator);
            LOG_INFO("帧率: 请求 %d fps -> 驱动接受 %d fps", fps, fps_);
        }
    } else {
        LOG_INFO("帧率: 驱动不支持 VIDIOC_S_PARM，保持请求值 %d fps", fps_);
    }

    // 4) 申请缓冲区
    v4l2_requestbuffers req{};
    req.count  = kBufferCount;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (xioctl(fd_, VIDIOC_REQBUFS, &req) < 0) {
        LOG_ERROR("VIDIOC_REQBUFS 失败: %s", std::strerror(errno));
        close(); return false;
    }
    if (req.count < 2) {
        LOG_ERROR("缓冲区不足 (%u)", req.count);
        close(); return false;
    }

    // 5) 映射缓冲区
    buffers_.resize(req.count);
    for (unsigned i = 0; i < req.count; ++i) {
        v4l2_buffer buf{};
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;
        if (xioctl(fd_, VIDIOC_QUERYBUF, &buf) < 0) {
            LOG_ERROR("VIDIOC_QUERYBUF[%u] 失败: %s", i, std::strerror(errno));
            close(); return false;
        }
        void* p = ::mmap(nullptr, buf.length, PROT_READ | PROT_WRITE,
                         MAP_SHARED, fd_, buf.m.offset);
        if (p == MAP_FAILED) {
            LOG_ERROR("mmap[%u] 失败: %s", i, std::strerror(errno));
            close(); return false;
        }
        buffers_[i].start  = p;
        buffers_[i].length = buf.length;
    }

    LOG_INFO("摄像头初始化完成，%zu 个 mmap 缓冲区", buffers_.size());
    return true;
}

bool CameraManager::startStreaming() {
    if (fd_ < 0 || streaming_) return streaming_;
    for (size_t i = 0; i < buffers_.size(); ++i)
        if (!requeue(static_cast<int>(i))) return false;
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(fd_, VIDIOC_STREAMON, &type) < 0) {
        LOG_ERROR("VIDIOC_STREAMON 失败: %s", std::strerror(errno));
        return false;
    }
    streaming_ = true;
    lastIndex_ = -1;
    LOG_INFO("开始取流");
    return true;
}

void CameraManager::stopStreaming() {
    if (fd_ < 0 || !streaming_) return;
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    xioctl(fd_, VIDIOC_STREAMOFF, &type);
    streaming_ = false;
    lastIndex_ = -1;
    LOG_INFO("停止取流");
}

bool CameraManager::requeue(int index) {
    v4l2_buffer buf{};
    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index  = static_cast<__u32>(index);
    if (xioctl(fd_, VIDIOC_QBUF, &buf) < 0) {
        LOG_ERROR("VIDIOC_QBUF[%d] 失败: %s", index, std::strerror(errno));
        return false;
    }
    return true;
}

bool CameraManager::grabFrame(const uint8_t** data, size_t* size, int timeoutMs) {
    if (fd_ < 0 || !streaming_) return false;
    if (lastIndex_ >= 0) {
        if (!requeue(lastIndex_)) return false;
        lastIndex_ = -1;
    }
    fd_set fds; FD_ZERO(&fds); FD_SET(fd_, &fds);
    timeval tv{timeoutMs / 1000, (timeoutMs % 1000) * 1000};
    int r = ::select(fd_ + 1, &fds, nullptr, nullptr, &tv);
    if (r < 0) {
        if (errno == EINTR) return false;
        LOG_ERROR("select 失败: %s", std::strerror(errno));
        return false;
    }
    if (r == 0) { LOG_WARN("取帧超时 (%d ms)", timeoutMs); return false; }

    v4l2_buffer buf{};
    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (xioctl(fd_, VIDIOC_DQBUF, &buf) < 0) {
        if (errno == EAGAIN) return false;
        LOG_ERROR("VIDIOC_DQBUF 失败: %s", std::strerror(errno));
        return false;
    }
    lastIndex_ = static_cast<int>(buf.index);
    *data = static_cast<const uint8_t*>(buffers_[buf.index].start);
    *size = buf.bytesused;
    return true;
}

void CameraManager::unmapBuffers() {
    for (auto& b : buffers_)
        if (b.start && b.start != MAP_FAILED)
            ::munmap(b.start, b.length);
    buffers_.clear();
}

void CameraManager::close() {
    if (streaming_) stopStreaming();
    unmapBuffers();
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
    lastIndex_ = -1;
}

} // namespace patrol