/*
 * Copyright 2011 Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include "application.h"

#include <QOrientationSensor>
#include <QOrientationFilter>
#include <QOrientationReading>
#include <QPluginLoader>
#include <context_provider.h>

QTM_USE_NAMESPACE

class OrientationSensorFilter : public QOrientationFilter
{
public:
    bool filter(QOrientationReading *reading)
    {
        Application *app = (Application *)qApp;

        switch (reading->orientation())
        {
        case QOrientationReading::TopUp:
            app->setOrientation(1);
            break;
        case QOrientationReading::TopDown:
            app->setOrientation(3);
            break;
        case QOrientationReading::LeftUp:
            app->setOrientation(2);
            break;
        case QOrientationReading::RightUp:
            app->setOrientation(0);
            break;
        default:
            break;
        }

        return false;
    }
};

int main(int argc, char *argv[])
{
    bool opengl = false;
    for (int i=1; i<argc; i++)
    {
        QString s(argv[i]);
        if (s == "--opengl")
        {
            opengl = true;
        }
    }

    Application app(argc, argv, opengl);

    QOrientationSensor sensor;
    OrientationSensorFilter filter;
    sensor.addFilter(&filter);
    sensor.start();

    QObject::connect(&app, SIGNAL(startOrientationSensor()), &sensor, SLOT(start()));
    QObject::connect(&app, SIGNAL(stopOrientationSensor()), &sensor, SLOT(stop()));

    foreach (QString path, QCoreApplication::libraryPaths())
    {
        QPluginLoader loader(path + "/libmultipointtouchplugin.so");
        loader.load();
        if (loader.isLoaded())
        {
            loader.instance();
            break;
        }
    }

    context_provider_init (DBUS_BUS_SESSION, "com.meego.meego-ux-daemon");
    context_provider_install_key("Notifications.Last", true, NULL, NULL);
    context_provider_install_key("Notifications.Unread", false, NULL, NULL);

    // As long as we do not support persisting notifications across boot
    // then this is a safe assumption
    context_provider_set_boolean("Notifications.Unread", false);

    return app.exec();
}
