#ifndef CONFIG_H
#define CONFIG_H

// 串口配置
#define SERIAL_PORT "COM5"
#define SERIAL_BAUD 115200

// 手柄配置
#define DEAD_ZONE 0.05f
#define UPDATE_RATE 60 // Hz

// 速度配置
#define SPEED_SLOW 100
#define SPEED_MEDIUM 500
#define SPEED_FAST 2000

// 转向灵敏度
#define STEERING_SENSITIVITY 1000

// 控制模式
#define MODE_STOP 0x00
#define MODE_JOYSTICK 0x01
#define MODE_BUTTON 0x02

// BLE 配置（JDY-x3 系列使用 Nordic UART Service）
#define BLE_SCAN_TIMEOUT 15000      // BLE 扫描超时（毫秒）
#define BLE_SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define BLE_TX_CHAR_UUID "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define BLE_RX_CHAR_UUID "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

#endif // CONFIG_H
