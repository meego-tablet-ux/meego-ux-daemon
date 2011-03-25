/*
 * Copyright 2011 Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include "application.h"
#include "dialog.h"

#include <QDesktopWidget>
#include <QDebug>
#include <QDBusConnection>
#include <QDBusError>
#include <QDBusInterface>
#include <QDeclarativeContext>
#include <QDeclarativeEngine>
#include <QGLFormat>
#include <QGLWidget>
#include <QSettings>
#include <MGConfItem>
#include <QX11Info>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <unistd.h>
#include <context_provider.h>

Dialog::Dialog(bool translucent, bool forceOnTop, bool opengl, QWidget * parent) :
    QDeclarativeView(parent),
    m_forceOnTop(forceOnTop)
{
    Application *app = static_cast<Application *>(qApp);

    connect(this, SIGNAL(requestTaskSwitcher()), qApp, SLOT(showTaskSwitcher()));
    connect(this, SIGNAL(requestHome()), qApp, SLOT(goHome()));

    int screenWidth = qApp->desktop()->rect().width();
    int screenHeight = qApp->desktop()->rect().height();
    setSceneRect(0, 0, screenWidth, screenHeight);
    rootContext()->setContextProperty("screenWidth", screenWidth);
    rootContext()->setContextProperty("screenHeight", screenHeight);
    rootContext()->setContextProperty("qApp", app);
    rootContext()->setContextProperty("mainWindow", this);
    rootContext()->setContextProperty("theme_name", MGConfItem("/meego/ux/theme").value().toString());
    foreach (QString key, app->themeConfig->allKeys())
    {
        if (key.contains("Size") || key.contains("Padding") ||
            key.contains("Width") ||key.contains("Height") ||
            key.contains("Margin") || key.contains("Thickness"))
        {
            rootContext()->setContextProperty("theme_" + key, app->themeConfig->value(key).toInt());
        }
        else if (key.contains("Opacity"))
        {
            rootContext()->setContextProperty("theme_" + key, app->themeConfig->value(key).toDouble());
        }
        else
        {
            rootContext()->setContextProperty("theme_" + key, app->themeConfig->value(key));
        }
    }

    if (opengl)
    {
        QGLFormat format = QGLFormat::defaultFormat();
        format.setSampleBuffers(false);
        format.setSamples(0);
        format.setAlpha(translucent);
        setViewport(new QGLWidget(format));
    }

    if (translucent)
        viewport()->setAttribute(Qt::WA_TranslucentBackground);

    setWindowFlags(Qt::FramelessWindowHint);
    if (translucent)
        setAttribute(Qt::WA_TranslucentBackground);
}

Dialog::~Dialog()
{
}

void Dialog::triggerSystemUIMenu()
{
    Application *app = static_cast<Application *>(qApp);
    app->openStatusIndicatorMenu();
}

void Dialog::goHome()
{
    emit requestHome();
}

void Dialog::showTaskSwitcher()
{
    requestTaskSwitcher();
}

bool Dialog::event (QEvent * event)
{
    if (event->type() == QEvent::Show && m_forceOnTop)
    {
        Atom stackingAtom = XInternAtom(QX11Info::display(), "_MEEGO_STACKING_LAYER", False);
        long layer = 2;
        XChangeProperty(QX11Info::display(), internalWinId(), stackingAtom, XA_CARDINAL, 32, PropModeReplace, (unsigned char*)&layer, 1);

        excludeFromTaskBar();
    }
    return QDeclarativeView::event(event);
}

void Dialog::setSkipAnimation()
{
    Atom miniAtom = XInternAtom(QX11Info::display(), "_MEEGOTOUCH_SKIP_ANIMATIONS", False);
    long min = 1;
    XChangeProperty(QX11Info::display(), internalWinId(), miniAtom, XA_CARDINAL, 32, PropModeReplace, (unsigned char*)&min, 1);
}

void Dialog::excludeFromTaskBar()
{
    // Tell the window to not to be shown in the switcher
    Atom skipTaskbarAtom = XInternAtom(QX11Info::display(), "_NET_WM_STATE_SKIP_TASKBAR", False);
    changeNetWmState(true, skipTaskbarAtom);

    // Also set the _NET_WM_STATE window property to ensure Home doesn't try to
    // manage this window in case the window manager fails to set the property in time
    Atom netWmStateAtom = XInternAtom(QX11Info::display(), "_NET_WM_STATE", False);
    QVector<Atom> atoms;
    atoms.append(skipTaskbarAtom);
    XChangeProperty(QX11Info::display(), internalWinId(), netWmStateAtom, XA_ATOM, 32, PropModeReplace, (unsigned char *)atoms.data(), atoms.count());
}

void Dialog::changeNetWmState(bool set, Atom one, Atom two)
{
    XEvent e;
    e.xclient.type = ClientMessage;
    Display *display = QX11Info::display();
    Atom netWmStateAtom = XInternAtom(display, "_NET_WM_STATE", FALSE);
    e.xclient.message_type = netWmStateAtom;
    e.xclient.display = display;
    e.xclient.window = internalWinId();
    e.xclient.format = 32;
    e.xclient.data.l[0] = set ? 1 : 0;
    e.xclient.data.l[1] = one;
    e.xclient.data.l[2] = two;
    e.xclient.data.l[3] = 0;
    e.xclient.data.l[4] = 0;
    XSendEvent(display, RootWindow(display, x11Info().screen()), FALSE, (SubstructureNotifyMask | SubstructureRedirectMask), &e);
    XSync(display, FALSE);
}

void Dialog::setActualOrientation(int orientation)
{
    m_actualOrientation = orientation;

    Atom orientationAtom = XInternAtom(QX11Info::display(), "_MEEGO_ORIENTATION", false);

    XChangeProperty(QX11Info::display(), internalWinId(), orientationAtom,
                    XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char*)&m_actualOrientation, 1);
}
