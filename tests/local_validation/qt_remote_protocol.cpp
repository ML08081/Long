#include <QCoreApplication>
#include "protocol.h"
#include <cstdio>
#include <cstdlib>

static void check(bool ok,const char* what) {
    if(!ok) { std::fprintf(stderr,"FAIL: %s\n",what);std::exit(1); }
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    Protocol protocol;
    // Bluetooth and WiFi hand-controller paths share this actual UART packer.
    for(int level=100;level<=2000;level+=100) {
        for(int direction : {-1,1}) {
            auto wire=protocol.packCommand(direction*level,direction*1000,0);
            TelemetryData decoded{};
            check(Protocol::unpackFrame(wire,decoded),"checksum/roundtrip");
            check(decoded.speed==direction*level && decoded.steering==direction*1000,
                  "20 speed levels, both directions, independent steering");
            wire[6]=char(quint8(wire[6])^1);
            check(!Protocol::unpackFrame(wire,decoded),"corrupt checksum rejected");
        }
    }
    for(int direction : {-1,1}) {
        auto wire=protocol.packCommand(direction*2500,direction*1500,0);
        TelemetryData decoded{};
        check(Protocol::unpackFrame(wire,decoded) && decoded.speed==direction*2000 &&
              decoded.steering==direction*1000,"speed/steering transport limits");
    }
    TelemetryData stopped{};
    check(Protocol::unpackFrame(protocol.packCommand(0,0,0),stopped) && !stopped.speed,
          "immediate zero preserved");
    std::puts("PASS: real hand-controller UART packer, 20 gears both directions, independent 2000/1000 limits, checksum reject and zero stop");
}
