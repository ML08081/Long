#include "joystickreader.h"
#include <cmath>

#ifdef Q_OS_WIN
#ifdef _MSC_VER
#pragma comment(lib, "Xinput.lib")
#endif
#endif

static const double DEAD_ZONE = 0.05;

JoystickReader::JoystickReader(QObject *parent)
    : QObject(parent)
{
#ifdef Q_OS_WIN
    ZeroMemory(&m_prevState, sizeof(XINPUT_STATE));
#endif
    m_connected = false;
}

JoystickReader::~JoystickReader()
{
}

bool JoystickReader::isConnected() const
{
    return m_connected;
}

void JoystickReader::update()
{
#ifdef Q_OS_WIN
    XINPUT_STATE state;
    ZeroMemory(&state, sizeof(XINPUT_STATE));

    DWORD result = XInputGetState(0, &state);

    if (result == ERROR_SUCCESS) {
        if (!m_connected) {
            m_connected = true;
            emit connected();
        }

        // 左摇杆 X, Y; 右摇杆 X, Y
        double axes[4];
        axes[0] = static_cast<double>(state.Gamepad.sThumbLX) / 32767.0;
        axes[1] = static_cast<double>(state.Gamepad.sThumbLY) / 32767.0;
        axes[2] = static_cast<double>(state.Gamepad.sThumbRX) / 32767.0;
        axes[3] = static_cast<double>(state.Gamepad.sThumbRY) / 32767.0;

        // 死区处理
        for (int i = 0; i < 4; i++) {
            if (fabs(axes[i]) < DEAD_ZONE) {
                axes[i] = 0.0;
            }
            // 检测变化并发送信号
            double prev = 0.0;
            switch (i) {
            case 0: prev = static_cast<double>(m_prevState.Gamepad.sThumbLX) / 32767.0; break;
            case 1: prev = static_cast<double>(m_prevState.Gamepad.sThumbLY) / 32767.0; break;
            case 2: prev = static_cast<double>(m_prevState.Gamepad.sThumbRX) / 32767.0; break;
            case 3: prev = static_cast<double>(m_prevState.Gamepad.sThumbRY) / 32767.0; break;
            }
            if (fabs(prev) < DEAD_ZONE) prev = 0.0;
            if (fabs(axes[i] - prev) > 0.01) {
                emit axisChanged(i, axes[i]);
            }
        }

        // 检测按键（16个按键）
        WORD prevButtons = m_prevState.Gamepad.wButtons;
        WORD currButtons = state.Gamepad.wButtons;

        // 用正确的 XInput button masks
        // wButtons bits: 0x0001=DPadUp, 0x0002=DPadDown, 0x0004=DPadLeft, 0x0008=DPadRight
        // 0x0010=Start, 0x0020=Back, 0x0040=LS, 0x0080=RS
        // 0x0100=LB, 0x0200=RB, 0x1000=A, 0x2000=B, 0x4000=X, 0x8000=Y

        // 检查标准按键变化
        WORD allMasks[] = {0x1000, 0x2000, 0x4000, 0x8000, 0x0100, 0x0200,
                           0x0020, 0x0010, 0x0040, 0x0080, 0x0001, 0x0002,
                           0x0004, 0x0008};
        for (int i = 0; i < 14; i++) {
            bool prevPressed = (prevButtons & allMasks[i]) != 0;
            bool currPressed = (currButtons & allMasks[i]) != 0;
            if (prevPressed != currPressed) {
                emit buttonChanged(i, currPressed);
            }
        }

        // 扳机键（L2/R2）作为按键9/10，基于阈值 > 127
        if ((state.Gamepad.bLeftTrigger > 127) != (m_prevState.Gamepad.bLeftTrigger > 127)) {
            emit buttonChanged(8, state.Gamepad.bLeftTrigger > 127);
        }
        if ((state.Gamepad.bRightTrigger > 127) != (m_prevState.Gamepad.bRightTrigger > 127)) {
            emit buttonChanged(9, state.Gamepad.bRightTrigger > 127);
        }

        m_prevState = state;

    } else {
        if (m_connected) {
            m_connected = false;
            emit disconnected();
        }
    }
#else
    // 非 Windows（龙芯 2K0300 / Linux）：无 XInput，物理手柄不可用。
    // 请使用界面上的“模拟手柄”按钮或键盘进行控制；此处保持空实现。
#endif
}
