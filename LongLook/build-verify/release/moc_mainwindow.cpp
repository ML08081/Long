/****************************************************************************
** Meta object code from reading C++ file 'mainwindow.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../mainwindow.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'mainwindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_MainWindow_t {
    QByteArrayData data[28];
    char stringdata0[292];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_MainWindow_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_MainWindow_t qt_meta_stringdata_MainWindow = {
    {
QT_MOC_LITERAL(0, 0, 10), // "MainWindow"
QT_MOC_LITERAL(1, 11, 16), // "onConnectClicked"
QT_MOC_LITERAL(2, 28, 0), // ""
QT_MOC_LITERAL(3, 29, 19), // "onDisconnectClicked"
QT_MOC_LITERAL(4, 49, 11), // "onConnected"
QT_MOC_LITERAL(5, 61, 14), // "onDisconnected"
QT_MOC_LITERAL(6, 76, 5), // "onLog"
QT_MOC_LITERAL(7, 82, 3), // "msg"
QT_MOC_LITERAL(8, 86, 15), // "onSensorUpdated"
QT_MOC_LITERAL(9, 102, 14), // "LL::SensorData"
QT_MOC_LITERAL(10, 117, 1), // "d"
QT_MOC_LITERAL(11, 119, 12), // "onVideoFrame"
QT_MOC_LITERAL(12, 132, 3), // "img"
QT_MOC_LITERAL(13, 136, 14), // "onThermalFrame"
QT_MOC_LITERAL(14, 151, 4), // "minC"
QT_MOC_LITERAL(15, 156, 4), // "maxC"
QT_MOC_LITERAL(16, 161, 11), // "onTextFrame"
QT_MOC_LITERAL(17, 173, 4), // "text"
QT_MOC_LITERAL(18, 178, 10), // "onRateTick"
QT_MOC_LITERAL(19, 189, 12), // "onFanToggled"
QT_MOC_LITERAL(20, 202, 2), // "on"
QT_MOC_LITERAL(21, 205, 15), // "onBuzzerToggled"
QT_MOC_LITERAL(22, 221, 14), // "onRelayToggled"
QT_MOC_LITERAL(23, 236, 12), // "onLedToggled"
QT_MOC_LITERAL(24, 249, 13), // "onModeChanged"
QT_MOC_LITERAL(25, 263, 5), // "index"
QT_MOC_LITERAL(26, 269, 7), // "onEstop"
QT_MOC_LITERAL(27, 277, 14) // "onOpenLinkTest"

    },
    "MainWindow\0onConnectClicked\0\0"
    "onDisconnectClicked\0onConnected\0"
    "onDisconnected\0onLog\0msg\0onSensorUpdated\0"
    "LL::SensorData\0d\0onVideoFrame\0img\0"
    "onThermalFrame\0minC\0maxC\0onTextFrame\0"
    "text\0onRateTick\0onFanToggled\0on\0"
    "onBuzzerToggled\0onRelayToggled\0"
    "onLedToggled\0onModeChanged\0index\0"
    "onEstop\0onOpenLinkTest"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_MainWindow[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      17,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    0,   99,    2, 0x08 /* Private */,
       3,    0,  100,    2, 0x08 /* Private */,
       4,    0,  101,    2, 0x08 /* Private */,
       5,    0,  102,    2, 0x08 /* Private */,
       6,    1,  103,    2, 0x08 /* Private */,
       8,    1,  106,    2, 0x08 /* Private */,
      11,    1,  109,    2, 0x08 /* Private */,
      13,    3,  112,    2, 0x08 /* Private */,
      16,    1,  119,    2, 0x08 /* Private */,
      18,    0,  122,    2, 0x08 /* Private */,
      19,    1,  123,    2, 0x08 /* Private */,
      21,    1,  126,    2, 0x08 /* Private */,
      22,    1,  129,    2, 0x08 /* Private */,
      23,    1,  132,    2, 0x08 /* Private */,
      24,    1,  135,    2, 0x08 /* Private */,
      26,    0,  138,    2, 0x08 /* Private */,
      27,    0,  139,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,    7,
    QMetaType::Void, 0x80000000 | 9,   10,
    QMetaType::Void, QMetaType::QImage,   12,
    QMetaType::Void, QMetaType::QImage, QMetaType::Double, QMetaType::Double,   12,   14,   15,
    QMetaType::Void, QMetaType::QString,   17,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,   20,
    QMetaType::Void, QMetaType::Bool,   20,
    QMetaType::Void, QMetaType::Bool,   20,
    QMetaType::Void, QMetaType::Bool,   20,
    QMetaType::Void, QMetaType::Int,   25,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<MainWindow *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->onConnectClicked(); break;
        case 1: _t->onDisconnectClicked(); break;
        case 2: _t->onConnected(); break;
        case 3: _t->onDisconnected(); break;
        case 4: _t->onLog((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 5: _t->onSensorUpdated((*reinterpret_cast< const LL::SensorData(*)>(_a[1]))); break;
        case 6: _t->onVideoFrame((*reinterpret_cast< const QImage(*)>(_a[1]))); break;
        case 7: _t->onThermalFrame((*reinterpret_cast< const QImage(*)>(_a[1])),(*reinterpret_cast< double(*)>(_a[2])),(*reinterpret_cast< double(*)>(_a[3]))); break;
        case 8: _t->onTextFrame((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 9: _t->onRateTick(); break;
        case 10: _t->onFanToggled((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 11: _t->onBuzzerToggled((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 12: _t->onRelayToggled((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 13: _t->onLedToggled((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 14: _t->onModeChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 15: _t->onEstop(); break;
        case 16: _t->onOpenLinkTest(); break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject MainWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_meta_stringdata_MainWindow.data,
    qt_meta_data_MainWindow,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_MainWindow.stringdata0))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 17)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 17;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 17)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 17;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
