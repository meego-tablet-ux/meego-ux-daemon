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
#include <QKeyEvent>
#include <QSettings>
#include <QTimer>
#include <MGConfItem>
#include <QX11Info>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <unistd.h>
#include <context_provider.h>

#include "atoms.h"

#define DEBUG_INFO_PATH "/var/tmp/debug.info"

Dialog::Dialog(bool translucent, bool skipAnimation, bool forceOnTop, QWidget * parent) :
    QDeclarativeView(parent),
    m_forceOnTop(forceOnTop),
    m_skipAnimation(skipAnimation),
    m_translucent(translucent),
    m_usingGl(false)
{
    Application *app = static_cast<Application *>(qApp);

    setEnableDebugInfo(true);
    connect(&m_debugInfoFileWatcher, SIGNAL(fileChanged(QString)), SLOT(debugFileChanged(QString)));
    connect(&m_debugInfoFileWatcher, SIGNAL(directoryChanged(QString)), SLOT(debugDirChanged(QString)));

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

    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    setWindowFlags(Qt::FramelessWindowHint);
    if (m_translucent)
        setAttribute(Qt::WA_TranslucentBackground);

    setGLRendering();
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
    if (event->type() == QEvent::Show)
    {
        if (m_forceOnTop)
        {
            Atom stackingAtom = getAtom(ATOM_MEEGO_STACKING_LAYER);
            long layer = 2;
            XChangeProperty(QX11Info::display(), internalWinId(), stackingAtom, XA_CARDINAL, 32, PropModeReplace, (unsigned char*)&layer, 1);

            excludeFromTaskBar();
        }
        if (m_skipAnimation)
        {
            Atom miniAtom = getAtom(ATOM_MEEGOTOUCH_SKIP_ANIMATIONS);
            long min = 1;
            XChangeProperty(QX11Info::display(), internalWinId(), miniAtom, XA_CARDINAL, 32, PropModeReplace, (unsigned char*)&min, 1);
        }
    }
    return QDeclarativeView::event(event);
}

void Dialog::excludeFromTaskBar()
{
    // Tell the window to not to be shown in the switcher
    Atom skipTaskbarAtom = getAtom(ATOM_NET_WM_STATE_SKIP_TASKBAR);
    changeNetWmState(true, skipTaskbarAtom);

    // Also set the _NET_WM_STATE window property to ensure Home doesn't try to
    // manage this window in case the window manager fails to set the property in time
    Atom netWmStateAtom = getAtom(ATOM_NET_WM_STATE);
    QVector<Atom> atoms;
    atoms.append(skipTaskbarAtom);
    XChangeProperty(QX11Info::display(), internalWinId(), netWmStateAtom, XA_ATOM, 32, PropModeReplace, (unsigned char *)atoms.data(), atoms.count());
}

void Dialog::changeNetWmState(bool set, Atom one, Atom two)
{
    XEvent e;
    e.xclient.type = ClientMessage;
    Display *display = QX11Info::display();
    Atom netWmStateAtom = getAtom(ATOM_NET_WM_STATE);
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

    Atom orientationAtom = getAtom(ATOM_MEEGO_ORIENTATION);

    XChangeProperty(QX11Info::display(), internalWinId(), orientationAtom,
                    XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char*)&m_actualOrientation, 1);
}

void Dialog::closeEvent(QCloseEvent *)
{
    emit requestClose();
}

void Dialog::keyPressEvent ( QKeyEvent * event )
{
    if ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_D)
    {
        setEnableDebugInfo(!m_debugInfoEnabled);
        return;
    }

    QDeclarativeView::keyPressEvent(event);
}

void Dialog::setGLRendering()
{
    QGLFormat format = QGLFormat::defaultFormat();
    format.setSampleBuffers(false);
    if (m_translucent)
    {
        format.setAlpha(true);
    }
    setViewport(new QGLWidget(format));

    // each time we create a new viewport widget, we must redo our optimisations
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    viewport()->setAttribute(Qt::WA_OpaquePaintEvent);
    viewport()->setAttribute(Qt::WA_NoSystemBackground);
    if (m_translucent)
    {
        viewport()->setAttribute(Qt::WA_TranslucentBackground);
    }
}

void Dialog::switchToSoftwareRendering()
{
    if (!m_usingGl)
        return;
    m_usingGl = false;

    setViewport(0);

    // each time we create a new viewport widget, we must redo our optimisations
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    viewport()->setAttribute(Qt::WA_OpaquePaintEvent);
    viewport()->setAttribute(Qt::WA_NoSystemBackground);
    if (m_translucent)
    {
        viewport()->setAttribute(Qt::WA_TranslucentBackground);
    }
}

void Dialog::switchToGLRendering()
{
    if (m_usingGl)
        return;

    m_usingGl = true;

    //go once around event loop to avoid crash in egl
    QTimer::singleShot(0, this, SLOT(setGLRendering()));
}

void Dialog::setSystemDialog()
{
    Atom atom = getAtom(ATOM_MEEGO_SYSTEM_DIALOG);
    long value = 1;
    XChangeProperty(QX11Info::display(), internalWinId(), atom, XA_CARDINAL, 32, PropModeReplace, (unsigned char*)&value, 1);
}

void Dialog::debugDirChanged(const QString)
{
    if (QFile::exists(DEBUG_INFO_PATH))
    {
        m_debugInfoFileWatcher.removePath("/var/tmp");
        m_debugInfoFileWatcher.addPath(DEBUG_INFO_PATH);
        debugFileChanged(DEBUG_INFO_PATH);
    }
}

void Dialog::debugFileChanged(const QString)
{
    if (QFile::exists(DEBUG_INFO_PATH))
    {
        QFile data(DEBUG_INFO_PATH);
        if (data.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            m_debugInfo = data.readAll();
        }
        emit debugInfoChanged();
    }
    else
    {
        m_debugInfoFileWatcher.removePath(DEBUG_INFO_PATH);
        m_debugInfoFileWatcher.addPath("/var/tmp");
        m_debugInfo.clear();
        emit debugInfoChanged();
    }
}

void Dialog::setEnableDebugInfo(bool enable)
{
    m_debugInfoEnabled = enable;
    if (m_debugInfoEnabled)
    {
        if (QFile::exists("/var/tmp/debug.info"))
        {
            m_debugInfoFileWatcher.addPath(DEBUG_INFO_PATH);
            debugFileChanged(DEBUG_INFO_PATH);
        }
        else
        {
            m_debugInfoFileWatcher.addPath("/var/tmp");
        }
    }
    else
    {
        m_debugInfoFileWatcher.removePath("/var/tmp");
        m_debugInfoFileWatcher.removePath(DEBUG_INFO_PATH);
        m_debugInfo.clear();
        emit debugInfoChanged();
    }
}
