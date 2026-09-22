/****************************************************************************
** Meta object code from reading C++ file 'plugin.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qt4/immodule/plugin.h"
#include <QtCore/qmetatype.h>
#include <QtCore/qplugin.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'plugin.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN21UimInputContextPluginE_t {};
} // unnamed namespace

template <> constexpr inline auto UimInputContextPlugin::qt_create_metaobjectdata<qt_meta_tag_ZN21UimInputContextPluginE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "UimInputContextPlugin"
    };

    QtMocHelpers::UintData qt_methods {
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<UimInputContextPlugin, qt_meta_tag_ZN21UimInputContextPluginE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject UimInputContextPlugin::staticMetaObject = { {
    QMetaObject::SuperData::link<QPlatformInputContextPlugin::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN21UimInputContextPluginE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN21UimInputContextPluginE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN21UimInputContextPluginE_t>.metaTypes,
    nullptr
} };

void UimInputContextPlugin::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<UimInputContextPlugin *>(_o);
    (void)_t;
    (void)_c;
    (void)_id;
    (void)_a;
}

const QMetaObject *UimInputContextPlugin::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *UimInputContextPlugin::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN21UimInputContextPluginE_t>.strings))
        return static_cast<void*>(this);
    return QPlatformInputContextPlugin::qt_metacast(_clname);
}

int UimInputContextPlugin::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QPlatformInputContextPlugin::qt_metacall(_c, _id, _a);
    return _id;
}

#ifdef QT_MOC_EXPORT_PLUGIN_V2
static constexpr unsigned char qt_pluginMetaDataV2_UimInputContextPlugin[] = {
    0xbf, 
    // "IID"
    0x02,  0x78,  0x3b,  'o',  'r',  'g',  '.',  'q', 
    't',  '-',  'p',  'r',  'o',  'j',  'e',  'c', 
    't',  '.',  'Q',  't',  '.',  'Q',  'P',  'l', 
    'a',  't',  'f',  'o',  'r',  'm',  'I',  'n', 
    'p',  'u',  't',  'C',  'o',  'n',  't',  'e', 
    'x',  't',  'F',  'a',  'c',  't',  'o',  'r', 
    'y',  'I',  'n',  't',  'e',  'r',  'f',  'a', 
    'c',  'e',  '.',  '5',  '.',  '1', 
    // "className"
    0x03,  0x75,  'U',  'i',  'm',  'I',  'n',  'p', 
    'u',  't',  'C',  'o',  'n',  't',  'e',  'x', 
    't',  'P',  'l',  'u',  'g',  'i',  'n', 
    // "MetaData"
    0x04,  0xa1,  0x64,  'K',  'e',  'y',  's',  0x81, 
    0x63,  'u',  'i',  'm', 
    0xff, 
};
QT_MOC_EXPORT_PLUGIN_V2(UimInputContextPlugin, UimInputContextPlugin, qt_pluginMetaDataV2_UimInputContextPlugin)
#else
QT_PLUGIN_METADATA_SECTION
Q_CONSTINIT static constexpr unsigned char qt_pluginMetaData_UimInputContextPlugin[] = {
    'Q', 'T', 'M', 'E', 'T', 'A', 'D', 'A', 'T', 'A', ' ', '!',
    // metadata version, Qt version, architectural requirements
    0, QT_VERSION_MAJOR, QT_VERSION_MINOR, qPluginArchRequirements(),
    0xbf, 
    // "IID"
    0x02,  0x78,  0x3b,  'o',  'r',  'g',  '.',  'q', 
    't',  '-',  'p',  'r',  'o',  'j',  'e',  'c', 
    't',  '.',  'Q',  't',  '.',  'Q',  'P',  'l', 
    'a',  't',  'f',  'o',  'r',  'm',  'I',  'n', 
    'p',  'u',  't',  'C',  'o',  'n',  't',  'e', 
    'x',  't',  'F',  'a',  'c',  't',  'o',  'r', 
    'y',  'I',  'n',  't',  'e',  'r',  'f',  'a', 
    'c',  'e',  '.',  '5',  '.',  '1', 
    // "className"
    0x03,  0x75,  'U',  'i',  'm',  'I',  'n',  'p', 
    'u',  't',  'C',  'o',  'n',  't',  'e',  'x', 
    't',  'P',  'l',  'u',  'g',  'i',  'n', 
    // "MetaData"
    0x04,  0xa1,  0x64,  'K',  'e',  'y',  's',  0x81, 
    0x63,  'u',  'i',  'm', 
    0xff, 
};
QT_MOC_EXPORT_PLUGIN(UimInputContextPlugin, UimInputContextPlugin)
#endif  // QT_MOC_EXPORT_PLUGIN_V2

QT_WARNING_POP
