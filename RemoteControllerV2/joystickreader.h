#ifndef JOYSTICKREADER_H
#define JOYSTICKREADER_H

#include <QObject>
#include <QTimer>
#ifdef Q_OS_WIN
#include <windows.h>
#include <Xinput.h>
#endif

class JoystickReader : public QObject
{
    Q_OBJECT
public:
    explicit JoystickReader(QObject *parent = nullptr);
    ~JoystickReader();

    void update();
    bool isConnected() const;

signals:
    void connected();
    void disconnected();
    void axisChanged(int axis, double value);
    void buttonChanged(int button, bool pressed);

private:
#ifdef Q_OS_WIN
    XINPUT_STATE m_prevState;
#endif
    bool m_connected;
};

#endif // JOYSTICKREADER_H
