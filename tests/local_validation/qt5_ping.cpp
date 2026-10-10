#include <QCoreApplication>
#include <QTcpServer>
#include <QSignalSpy>
#include <QElapsedTimer>
#include <QTest>
#include "wifimanager.h"
#include <cstdio>
#include <cstdlib>
static void check(bool v,const char* s) { if(!v){std::fprintf(stderr,"FAIL: %s\n",s);std::exit(1);} }
int main(int argc,char**argv) {
 QCoreApplication app(argc,argv);
 WiFiManager wifi; QTcpServer server;check(server.listen(QHostAddress::LocalHost,0),"listen");
 QSignalSpy results(&wifi,&WiFiManager::pingResult);
 QElapsedTimer timer;timer.start();wifi.pingHost("127.0.0.1",server.serverPort(),3000);
 check(timer.elapsed()<100,"ping does not block GUI");
 for(int i=0;i<50 && results.count()==0;i++)QTest::qWait(10);
 check(results.count()==1 && results[0][0].toBool(),"one success after call returned");
 auto port=server.serverPort();server.close();results.clear();
 wifi.pingHost("127.0.0.1",port,100);QTest::qWait(300);
 check(results.count()==1 && !results[0][0].toBool(),"one failure without duplicate timeout");
 std::puts("PASS: direct controller asynchronous ping lifetime, responsive return, success and failure deduplication");
}
