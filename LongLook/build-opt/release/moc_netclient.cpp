/****************************************************************************
** Meta object code from reading C++ file 'netclient.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../netclient.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'netclient.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN9NetClientE_t {};
} // unnamed namespace

template <> constexpr inline auto NetClient::qt_create_metaobjectdata<qt_meta_tag_ZN9NetClientE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "NetClient",
        "connected",
        "",
        "disconnected",
        "connecting",
        "attempt",
        "maxAttempts",
        "logMessage",
        "msg",
        "sensorUpdated",
        "LL::SensorData",
        "data",
        "pidTeleUpdated",
        "LL::PidSample",
        "s",
        "videoFrame",
        "QImage",
        "img",
        "thermalFrame",
        "minC",
        "maxC",
        "textFrame",
        "text",
        "discovered",
        "ip",
        "port",
        "version",
        "onConnected",
        "onDisconnected",
        "onReadyRead",
        "onError",
        "QAbstractSocket::SocketError",
        "error",
        "onConnectTimeout",
        "onDiscoveryDatagram"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'connected'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'disconnected'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'connecting'
        QtMocHelpers::SignalData<void(int, int)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 5 }, { QMetaType::Int, 6 },
        }}),
        // Signal 'logMessage'
        QtMocHelpers::SignalData<void(const QString &)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 8 },
        }}),
        // Signal 'sensorUpdated'
        QtMocHelpers::SignalData<void(const LL::SensorData &)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 10, 11 },
        }}),
        // Signal 'pidTeleUpdated'
        QtMocHelpers::SignalData<void(const LL::PidSample &)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 13, 14 },
        }}),
        // Signal 'videoFrame'
        QtMocHelpers::SignalData<void(const QImage &)>(15, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 16, 17 },
        }}),
        // Signal 'thermalFrame'
        QtMocHelpers::SignalData<void(const QImage &, double, double)>(18, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 16, 17 }, { QMetaType::Double, 19 }, { QMetaType::Double, 20 },
        }}),
        // Signal 'textFrame'
        QtMocHelpers::SignalData<void(const QString &)>(21, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 22 },
        }}),
        // Signal 'discovered'
        QtMocHelpers::SignalData<void(const QString &, quint16, const QString &)>(23, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 24 }, { QMetaType::UShort, 25 }, { QMetaType::QString, 26 },
        }}),
        // Slot 'onConnected'
        QtMocHelpers::SlotData<void()>(27, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onDisconnected'
        QtMocHelpers::SlotData<void()>(28, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onReadyRead'
        QtMocHelpers::SlotData<void()>(29, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onError'
        QtMocHelpers::SlotData<void(QAbstractSocket::SocketError)>(30, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 31, 32 },
        }}),
        // Slot 'onConnectTimeout'
        QtMocHelpers::SlotData<void()>(33, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onDiscoveryDatagram'
        QtMocHelpers::SlotData<void()>(34, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<NetClient, qt_meta_tag_ZN9NetClientE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject NetClient::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9NetClientE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9NetClientE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN9NetClientE_t>.metaTypes,
    nullptr
} };

void NetClient::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<NetClient *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->connected(); break;
        case 1: _t->disconnected(); break;
        case 2: _t->connecting((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 3: _t->logMessage((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 4: _t->sensorUpdated((*reinterpret_cast<std::add_pointer_t<LL::SensorData>>(_a[1]))); break;
        case 5: _t->pidTeleUpdated((*reinterpret_cast<std::add_pointer_t<LL::PidSample>>(_a[1]))); break;
        case 6: _t->videoFrame((*reinterpret_cast<std::add_pointer_t<QImage>>(_a[1]))); break;
        case 7: _t->thermalFrame((*reinterpret_cast<std::add_pointer_t<QImage>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3]))); break;
        case 8: _t->textFrame((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 9: _t->discovered((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<quint16>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 10: _t->onConnected(); break;
        case 11: _t->onDisconnected(); break;
        case 12: _t->onReadyRead(); break;
        case 13: _t->onError((*reinterpret_cast<std::add_pointer_t<QAbstractSocket::SocketError>>(_a[1]))); break;
        case 14: _t->onConnectTimeout(); break;
        case 15: _t->onDiscoveryDatagram(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 13:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QAbstractSocket::SocketError >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (NetClient::*)()>(_a, &NetClient::connected, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (NetClient::*)()>(_a, &NetClient::disconnected, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (NetClient::*)(int , int )>(_a, &NetClient::connecting, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (NetClient::*)(const QString & )>(_a, &NetClient::logMessage, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (NetClient::*)(const LL::SensorData & )>(_a, &NetClient::sensorUpdated, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (NetClient::*)(const LL::PidSample & )>(_a, &NetClient::pidTeleUpdated, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (NetClient::*)(const QImage & )>(_a, &NetClient::videoFrame, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (NetClient::*)(const QImage & , double , double )>(_a, &NetClient::thermalFrame, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (NetClient::*)(const QString & )>(_a, &NetClient::textFrame, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (NetClient::*)(const QString & , quint16 , const QString & )>(_a, &NetClient::discovered, 9))
            return;
    }
}

const QMetaObject *NetClient::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *NetClient::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9NetClientE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int NetClient::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 16)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 16;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 16)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 16;
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
void NetClient::connecting(int _t1, int _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2);
}

// SIGNAL 3
void NetClient::logMessage(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void NetClient::sensorUpdated(const LL::SensorData & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}

// SIGNAL 5
void NetClient::pidTeleUpdated(const LL::PidSample & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void NetClient::videoFrame(const QImage & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1);
}

// SIGNAL 7
void NetClient::thermalFrame(const QImage & _t1, double _t2, double _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1, _t2, _t3);
}

// SIGNAL 8
void NetClient::textFrame(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 8, nullptr, _t1);
}

// SIGNAL 9
void NetClient::discovered(const QString & _t1, quint16 _t2, const QString & _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 9, nullptr, _t1, _t2, _t3);
}
QT_WARNING_POP
