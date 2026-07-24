cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME F4_VGT6first)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    Core/Servo/servo.c
    Core/Servo/servo2.c
    Core/Encoder/encoder.c
    Core/Motor/motor.c
    Core/PID/pid.c
    Core/HC/hc.c
    # OLED 已移除（2026-07-08 硬件取消）
    Core/Protocol/protocol.c
    # ---- CAN 链路抽象层（P1 CAN 迁移，2026-07-17）----
    Core/CAN/can_link.c
    Core/LineTrack/linetrack.c
    Core/Patrol/patrol.c
    Core/Watchdog/watchdog.c
    Core/I2CFix/i2c_recover.c
    Core/Debug/bt_debug.c
    Core/Debug/boot_trace.c
    Core/Buzzer/buzzer.c
    Core/MQ2/mq2.c
    Core/Flame/flame.c
    Core/DHT11/dht11.c
    Core/VL53L0X/vl53l0x.c
    # ---- 热成像（MLX90640）：F4 解算，走 USART1 专线发 0x5B 行帧（2026-07-07 启用）----
    Core/MLX90640/MLX90640_I2C_Driver.c
    Core/MLX90640/MLX90640_API.c
    Core/MLX90640/thermal.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    Core/Servo
    Core/Encoder
    Core/Motor
    Core/PID
    Core/HC
    Core/Protocol
    Core/CAN
    Core/LineTrack
    Core/Patrol
    Core/Watchdog
    Core/I2CFix
    Core/Debug
    Core/Buzzer
    Core/MQ2
    Core/Flame
    Core/DHT11
    Core/VL53L0X
    Core/MLX90640

)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
