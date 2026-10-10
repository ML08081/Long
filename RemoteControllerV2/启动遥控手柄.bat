@echo off
chcp 65001 >/dev/null
title 遥控手柄控制器 v2

echo ============================================
echo   🎮 遥控手柄控制器 v2
echo   STM32 F1xc 蓝牙/WiFi 遥控
echo ============================================
echo.

cd /d "%~dp0"

:: 优先使用新版exe
if exist "RemoteController_v2.exe" (
    start /b "" "RemoteController_v2.exe"
) else (
    start /b "" "RemoteController.exe"
)

echo ✅ 程序已启动...
echo 💡 关闭窗口即可退出程序
echo.
pause >/dev/null
