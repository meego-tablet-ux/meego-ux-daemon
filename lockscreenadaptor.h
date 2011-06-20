/*
 * This file was generated by qdbusxml2cpp version 0.7
 * Command line was: qdbusxml2cpp -c LockscreenAdaptor -a lockscreenadaptor.h:lockscreenadaptor.cpp interfaces/lockcontrol.xml
 *
 * qdbusxml2cpp is Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
 *
 * This is an auto-generated file.
 * This file may have been hand-edited. Look for HAND-EDIT comments
 * before re-generating it.
 */

#ifndef LOCKSCREENADAPTOR_H_1306256858
#define LOCKSCREENADAPTOR_H_1306256858

#include <QObject>
#include <QDBusAbstractAdaptor>
class QByteArray;
template<class T> class QList;
template<class Key, class Value> class QMap;
class QString;
class QStringList;
class QVariant;

/*
 * Adaptor class for interface com.lockstatus.query
 */
class LockscreenAdaptor: public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.lockstatus.query")
    Q_CLASSINFO("D-Bus Introspection", ""
"  <interface name=\"com.lockstatus.query\">\n"
"    <method name=\"lockscreen_status\">\n"
"      <arg direction=\"out\" type=\"b\" name=\"status\"/>\n"
"    </method>\n"
"    <method name=\"lockscreen\"/>\n"
"    <signal name=\"home_activated\"/>\n"
"    <signal name=\"screenOn\"/>\n"
"  </interface>\n"
        "")
public:
    LockscreenAdaptor(QObject *parent);
    virtual ~LockscreenAdaptor();

public: // PROPERTIES
public Q_SLOTS: // METHODS
    void lockscreen();
    bool lockscreen_status();
    void home();
    void sendScreenOn(bool status);

Q_SIGNALS: // SIGNALS
    void home_activated();
    void screenOn(bool status);
};

#endif
