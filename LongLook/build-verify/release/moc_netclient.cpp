/****************************************************************************
** Meta object code from reading C++ file 'netclient.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../netclient.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'netclient.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_NetClient_t {
    QByteArrayData data[23];
    char stringdata0[235];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_NetClient_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_NetClient_t qt_meta_stringdata_NetClient = {
    {
QT_MOC_LITERAL(0, 0, 9), // "NetClient"
QT_MOC_LITERAL(1, 10, 9), // "connected"
QT_MOC_LITERAL(2, 20, 0), // ""
QT_MOC_LITERAL(3, 21, 12), // "disconnected"
QT_MOC_LITERAL(4, 34, 10), // "logMessage"
QT_MOC_LITERAL(5, 45, 3), // "msg"
QT_MOC_LITERAL(6, 49, 13), // "sensorUpdated"
QT_MOC_LITERAL(7, 63, 14), // "LL::SensorData"
QT_MOC_LITERAL(8, 78, 4), // "data"
QT_MOC_LITERAL(9, 83, 10), // "videoFrame"
QT_MOC_LITERAL(10, 94, 3), // "img"
QT_MOC_LITERAL(11, 98, 12), // "thermalFrame"
QT_MOC_LITERAL(12, 111, 4), // "minC"
QT_MOC_LITERAL(13, 116, 4), // "maxC"
QT_MOC_LITERAL(14, 121, 9), // "textFrame"
QT_MOC_LITERAL(15, 131, 4), // "text"
QT_MOC_LITERAL(16, 136, 11), // "onConnected"
QT_MOC_LITERAL(17, 148, 14), // "onDisconnected"
QT_MOC_LITERAL(18, 163, 11), // "onReadyRead"
QT_MOC_LITERAL(19, 175, 7), // "onError"
QT_MOC_LITERAL(20, 183, 28), // "QAbstractSocket::SocketError"
QT_MOC_LITERAL(21, 212, 5), // "error"
QT_MOC_LITERAL(22, 218, 16) // "onConnectTimeout"

    },
    "NetClient\0connected\0\0disconnected\0"
    "logMessage\0msg\0sensorUpdated\0"
    "LL::SensorData\0data\0videoFrame\0img\0"
    "thermalFrame\0minC\0maxC\0textFrame\0text\0"
    "onConnected\0onDisconnected\0onReadyRead\0"
    "onError\0QAbstractSocket::SocketError\0"
    "error\0onConnectTimeout"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_NetClient[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      12,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       7,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    0,   74,    2, 0x06 /* Public */,
       3,    0,   75,    2, 0x06 /* Public */,
       4,    1,   76,    2, 0x06 /* Public */,
       6,    1,   79,    2, 0x06 /* Public */,
       9,    1,   82,    2, 0x06 /* Public */,
      11,    3,   85,    2, 0x06 /* Public */,
      14,    1,   92,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
      16,    0,   95,    2, 0x08 /* Private */,
      17,    0,   96,    2, 0x08 /* Private */,
      18,    0,   97,    2, 0x08 /* Private */,
      19,    1,   98,    2, 0x08 /* Private */,
      22,    0,  101,    2, 0x08 /* Private */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,    5,
    QMetaType::Void, 0x80000000 | 7,    8,
    QMetaType::Void, QMetaType::QImage,   10,
    QMetaType::Void, QMetaType::QImage, QMetaType::Double, QMetaType::Double,   10,   12,   13,
    QMetaType::Void, QMetaType::QString,   15,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 20,   21,
    QMetaType::Void,

       0        // eod
};

void NetClient::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<NetClient *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->connected(); break;
        case 1: _t->disconnected(); break;
        case 2: _t->logMessage((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 3: _t->sensorUpdated((*reinterpret_cast< const LL::SensorData(*)>(_a[1]))); break;
        case 4: _t->videoFrame((*reinterpret_cast< const QImage(*)>(_a[1]))); break;
        case 5: _t->thermalFrame((*reinterpret_cast< const QImage(*)>(_a[1])),(*reinterpret_cast< double(*)>(_a[2])),(*reinterpret_cast< double(*)>(_a[3]))); break;
        case 6: _t->textFrame((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 7: _t->onConnected(); break;
        case 8: _t->onDisconnected(); break;
        case 9: _t->onReadyRead(); break;
        case 10: _t->onError((*reinterpret_cast< QAbstractSocket::SocketError(*)>(_a[1]))); break;
        case 11: _t->onConnectTimeout(); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<int*>(_a[0]) = -1; break;
        case 10:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QAbstractSocket::SocketError >(); break;
            }
            break;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (NetClient::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&NetClient::connected)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (NetClient::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&NetClient::disconnected)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (NetClient::*)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&NetClient::logMessage)) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (NetClient::*)(const LL::SensorData & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&NetClient::sensorUpdated)) {
                *result = 3;
                return;
            }
        }
        {
            using _t = void (NetClient::*)(const QImage & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&NetClient::videoFrame)) {
                *result = 4;
                return;
            }
        }
        {
            using _t = void (NetClient::*)(const QImage & , double , double );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&NetClient::thermalFrame)) {
                *result = 5;
                return;
            }
        }
        {
            using _t = void (NetClient::*)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&NetClient::textFrame)) {
                *result = 6;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject NetClient::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_NetClient.data,
    qt_meta_data_NetClient,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *NetClient::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *NetClient::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_NetClient.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int NetClient::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    }
    return _id;
}

// SIGNAL 0
void NetClient::connected()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void NetClient::disconnected()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void NetClient::logMessage(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void NetClient::sensorUpdated(const LL::SensorData & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void NetClient::videoFrame(const QImage & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void NetClient::thermalFrame(const QImage & _t1, double _t2, double _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 5, _a);
}

// SIGNAL 6
void NetClient::textFrame(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 6, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
