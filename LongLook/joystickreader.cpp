#include "joystickreader.h"

#include <cmath>

#ifdef Q_OS_WIN
#pragma comment(lib, "Xinput.lib")
#endif

static const double kDeadZone = 0.05;

JoystickReader::JoystickReader(QObject *parent)
    : QObject(parent)
{
#ifdef Q_OS_WIN
    ZeroMemory(&m_prevState, sizeof(XINPUT_STATE));
#endif
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

    const DWORD result = XInputGetState(0, &state);
    if (result != ERROR_SUCCESS) {
        if (m_connected) {
            m_connected = false;
            emit disconnected();
        }
        return;
    }

    if (!m_connected) {
        m_connected = true;
        emit connected();
    }

    const double axes[4] = {
        static_cast<double>(state.Gamepad.sThumbLX) / 32767.0,
        static_cast<double>(state.Gamepad.sThumbLY) / 32767.0,
        static_cast<double>(state.Gamepad.sThumbRX) / 32767.0,
        static_cast<double>(state.Gamepad.sThumbRY) / 32767.0,
    };
    const double prevAxes[4] = {
        static_cast<double>(m_prevState.Gamepad.sThumbLX) / 32767.0,
        static_cast<double>(m_prevState.Gamepad.sThumbLY) / 32767.0,
        static_cast<double>(m_prevState.Gamepad.sThumbRX) / 32767.0,
        static_cast<double>(m_prevState.Gamepad.sThumbRY) / 32767.0,
    };

    for (int i = 0; i < 4; ++i) {
        double cur = std::fabs(axes[i]) < kDeadZone ? 0.0 : axes[i];
        double prev = std::fabs(prevAxes[i]) < kDeadZone ? 0.0 : prevAxes[i];
        if (std::fabs(cur - prev) > 0.01) {
            emit axisChanged(i, cur);
        }
    }

    const WORD masks[] = {
        0x1000, 0x2000, 0x4000, 0x8000,
        0x0100, 0x0200, 0x0020, 0x0010,
        0x0040, 0x0080, 0x0001, 0x0002,
        0x0004, 0x0008
    };
    for (int i = 0; i < int(sizeof(masks) / sizeof(masks[0])); ++i) {
        const bool prev = (m_prevState.Gamepad.wButtons & masks[i]) != 0;
        const bool cur = (state.Gamepad.wButtons & masks[i]) != 0;
        if (prev != cur) {
            emit buttonChanged(i, cur);
        }
    }

    if ((state.Gamepad.bLeftTrigger > 127) != (m_prevState.Gamepad.bLeftTrigger > 127)) {
        emit buttonChanged(14, state.Gamepad.bLeftTrigger > 127);
    }
    if ((state.Gamepad.bRightTrigger > 127) != (m_prevState.Gamepad.bRightTrigger > 127)) {
        emit buttonChanged(15, state.Gamepad.bRightTrigger > 127);
    }

    m_prevState = state;
#endif
}
