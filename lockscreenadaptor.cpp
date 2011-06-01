/*
 * This file was generated by qdbusxml2cpp version 0.7
 * Command line was: qdbusxml2cpp -c LockscreenAdaptor -a lockscreenadaptor.h:lockscreenadaptor.cpp interfaces/lockcontrol.xml
 *
 * qdbusxml2cpp is Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
 *
 * This is an auto-generated file.
 * Do not edit! All changes made to it will be lost.
 */

#include "lockscreenadaptor.h"
#include <QtCore/QMetaObject>
#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

/*
 * Implementation of adaptor class LockscreenAdaptor
 */

LockscreenAdaptor::LockscreenAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    // constructor
    setAutoRelaySignals(true);
}

LockscreenAdaptor::~LockscreenAdaptor()
{
    // destructor
}

void LockscreenAdaptor::lockscreen()
{
    // handle method call com.lockstatus.query.lockscreen
    QMetaObject::invokeMethod(parent(), "activateLockScreen");
}

bool LockscreenAdaptor::lockscreen_status()
{
    // handle method call com.lockstatus.query.lockscreen_status
    bool status;
    QMetaObject::invokeMethod(parent(), "screenOn", Q_RETURN_ARG(bool, status));
    return !status;
}

void LockscreenAdaptor::home()
{
    emit home_activated();
}
