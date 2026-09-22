/****************************************************************************
** Meta object code from reading C++ file 'common-quimhelpertoolbar.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qt4/toolbar/common-quimhelpertoolbar.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'common-quimhelpertoolbar.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.10.2. It"
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
struct qt_meta_tag_ZN17QUimHelperToolbarE_t {};
} // unnamed namespace

template <> constexpr inline auto QUimHelperToolbar::qt_create_metaobjectdata<qt_meta_tag_ZN17QUimHelperToolbarE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "QUimHelperToolbar",
        "quitToolbar",
        "",
        "toolbarResized",
        "menuRequested",
        "QMenu*",
        "menu",
        "slotExecPref",
        "contextMenu",
        "slotExecImSwitcher",
        "slotExecDict",
        "slotExecInputPad",
        "slotExecHandwritingInputPad",
        "slotExecHelp",
        "slotIndicatorResized"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'quitToolbar'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'toolbarResized'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'menuRequested'
        QtMocHelpers::SignalData<void(QMenu *)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 5, 6 },
        }}),
        // Slot 'slotExecPref'
        QtMocHelpers::SlotData<void()>(7, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'contextMenu'
        QtMocHelpers::SlotData<QMenu *()>(8, 2, QMC::AccessPublic, 0x80000000 | 5),
        // Slot 'slotExecImSwitcher'
        QtMocHelpers::SlotData<void()>(9, 2, QMC::AccessProtected, QMetaType::Void),
        // Slot 'slotExecDict'
        QtMocHelpers::SlotData<void()>(10, 2, QMC::AccessProtected, QMetaType::Void),
        // Slot 'slotExecInputPad'
        QtMocHelpers::SlotData<void()>(11, 2, QMC::AccessProtected, QMetaType::Void),
        // Slot 'slotExecHandwritingInputPad'
        QtMocHelpers::SlotData<void()>(12, 2, QMC::AccessProtected, QMetaType::Void),
        // Slot 'slotExecHelp'
        QtMocHelpers::SlotData<void()>(13, 2, QMC::AccessProtected, QMetaType::Void),
        // Slot 'slotIndicatorResized'
        QtMocHelpers::SlotData<void()>(14, 2, QMC::AccessProtected, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<QUimHelperToolbar, qt_meta_tag_ZN17QUimHelperToolbarE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject QUimHelperToolbar::staticMetaObject = { {
    QMetaObject::SuperData::link<QFrame::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17QUimHelperToolbarE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17QUimHelperToolbarE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN17QUimHelperToolbarE_t>.metaTypes,
    nullptr
} };

void QUimHelperToolbar::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<QUimHelperToolbar *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->quitToolbar(); break;
        case 1: _t->toolbarResized(); break;
        case 2: _t->menuRequested((*reinterpret_cast<std::add_pointer_t<QMenu*>>(_a[1]))); break;
        case 3: _t->slotExecPref(); break;
        case 4: { QMenu* _r = _t->contextMenu();
            if (_a[0]) *reinterpret_cast<QMenu**>(_a[0]) = std::move(_r); }  break;
        case 5: _t->slotExecImSwitcher(); break;
        case 6: _t->slotExecDict(); break;
        case 7: _t->slotExecInputPad(); break;
        case 8: _t->slotExecHandwritingInputPad(); break;
        case 9: _t->slotExecHelp(); break;
        case 10: _t->slotIndicatorResized(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (QUimHelperToolbar::*)()>(_a, &QUimHelperToolbar::quitToolbar, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (QUimHelperToolbar::*)()>(_a, &QUimHelperToolbar::toolbarResized, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (QUimHelperToolbar::*)(QMenu * )>(_a, &QUimHelperToolbar::menuRequested, 2))
            return;
    }
}

const QMetaObject *QUimHelperToolbar::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QUimHelperToolbar::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17QUimHelperToolbarE_t>.strings))
        return static_cast<void*>(this);
    return QFrame::qt_metacast(_clname);
}

int QUimHelperToolbar::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QFrame::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 11)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 11;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 11)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 11;
    }
    return _id;
}

// SIGNAL 0
void QUimHelperToolbar::quitToolbar()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void QUimHelperToolbar::toolbarResized()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void QUimHelperToolbar::menuRequested(QMenu * _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}
QT_WARNING_POP
