/*
 * Copyright 2011 Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include "application.h"

#include <QPluginLoader>
#include <context_provider.h>

int main(int argc, char *argv[])
{
    bool acceleratedPanels = false;
    for(int i = 1; i < argc; i++)
    {
        QString s(argv[i]);
        if(s == "--enable-accelerated-panels")
        {
            acceleratedPanels = true;
        }
    }

    // Fix for BMC #17521
    XInitThreads();

    // we never, ever want to be saddled with 'native' graphicssystem, as it is
    // amazingly slow. set us to 'raster'. this won't impact GL mode, as we are
    // explicitly using a QGLWidget viewport in those cases.
    QApplication::setGraphicsSystem("raster");

    Application app(argc, argv, acceleratedPanels);

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
    context_provider_install_key("Session.State",false, NULL ,NULL);

    // As long as we do not support persisting notifications across boot
    // then this is a safe assumption
    context_provider_set_boolean("Notifications.Unread", false);

    return app.exec();
}
