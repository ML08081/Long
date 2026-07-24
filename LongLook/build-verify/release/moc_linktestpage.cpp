/****************************************************************************
** Meta object code from reading C++ file 'linktestpage.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../linktestpage.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'linktestpage.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_LinkTestPage_t {
    QByteArrayData data[19];
    char stringdata0[183];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_LinkTestPage_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_LinkTestPage_t qt_meta_stringdata_LinkTestPage = {
    {
QT_MOC_LITERAL(0, 0, 12), // "LinkTestPage"
QT_MOC_LITERAL(1, 13, 14), // "onSpeedChanged"
QT_MOC_LITERAL(2, 28, 0), // ""
QT_MOC_LITERAL(3, 29, 1), // "v"
QT_MOC_LITERAL(4, 31, 14), // "onSteerChanged"
QT_MOC_LITERAL(5, 46, 11), // "sendCurrent"
QT_MOC_LITERAL(6, 58, 16), // "onContinuousTick"
QT_MOC_LITERAL(7, 75, 13), // "onModeClicked"
QT_MOC_LITERAL(8, 89, 4), // "mode"
QT_MOC_LITERAL(9, 94, 6), // "onStop"
QT_MOC_LITERAL(10, 101, 7), // "onEstop"
QT_MOC_LITERAL(11, 109, 8), // "onSensor"
QT_MOC_LITERAL(12, 118, 14), // "LL::SensorData"
QT_MOC_LITERAL(13, 133, 1), // "d"
QT_MOC_LITERAL(14, 135, 5), // "onLog"
QT_MOC_LITERAL(15, 141, 3), // "msg"
QT_MOC_LITERAL(16, 145, 11), // "onConnected"
QT_MOC_LITERAL(17, 157, 14), // "onDisconnected"
QT_MOC_LITERAL(18, 172, 10) // "onRateTick"

    },
    "LinkTestPage\0onSpeedChanged\0\0v\0"
    "onSteerChanged\0sendCurrent\0onContinuousTick\0"
    "onModeClicked\0mode\0onStop\0onEstop\0"
    "onSensor\0LL::SensorData\0d\0onLog\0msg\0"
    "onConnected\0onDisconnected\0onRateTick"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_LinkTestPage[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      12,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    1,   74,    2, 0x08 /* Private */,
       4,    1,   77,    2, 0x08 /* Private */,
       5,    0,   80,    2, 0x08 /* Private */,
       6,    0,   81,    2, 0x08 /* Private */,
       7,    1,   82,    2, 0x08 /* Private */,
       9,    0,   85,    2, 0x08 /* Private */,
      10,    0,   86,    2, 0x08 /* Private */,
      11,    1,   87,    2, 0x08 /* Private */,
      14,    1,   90,    2, 0x08 /* Private */,
      16,    0,   93,    2, 0x08 /* Private */,
      17,    0,   94,    2, 0x08 /* Private */,
      18,    0,   95,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,    8,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 12,   13,
    QMetaType::Void, QMetaType::QString,   15,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void LinkTestPage::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<LinkTestPage *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->onSpeedChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 1: _t->onSteerChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 2: _t->sendCurrent(); break;
        case 3: _t->onContinuousTick(); break;
        case 4: _t->onModeClicked((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 5: _t->onStop(); break;
        case 6: _t->onEstop(); break;
        case 7: _t->onSensor((*reinterpret_cast< const LL::SensorData(*)>(_a[1]))); break;
        case 8: _t->onLog((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 9: _t->onConnected(); break;
        case 10: _t->onDisconnected(); break;
        case 11: _t->onRateTick(); break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject LinkTestPage::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_LinkTestPage.data,
    qt_meta_data_LinkTestPage,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *LinkTestPage::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *LinkTestPage::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_LinkTestPage.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int LinkTestPage::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 12)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 12;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
