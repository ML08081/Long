#include "business/PatrolController.h"
#include "modules/logger/Logger.h"

namespace patrol {

bool PatrolController::start(const std::vector<Waypoint>& path, DoneCallback cb) {
    if (path.empty()) { LOG_WARN("巡检路径为空"); return false; }
    path_      = path;
    currentWp_ = 0;
    running_   = true;
    doneCb_    = std::move(cb);
    LOG_INFO("巡检启动，共 %zu 个路径点", path_.size());
    return true;
}

void PatrolController::pause()  { running_ = false; LOG_INFO("巡检暂停"); }
void PatrolController::resume() { running_ = true;  LOG_INFO("巡检恢复"); }

void PatrolController::abort() {
    running_ = false;
    currentWp_ = 0;
    if (doneCb_) doneCb_(false);
    LOG_INFO("巡检中止");
}

void PatrolController::tick() {
    if (!running_ || currentWp_ >= path_.size()) return;
    // TODO: 到达判断 + 驱动运动控制器前进
    //       路径点切换：currentWp_++
    //       全部完成：doneCb_(true)
}

} // namespace patrol