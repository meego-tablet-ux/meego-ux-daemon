/*
 * This file was generated by qdbusxml2cpp version 0.7
 * Command line was: qdbusxml2cpp -c StatusIndicatorMenuAdaptor interfaces/statusindicatormenu.xml -a statusindicatormenuadaptor
 *
 * qdbusxml2cpp is Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 *
 * This is an auto-generated file.
 * Do not edit! All changes made to it will be lost.
 */

#include "statusindicatormenuadaptor.h"
#include <QtCore/QMetaObject>
#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

/*
 * Implementation of adaptor class StatusIndicatorMenuAdaptor
 */

StatusIndicatorMenuAdaptor::StatusIndicatorMenuAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    // constructor
    setAutoRelaySignals(true);
}

StatusIndicatorMenuAdaptor::~StatusIndicatorMenuAdaptor()
{
    // destructor
}

void StatusIndicatorMenuAdaptor::open()
{
    // handle method call com.meego.core.MStatusIndicatorMenu.open
    QMetaObject::invokeMethod(parent(), "openStatusIndicatorMenu");
}

