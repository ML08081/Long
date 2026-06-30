# =============================================================================
#  PatrolSystem -- 龙芯 2K0300 智能巡检系统（下位机）
#
#  本地编译（WSL x86_64）: bash scripts/build_local.sh
#  龙芯交叉编译:          bash scripts/build_loong.sh
# =============================================================================
cmake_minimum_required(VERSION 3.10)
project(PatrolSystem LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_compile_options(-Wall -Wextra)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
include_directories(${CMAKE_SOURCE_DIR}/include)

# ---- 阶段一：视频链路 + 配置加载（当前编译目标）--------------------------
set(PATROL_SOURCES
    src/main.cpp
    src/app/Application.cpp
    src/modules/logger/Logger.cpp
    src/modules/camera/CameraManager.cpp
    src/modules/network/TcpServer.cpp
    src/modules/config/ConfigManager.cpp
)

# ---- 阶段二：串口 + 运动控制（硬件确认后取消注释）------------------------
#    src/modules/serial/SerialManager.cpp
#    src/modules/network/TcpClient.cpp
#    src/business/RobotController.cpp
#    src/business/PatrolController.cpp
#    src/business/MissionManager.cpp

add_executable(patrol_system ${PATROL_SOURCES})

find_package(Threads REQUIRED)
target_link_libraries(patrol_system PRIVATE Threads::Threads)

option(BUILD_TESTS "构建链路集成测试 link_test" OFF)
if(BUILD_TESTS)
    add_executable(link_test
        test/link_test.cpp
        src/modules/network/TcpServer.cpp
        src/modules/logger/Logger.cpp)
    target_link_libraries(link_test PRIVATE Threads::Threads)
endif()

install(TARGETS patrol_system RUNTIME DESTINATION bin)
install(FILES config/config.json DESTINATION etc/patrol OPTIONAL)
install(FILES config/patrol.json DESTINATION etc/patrol OPTIONAL)

message(STATUS "================ PatrolSystem ================")
message(STATUS "  目标系统   : ${CMAKE_SYSTEM_NAME} / ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "  C++ 编译器 : ${CMAKE_CXX_COMPILER}")
message(STATUS "  构建类型   : ${CMAKE_BUILD_TYPE}")
message(STATUS "  构建测试   : ${BUILD_TESTS}")
message(STATUS "=============================================")