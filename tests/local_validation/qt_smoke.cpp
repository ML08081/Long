#include <QApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QSignalSpy>
#include <QTest>
#include <QSpinBox>
#include <QComboBox>
#include <QLabel>
#include <QtEndian>
#include <QElapsedTimer>
#include "mainwindow.h"
#include "remotepanel.h"
#include "netclient.h"
#include "imageview.h"
#include <cstdio>
#include <cstdlib>
static void check(bool v,const char* what) { if(!v) { std::fprintf(stderr,"FAIL: %s\n",what); std::exit(1); } }
static QByteArray frame(quint8 type,const QByteArray& p) {
    QByteArray b(6,0); b[0]=char(0xA5); b[1]=char(type);
    qToLittleEndian<quint32>(p.size(),reinterpret_cast<uchar*>(b.data()+2)); return b+p;
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    MainWindow window;
    NetClient *net=window.findChild<NetClient*>(); check(net,"client");
    if(app.arguments().contains("--live-stationary")) {
        // Explicit opt-in hardware path: sends only ESTOP and thermal preference.
        int thermalCount=0; LL::SensorData latest{}; QByteArray status;
        QObject::connect(net,&NetClient::thermalFrame,[&](const QImage&,double,double){++thermalCount;});
        QObject::connect(net,&NetClient::sensorUpdated,[&](const LL::SensorData& s){latest=s;});
        QObject::connect(net,&NetClient::deviceStatus,[&](const QByteArray& p){status=p;});
        auto wait=[&](auto pred,int ms){QElapsedTimer t;t.start();while(t.elapsed()<ms){QTest::qWait(20);if(pred())return true;}return false;};
        auto liveCheck=[&](bool ok,const char* message){if(!ok){net->sendCommand(LL::CMD_ESTOP,1);net->requestThermal(0);QTest::qWait(500);net->disconnectFromHost();check(false,message);}};
        const QString host = qEnvironmentVariable("PATROL_TEST_HOST");
        check(!host.isEmpty(),"PATROL_TEST_HOST required for hardware test");
        net->connectToHost(host,8080);
        liveCheck(wait([&]{return net->isConnected() && status.size()==27;},5000),"hardware connection/status");
        net->sendCommand(LL::CMD_ESTOP,1);
        liveCheck(net->requestThermal(1),"hardware thermal ON request");
        liveCheck(wait([&]{return thermalCount>=4 && latest.dhtOk();},12000),"hardware live thermal and DHT samples");
        auto views=window.findChildren<ImageView*>();liveCheck(views.size()==2,"two image views");
        auto thermal=views.last();thermal->setFixedSize(320,240);liveCheck(thermal->hasImage(),"real thermal image displayed");
        liveCheck(net->requestThermal(2),"hardware thermal OFF request");
        liveCheck(wait([&]{return status.size()==27 && quint8(status[5])==0 && quint8(status[6])==2;},5000),"actual F4 stopped");
        QTest::qWait(500);auto image=thermal->grab().toImage();const int stoppedCount=thermalCount;
        QTest::qWait(3500);
        liveCheck(thermal->hasImage() && thermal->grab().toImage()==image && thermalCount==stoppedCount,"stopped real frame retained pixel-for-pixel");
        bool frozen=false;for(auto label:window.findChildren<QLabel*>())frozen|=label->text().contains("保留最后一帧");
        liveCheck(frozen,"real stopped frame explicitly marked frozen");
        liveCheck(latest.dhtOk(),"DHT still live after thermal OFF");
        net->requestThermal(0);net->sendCommand(LL::CMD_ESTOP,1);QTest::qWait(500);net->disconnectFromHost();QTest::qWait(100);
        liveCheck(thermal->hasImage() && thermal->grab().toImage()==image,"real frame held on disconnect");
        bool valuesHeld=false;for(auto label:window.findChildren<QLabel*>())valuesHeld|=label->text().contains("°C") && label->text().contains("保留");
        check(valuesHeld,"real temperature retained on disconnect");
        std::puts("PASS: actual F4/Loongson/Qt live DHT and thermal, OFF freezes image pixel-for-pixel, disconnect holds image and values; zero drive commands; restored AUTO/ESTOP");
        return 0;
    }
    QTcpServer server; check(server.listen(QHostAddress::LocalHost,0),"local TCP listen");
    net->connectToHost("127.0.0.1",server.serverPort());
    for(int i=0;i<100 && !server.hasPendingConnections();++i) QTest::qWait(10);
    check(server.hasPendingConnections(),"client connect");
    auto peer=server.nextPendingConnection(); QTest::qWait(30); peer->readAll();
    RemotePanel remote(net);
    auto settings=window.findChildren<QSpinBox*>(); check(settings.size()==1,"main speed control");
    QSpinBox* mainSpeed=settings[0];
    check(mainSpeed->minimum()==100 && mainSpeed->maximum()==2000 && mainSpeed->singleStep()==100 && mainSpeed->value()==500,"20 gears/default");
    for(int g=0;g<=19;++g) {
        mainSpeed->setValue(100+100*g); QTest::qWait(5);
        check(net->speedLimit()==100+100*g,"main speed shared");
        check(remote.findChild<QSpinBox*>()->value()==100+100*g,"remote sync");
    }
    mainSpeed->setValue(425); check(mainSpeed->value()==400 && net->speedLimit()==400,"keyboard quantization");
    QMetaObject::invokeMethod(&remote,"onPadButton",Qt::DirectConnection,Q_ARG(int,4),Q_ARG(bool,true));
    check(mainSpeed->value()==300 && net->speedLimit()==300,"LB subtract 100");
    QMetaObject::invokeMethod(&remote,"onPadButton",Qt::DirectConnection,Q_ARG(int,5),Q_ARG(bool,true));
    check(mainSpeed->value()==400,"RB add 100");
    QMetaObject::invokeMethod(&remote,"onPadButton",Qt::DirectConnection,Q_ARG(int,6),Q_ARG(bool,true));
    check(mainSpeed->value()==500,"Back default 500");
    // Device acknowledgement is separate from the requested setting.
    QSignalSpy statuses(net,&NetClient::deviceStatus);
    auto sendStatus=[&](const QByteArray& payload) {
        const int previous=statuses.count();peer->write(frame(2,payload));peer->flush();
        QElapsedTimer timer;timer.start();
        while(statuses.count()==previous && timer.elapsed()<1000) QTest::qWait(5);
        check(statuses.count()==previous+1,"status fully processed before UI assertion");
    };
    QByteArray p(27,0); p[0]=1;p[1]=1;p[2]=2;p[3]=4;p[4]=4;p[5]=0;p[6]=2;p[10]=4;p[11]=7;p[12]=5;p[18]=2;p[25]=7;p[26]=1;
    peer->write(frame(2,p).left(8)); peer->flush(); QTest::qWait(10);
    check(statuses.count()==0,"partial frame held");
    peer->write(frame(2,p).mid(8)); peer->flush(); QTest::qWait(40);
    check(statuses.count()==1,"device status dispatch");
    bool off=false,confirmed=false;
    for(auto label:window.findChildren<QLabel*>()) {
        off|=label->text().contains("已关闭"); confirmed|=label->text().contains("已确认");
    }
    check(off && confirmed,"confirmed OFF visible");
    p[5]=char(255);peer->write(frame(2,p));peer->flush();QTest::qWait(20);
    check(statuses.count()==1,"invalid device state rejected");
    p[5]=2;p[8]=32;sendStatus(p);
    bool override=false;
    for(auto label:window.findChildren<QLabel*>()) override|=label->text().contains("视觉火焰");
    check(override,"alarm overrides closed preview visible");
    check(net->requestSpeed(700),"new speed request");
    sendStatus(p);
    check(net->speedLimit()==700 && mainSpeed->value()==700,"old status cannot undo new request");
    p[3]=6;p[10]=6;sendStatus(p);
    check(net->speedLimit()==700,"new setting confirmed");
    check(net->requestThermal(1),"thermal request");
    sendStatus(p);
    auto combos=window.findChildren<QComboBox*>();bool preview=false;
    for(auto combo:combos) if(combo->itemText(0).contains("开机预览")) preview=combo->currentIndex()==1;
    check(preview,"old status cannot undo preview request");
    p[2]=1;p[6]=1;sendStatus(p);
    check(net->requestSpeed(2000),"maximum 2000 request");p[3]=19;p[10]=19;sendStatus(p);
    check(mainSpeed->value()==2000,"2000 device confirmation accepted");
    QTest::qWait(20);peer->readAll();net->sendDrive(2500,1500);QTest::qWait(20);
    auto maximumDrive=peer->readAll();check(maximumDrive.size()==10,"maximum drive sent");
    check(qFromLittleEndian<qint16>(reinterpret_cast<const uchar*>(maximumDrive.constData()+6))==2000,"2000 drive reaches transport");
    check(qFromLittleEndian<qint16>(reinterpret_cast<const uchar*>(maximumDrive.constData()+8))==1000,"steering range remains 1000");
    check(!net->requestSpeed(2100),"out-of-range speed rejected while connected");
    auto malformed=p;malformed[3]=20;malformed[10]=20;
    int accepted=statuses.count();peer->write(frame(2,malformed));peer->flush();QTest::qWait(20);
    check(statuses.count()==accepted && mainSpeed->value()==2000,"20th index rejected; max remains gear19");
    check(net->requestSpeed(500),"restore speed");p[3]=4;p[10]=4;
    sendStatus(p);
    QComboBox* modeChoice=nullptr;
    for(auto combo:window.findChildren<QComboBox*>())if(combo->itemText(0)=="遥控模式")modeChoice=combo;
    check(modeChoice,"mode selector");peer->readAll();
    modeChoice->setCurrentIndex(1);QTest::qWait(20);
    check(peer->readAll().isEmpty(),"programmatic mode echo cannot rearm ESTOP");
    modeChoice->setCurrentIndex(0);
    check(QMetaObject::invokeMethod(modeChoice,"activated",Qt::DirectConnection,Q_ARG(int,0)),"user reselects current manual mode");
    QTest::qWait(20);auto rearm=peer->readAll();
    check(rearm.size()==8 && quint8(rearm[6])==LL::CMD_MODE && quint8(rearm[7])==0,"manual mode reselect sends explicit rearm command");
    peer->readAll();net->sendDrive(1000,0);QTest::qWait(20);
    auto drive=peer->readAll();check(drive.size()>=10,"drive sent");
    check(qFromLittleEndian<qint16>(reinterpret_cast<const uchar*>(drive.constData()+6))==500,"drive clamp shared");
    // Preserve values when sources stop; invalid zero payloads must not erase them.
    QByteArray sample(42,0);qToLittleEndian<qint16>(260,reinterpret_cast<uchar*>(sample.data()+4));
    qToLittleEndian<quint16>(600,reinterpret_cast<uchar*>(sample.data()+6));sample[35]=4;
    peer->write(frame(1,sample));peer->flush();QTest::qWait(20);
    sample[33]=2;sample[35]=0;qToLittleEndian<qint16>(0,reinterpret_cast<uchar*>(sample.data()+4));
    peer->write(frame(1,sample));peer->flush();QTest::qWait(20);
    bool heldTemperature=false;
    for(auto label:window.findChildren<QLabel*>()) heldTemperature|=label->text().contains("26.0") && label->text().contains("保留");
    check(heldTemperature,"last temperature retained on source outage");
    LL::SensorData calibration{};calibration.fault=LL::FAULT_LINE_CALIBRATION;calibration.mode=1;
    check(QMetaObject::invokeMethod(&window,"onSensorUpdated",Qt::DirectConnection,Q_ARG(LL::SensorData,calibration)),"line calibration status");
    bool waiting=false;for(auto label:window.findChildren<QLabel*>())waiting|=label->text().contains("循迹待标定，已停车");
    check(waiting,"uncalibrated patrol has visible reason");
    calibration.mode=0;
    check(QMetaObject::invokeMethod(&window,"onSensorUpdated",Qt::DirectConnection,Q_ARG(LL::SensorData,calibration)),"manual calibration status");
    waiting=false;for(auto label:window.findChildren<QLabel*>())waiting|=label->text().contains("遥控可用");
    check(waiting,"uncalibrated sensor does not disable remote mode");
    LL::SensorData locked;locked.flags=0x80;
    check(QMetaObject::invokeMethod(&window,"onSensorUpdated",Qt::DirectConnection,Q_ARG(LL::SensorData,locked)),"obstacle lock slot");
    bool lockVisible=false;
    for(auto label:window.findChildren<QLabel*>())lockVisible|=label->text().contains("避障锁定") && label->text().contains("两次");
    check(lockVisible,"obstacle lock and real operator unlock instructions visible");
    locked.fault=LL::FAULT_MOTION;
    check(QMetaObject::invokeMethod(&window,"onSensorUpdated",Qt::DirectConnection,Q_ARG(LL::SensorData,locked)),"motion fault slot");
    bool faultVisible=false;
    for(auto label:window.findChildren<QLabel*>())faultVisible|=label->text().contains("电机反馈异常，已停机");
    check(faultVisible,"motion fault takes priority over obstacle lock");
    QImage thermal(32,24,QImage::Format_RGB888);thermal.fill(Qt::red);
    check(QMetaObject::invokeMethod(&window,"onThermalFrame",Qt::DirectConnection,Q_ARG(QImage,thermal),Q_ARG(double,24.0),Q_ARG(double,30.0)),"thermal frame slot");
    p[5]=0;p[2]=2;p[6]=2;p[24]=0;sendStatus(p);
    bool frozen=false,imageHeld=false;
    for(auto label:window.findChildren<QLabel*>()) frozen|=label->text().contains("保留最后一帧");
    for(auto view:window.findChildren<ImageView*>()) imageHeld|=view->hasImage();
    check(frozen && imageHeld,"thermal OFF retains last image with frozen label");
    net->disconnectFromHost();QTest::qWait(20);
    imageHeld=false;for(auto view:window.findChildren<ImageView*>()) imageHeld|=view->hasImage();
    check(imageHeld,"disconnect retains frozen image");
    check(!net->requestSpeed(100),"disconnected request rejected");
    check(!net->requestSpeed(425),"invalid gear rejected");
    std::puts("PASS: Qt main/remote controls, 20 gears, 2000 drive, keyboard snapping, pad stepping, frame fragmentation, confirmed OFF/alarm status, malformed state and drive clamp");
}
