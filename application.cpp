/*
 * Copyright 2011 Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 
 * http://www.apache.org/licenses/LICENSE-2.0
 */
#include <QDBusMetaType>
#include <QDBusArgument>
#include <QDBusInterface>
#include <QDBusConnection>
#include <QDeclarativeEngine>
#include <QDeclarativeContext>
#include <QLibraryInfo>
#include <QSettings>
#include <QTimer>
#include <QInputContextFactory>
#include <MGConfItem>
#include <context_provider.h>

#include "application.h"
#include "notificationsmanageradaptor.h"
#include "statusindicatormenuadaptor.h"
#include "dialog.h"
#include "desktop.h"
#include "notificationdatastore.h"
#include "notificationmodel.h"

#include <QX11Info>
#include <X11/Xatom.h>
#include <X11/keysymdef.h>
#include <X11/extensions/scrnsaver.h>

#define CONTEXT_NOTIFICATIONS_LAST "Notifications.Last"
#define CONTEXT_NOTIFICATIONS_UNREAD "Notifications.Unread"

Q_DECLARE_METATYPE(QList<MNotification>);
Q_DECLARE_METATYPE(QList<MNotificationGroup>);

void messageHandler(QtMsgType type, const char *msg)
{
    switch (type) {
    case QtDebugMsg:
        fprintf(stderr, "Debug: %s\n", msg);
        break;
    case QtWarningMsg:
        fprintf(stderr, "Warning: %s\n", msg);
        break;
    case QtCriticalMsg:
        fprintf(stderr, "Critical: %s\n", msg);
        break;
    case QtFatalMsg:
        fprintf(stderr, "Fatal: %s\n", msg);
        abort();
    }
}

static int ignoreXError (Display *, XErrorEvent *)
{
    return 0;
}

static int grabKey(const char* key)
{
    Display *dpy = QX11Info::display();
    KeyCode code = XKeysymToKeycode(dpy, XStringToKeysym(key));
    if(!code) {
        qWarning() << "Cannot find keysym/keycode for " << key << ", not grabbing.";
        return 0;
    }
    XGrabKey(dpy, code, 0, QX11Info::appRootWindow(QX11Info::appScreen()),
             true, GrabModeAsync, GrabModeAsync);
    return code;
}

void Application::grabHomeKey(const char* key)
{
    homeKeys.insert(grabKey(key));
}

Application::Application(int & argc, char ** argv, bool opengl) :
    QApplication(argc, argv),
    orientation(1),
    useOpenGL(opengl),
    taskSwitcher(NULL),
    lockScreen(NULL),
    panelsScreen(NULL),
    statusIndicatorMenu(NULL),
    m_runningAppsLimit(16),
    m_homeActive(false),
    m_homePressTime(0),
    m_haveAppStore(QFile::exists("/usr/share/applications/com.intel.appup-tablet.desktop")),
    m_foregroundOrientation(2),
    m_notificationDataStore(NotificationDataStore::instance()),
    m_notificationModel(new NotificationModel),
    m_lastNotificationId(0)
{
    setApplicationName("meego-ux-daemon");

    setQuitOnLastWindowClosed(false);

    int (*oldXErrorHandler)(Display*, XErrorEvent*);

    Display *dpy = QX11Info::display();
    int screen = QX11Info::appScreen();
    XID root = QX11Info::appRootWindow(screen);
    windowTypeAtom = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", false);
    windowTypeNormalAtom = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_NORMAL", false);
    windowTypeDesktopAtom = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DESKTOP", false);
    windowTypeNotificationAtom = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_NOTIFICATION", false);
    windowTypeDockAtom = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", false);
    clientListAtom = XInternAtom(dpy, "_NET_CLIENT_LIST", false);
    closeWindowAtom = XInternAtom(dpy, "_NET_CLOSE_WINDOW", false);
    skipTaskbarAtom = XInternAtom(dpy, "_NET_WM_STATE_SKIP_TASKBAR", false);
    windowStateAtom = XInternAtom(dpy, "_NET_WM_STATE", false);
    activeWindowAtom = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", false);
    foregroundOrientationAtom = XInternAtom(dpy, "_MEEGO_ORIENTATION", false);
    inhibitScreenSaverAtom = XInternAtom(dpy, "_MEEGO_INHIBIT_SCREENSAVER", false);

    m_screenSaverTimeoutItem = new MGConfItem("/meego/ux/ScreenSaverTimeout", this);
    if (!m_screenSaverTimeoutItem || m_screenSaverTimeoutItem->value() == QVariant::Invalid)
    {
        m_screenSaverTimeout = 180;
    }
    else
    {
        m_screenSaverTimeout = m_screenSaverTimeoutItem->value().toInt();
    }
    connect(m_screenSaverTimeoutItem, SIGNAL(valueChanged()), this, SLOT(screenSaverTimeoutChanged()));

    if (!XScreenSaverQueryExtension(dpy, &m_ss_event, &m_ss_error))
    {
        qFatal("meego-ux-daemon requires support for the XScreenSaver extension");
    }

    XID kill_xid;
    Atom kill_type;

    oldXErrorHandler = XSetErrorHandler (ignoreXError);
    if (XScreenSaverGetRegistered(dpy, screen, &kill_xid, &kill_type))
    {
        XKillClient (dpy, kill_xid);

        // There's only room for one screen saver in this town!
        XScreenSaverUnregister(dpy, screen);
    }

    XSetScreenSaver(dpy, m_screenSaverTimeout, 0, DefaultBlanking, DontAllowExposures);

    XSync(dpy, FALSE);
    XSetErrorHandler(oldXErrorHandler);

    XScreenSaverSelectInput(dpy, root, ScreenSaverNotifyMask);

    Pixmap blank_pix = XCreatePixmap (dpy, root, 1, 1, 1);
    XScreenSaverRegister(dpy, screen, blank_pix, XA_PIXMAP);

    MGConfItem *landscapeItem = new MGConfItem("/meego/ux/PreferredLandscapeOrientation", this);
    if (!landscapeItem || landscapeItem->value() == QVariant::Invalid)
    {
        m_preferredLandscapeOrientation = 1;
    }
    else
    {
        m_preferredLandscapeOrientation = landscapeItem->value().toInt();
    }

    MGConfItem *portraitItem = new MGConfItem("/meego/ux/PreferredPortraitOrientation", this);
    if (!portraitItem || portraitItem->value() == QVariant::Invalid)
    {
        m_preferredPortraitOrientation = 2;
    }
    else
    {
        m_preferredPortraitOrientation = portraitItem->value().toInt();
    }

    MGConfItem *showPanelsItem = new MGConfItem("/meego/ux/ShowPanelsAsHome", this);
    if (!showPanelsItem || showPanelsItem->value() == QVariant::Invalid)
    {
        m_showPanelsAsHome = false;
    }
    else
    {
        m_showPanelsAsHome = showPanelsItem->value().toBool();
    }

    MGConfItem *homeScreenDirectoryName = new MGConfItem("/meego/ux/HomeScreenDirectoryName", this);
    if (!homeScreenDirectoryName ||
        homeScreenDirectoryName->value() == QVariant::Invalid ||
        !QFile::exists("/usr/share/" + homeScreenDirectoryName->value().toString()))
    {
        m_homeScreenDirectoryName = QString("meego-ux-appgrid");
    }
    else
    {
        m_homeScreenDirectoryName = homeScreenDirectoryName->value().toString();
    }

    QString theme = MGConfItem("/meego/ux/theme").value().toString();
    QString themeFile = QString("/usr/share/themes/") + theme + "/theme.ini";
    if(!QFile::exists(themeFile))
    {
        // fallback
        themeFile = QString("/usr/share/themes/1024-600-10/theme.ini");
    }
    themeConfig = new QSettings(themeFile, QSettings::NativeFormat, this);

    setFont(QFont(themeConfig->value("fontFamily").toString(), themeConfig->value("fontPixelSizeMedium").toInt()));

    // As long as we do not support persisting notifications across boot
    // then this is a safe assumption
    context_provider_set_boolean(CONTEXT_NOTIFICATIONS_UNREAD, false);

    QInputContext *ic = QInputContextFactory::create("MInputContext", 0);
    if(ic)
        setInputContext(ic);

    qInstallMsgHandler(messageHandler);

    // Brutal hack: MTF (underneath the input context somewhere)
    // registers a duplicate session on a default name (sucked from
    // QApplication I guess) that collides.  We don't need it, so drop
    // it.
    QDBusConnection::sessionBus().unregisterService("com.nokia.meego-ux-daemon");

    loadTranslators();

    installTranslator(&qtTranslator);
    installTranslator(&commonTranslator);
    installTranslator(&homescreenTranslator);
    installTranslator(&panelsTranslator);
    installTranslator(&daemonTranslator);
    installTranslator(&mediaTranslator);

    qDBusRegisterMetaType<MNotification>();
    qDBusRegisterMetaType<QList<MNotification> >();
    qDBusRegisterMetaType<QList<MNotificationGroup> >();

    new NotificationManagerAdaptor(this);
    QDBusConnection::sessionBus().registerService("com.meego.core.MNotificationManager");
    QDBusConnection::sessionBus().registerObject("/notificationmanager", this);

    new StatusIndicatorMenuAdaptor(this);
    QDBusConnection::sessionBus().registerService("com.nokia.systemui");
    QDBusConnection::sessionBus().registerObject("/statusindicatormenu", this);

    grabHomeKey("Super_L");
    grabHomeKey("Super_R");
    grabHomeKey("XF86AudioMedia"); // WeTab's corner key

    menu = grabKey("Menu");

    if (m_showPanelsAsHome)
    {
        gridScreen = new Dialog(false, false, useOpenGL);
        gridScreen->setSource(QUrl::fromLocalFile("/usr/share/" + m_homeScreenDirectoryName + "/main.qml"));

        panelsScreen = new Dialog(false, false, useOpenGL);
        panelsScreen->setAttribute(Qt::WA_X11NetWmWindowTypeDesktop);
        panelsScreen->rootContext()->setContextProperty("notificationModel", m_notificationModel);
        panelsScreen->setSource(QUrl::fromLocalFile("/usr/share/meego-ux-panels/main.qml"));
        panelsScreen->show();
    }
    else
    {
        panelsScreen = new Dialog(false, false, useOpenGL);
        panelsScreen->rootContext()->setContextProperty("notificationModel", m_notificationModel);
        panelsScreen->setSource(QUrl::fromLocalFile("/usr/share/meego-ux-panels/main.qml"));

        gridScreen = new Dialog(false, false, useOpenGL);
        gridScreen->setAttribute(Qt::WA_X11NetWmWindowTypeDesktop);
        gridScreen->setSource(QUrl::fromLocalFile("/usr/share/" + m_homeScreenDirectoryName + "/main.qml"));
        gridScreen->show();
    }

    m_homeLongPressTimer = new QTimer(this);
    m_homeLongPressTimer->setInterval(500);
    connect(m_homeLongPressTimer, SIGNAL(timeout()), this, SLOT(toggleSwitcher()));

    // Lock the screen
    lock();

    connect(this, SIGNAL(windowListUpdated(QList<WindowInfo>)), this, SLOT(updateApps(QList<WindowInfo>)));

    context_provider_init (DBUS_BUS_SESSION, "com.meego.meego-ux-daemon");
    context_provider_install_key(CONTEXT_NOTIFICATIONS_LAST, true, NULL, NULL);
    context_provider_install_key(CONTEXT_NOTIFICATIONS_UNREAD, false, NULL, NULL);
}

Application::~Application()
{
}

void Application::setRunningAppsLimit(int limit)
{
    m_runningAppsLimit = limit;

    if (m_runningAppsLimit < m_runningApps.length() - 1)
    {
        while (m_runningApps.length() > m_runningAppsLimit)
        {
            Desktop *d = m_runningApps.takeLast();
            m_runningAppsOverflow << d;
        }
        emit runningAppsChanged();
    }
}

QDeclarativeListProperty<Desktop> Application::runningApps()
{
    return QDeclarativeListProperty<Desktop>(this, m_runningApps);
}

void Application::showTaskSwitcher()
{
    if (taskSwitcher)
    {
        taskSwitcher->activateWindow();
        taskSwitcher->raise();
        taskSwitcher->show();
        return;
    }

    taskSwitcher = new Dialog(true, false, useOpenGL);
    taskSwitcher->setSkipAnimation();
    connect(taskSwitcher->engine(), SIGNAL(quit()), this, SLOT(cleanupTaskSwitcher()));
    taskSwitcher->setAttribute(Qt::WA_X11NetWmWindowTypeDialog);
    taskSwitcher->setSource(QUrl::fromLocalFile("/usr/share/meego-ux-daemon/taskswitcher.qml"));
    taskSwitcher->show();
}

void Application::showPanels()
{
    if (m_showPanelsAsHome)
    {
        goHome();
    }
    else
    {
        panelsScreen->show();
        panelsScreen->activateWindow();
        panelsScreen->raise();
    }
}

void Application::showGrid()
{
    if (m_showPanelsAsHome)
    {
        gridScreen->show();
        gridScreen->activateWindow();
        gridScreen->raise();
    }
    else
    {
        goHome();
    }
}

void Application::showAppStore()
{
    launchDesktopByName("/usr/share/applications/com.intel.appup-tablet.desktop");
}

void Application::minimizeWindow(int windowId)
{
    XEvent e;
    memset(&e, 0, sizeof(e));

    e.xclient.type = ClientMessage;
    e.xclient.message_type = XInternAtom(QX11Info::display(), "WM_CHANGE_STATE",
                                         False);
    e.xclient.display = QX11Info::display();
    e.xclient.window = windowId;
    e.xclient.format = 32;
    e.xclient.data.l[0] = IconicState;
    e.xclient.data.l[1] = 0;
    e.xclient.data.l[2] = 0;
    e.xclient.data.l[3] = 0;
    e.xclient.data.l[4] = 0;
    XSendEvent(QX11Info::display(), QX11Info::appRootWindow(),
               False, (SubstructureNotifyMask | SubstructureRedirectMask), &e);

    XSync(QX11Info::display(), FALSE);
}

void Application::cleanupTaskSwitcher()
{
    taskSwitcher->hide();
}

void Application::cleanupLockscreen()
{
    lockScreen->deleteLater();
    lockScreen = NULL;
}

void Application::cleanupStatusIndicatorMenu()
{
    statusIndicatorMenu->hide();
}

void Application::goHome()
{
    if (taskSwitcher)
    {
        cleanupTaskSwitcher();
    }
    if (statusIndicatorMenu)
    {
        cleanupStatusIndicatorMenu();
    }
    // Minimize all windows
    foreach (Window win, openWindows)
    {
        minimizeWindow(win);
    }
}

void Application::lock()
{
    QDBusInterface iface("com.acer.AcerLockScreen",
                         "/com/acer/AcerLockScreen/request",
                         "com.acer.AcerLockScreen.request",
                         QDBusConnection::systemBus());
    if(iface.isValid())
    {
        iface.call("lockscreen_open", (unsigned)0, false, false);
    }
    else
    {
        if (lockScreen)
        {
            lockScreen->activateWindow();
            lockScreen->raise();
        }
        else
        {
            lockScreen = new Dialog(true, true, useOpenGL);
            connect(lockScreen->engine(), SIGNAL(quit()), this, SLOT(cleanupLockscreen()));
            lockScreen->setAttribute(Qt::WA_X11NetWmWindowTypeDialog);
            lockScreen->rootContext()->setContextProperty("notificationModel", m_notificationModel); 
            lockScreen->setSource(QUrl::fromLocalFile("/usr/share/meego-ux-daemon/lockscreen.qml"));
            lockScreen->show();
        }
    }
}

bool Application::x11EventFilter(XEvent *event)
{
    if (event->type == KeyRelease)
    {
        XKeyEvent * keyEvent = (XKeyEvent *)event;
        if (homeKeys.contains(keyEvent->keycode))
        {
            if (m_homeLongPressTimer->isActive())
            {
                m_homeLongPressTimer->stop();

                if (m_homeActive || (taskSwitcher && taskSwitcher->isVisible()))
                    toggleSwitcher();
                else
                    goHome();
            }
        }
    }
    else if (event->type == KeyPress)
    {
        XKeyEvent * keyEvent = (XKeyEvent *)event;
        if (homeKeys.contains(keyEvent->keycode))
        {
            m_homeLongPressTimer->start();
            m_homePressTime = keyEvent->time;
        }
        else if (keyEvent->keycode == menu)
        {
            toggleSwitcher();
        }
    }

    if (event->type == PropertyNotify &&
        event->xproperty.window == DefaultRootWindow(QX11Info::display()) &&
        event->xproperty.atom == clientListAtom)
    {
        m_homeActive = false;
        updateWindowList();
        return true;
    }
    else if (event->type == PropertyNotify &&
             event->xproperty.atom == foregroundOrientationAtom &&
             (int)event->xproperty.window == m_foregroundWindow)
    {
        setForegroundOrientationForWindow(event->xproperty.window);
    }
    else if (event->type == ClientMessage &&
             event->xclient.message_type == closeWindowAtom)
    {
        if (!windowsBeingClosed.contains(event->xclient.window))
        {
            windowsBeingClosed.append(event->xclient.window);
        }
        updateWindowList();
        return true;
    }
    else if (event->type == PropertyNotify &&
             (event->xproperty.atom == windowTypeAtom ||
              event->xproperty.atom == windowStateAtom))
    {
        updateWindowList();
        return true;
    }
    else if (event->type == PropertyNotify &&
             event->xproperty.atom == activeWindowAtom)
    {
        Display *dpy = QX11Info::display();
        Atom actualType;
        int actualFormat;
        unsigned long numWindowItems, bytesLeft;
        unsigned char *data = NULL;

        int result = XGetWindowProperty(dpy,
                                        DefaultRootWindow(dpy),
                                        activeWindowAtom,
                                        0, 0x7fffffff,
                                        false, XA_WINDOW,
                                        &actualType,
                                        &actualFormat,
                                        &numWindowItems,
                                        &bytesLeft,
                                        &data);

        if (result == Success && data != None)
        {
            Window w = *(Window *)data;
            if ((!taskSwitcher || !taskSwitcher->isVisible()) &&
                    (!statusIndicatorMenu || !statusIndicatorMenu->isVisible()))
            {
                m_foregroundWindow = (int)w;
                emit foregroundWindowChanged();

                XSelectInput(QX11Info::display(), w, PropertyChangeMask);
                setForegroundOrientationForWindow(w);

                updateScreenSaver(w);

                if (m_showPanelsAsHome)
                {
                    if (m_foregroundWindow == panelsScreen->winId())
                        m_homeActive = true;
                    else
                        m_homeActive = false;

                    if (!taskSwitcher && m_foregroundWindow != gridScreen->winId())
                        minimizeWindow(gridScreen->winId());
                }
                else
                {
                    if (m_foregroundWindow == gridScreen->winId())
                        m_homeActive = true;
                    else
                        m_homeActive = false;

                    if (!taskSwitcher && m_foregroundWindow != panelsScreen->winId())
                        minimizeWindow(panelsScreen->winId());
                }
            }
            XFree(data);
        }
        else
        {
            qDebug() << "XXX active window ???";
        }
    }
    else if (event->type == PropertyNotify &&
             event->xproperty.atom == inhibitScreenSaverAtom)
    {
        Display *dpy = QX11Info::display();
        Atom actualType;
        int actualFormat;
        unsigned long numWindowItems, bytesLeft;
        unsigned char *data = NULL;

        int result = XGetWindowProperty(dpy,
                                        DefaultRootWindow(dpy),
                                        inhibitScreenSaverAtom,
                                        0, 0x7fffffff,
                                        false, XA_WINDOW,
                                        &actualType,
                                        &actualFormat,
                                        &numWindowItems,
                                        &bytesLeft,
                                        &data);

        if (result == Success && data != None)
        {
            Window w = event->xproperty.window;
            bool inhibit = *(bool *)data;
            if (inhibit)
            {
                inhibitList << w;
            }
            else
            {
                inhibitList.removeAll(w);
            }

            if ((int)w == m_foregroundWindow)
            {
                updateScreenSaver(w);
            }

            XFree(data);
        }
    }

    if (event->type == m_ss_event)
    {
        XScreenSaverNotifyEvent *sevent = (XScreenSaverNotifyEvent *) event;
        if (sevent->state == ScreenSaverOn)
        {
            lock();
            return true;
        }
    }
    return QApplication::x11EventFilter(event);
}

int Application::getForegroundOrientation()
{
    return m_foregroundOrientation;
}

void Application::updateWindowList()
{
    Display *dpy = QX11Info::display();
    XWindowAttributes wAttributes;
    Atom actualType;
    int actualFormat;
    unsigned long numWindowItems, bytesLeft;
    unsigned char *windowData = NULL;

    int result = XGetWindowProperty(dpy,
                                    DefaultRootWindow(dpy),
                                    clientListAtom,
                                    0, 0x7fffffff,
                                    false, XA_WINDOW,
                                    &actualType,
                                    &actualFormat,
                                    &numWindowItems,
                                    &bytesLeft,
                                    &windowData);

    if (result == Success && windowData != None)
    {
        QList<Window> windowsStillBeingClosed;
        QList<WindowInfo> windowList;
        Window *wins = (Window *)windowData;

        openWindows.clear();

        for (unsigned int i = 0; i < numWindowItems; i++)
        {
            if (XGetWindowAttributes(dpy, wins[i], &wAttributes) != 0
                && wAttributes.width > 0 &&
                wAttributes.height > 0 && wAttributes.c_class == InputOutput &&
                wAttributes.map_state != IsUnmapped)
            {
                unsigned char *typeData = NULL;
                unsigned long numTypeItems;
                unsigned char *meegoIconName = NULL;
                unsigned char *notifyIconName = NULL;
                unsigned long pid = 0;

                Atom iconAtom = XInternAtom(dpy, "_NET_WM_ICON_NAME", False);
                Atom stringAtom = XInternAtom(dpy, "UTF8_STRING", False);
                result = XGetWindowProperty(dpy,
                                            wins[i],
                                            iconAtom,
                                            0L, (long)BUFSIZ, false,
                                            stringAtom,
                                            &actualType,
                                            &actualFormat,
                                            &numTypeItems,
                                            &bytesLeft,
                                            &meegoIconName);
                if (result == Success)
                {
                    if (meegoIconName &&
                        !((actualType == stringAtom) &&  (actualFormat == 8)))
                    {
                        XFree ((char *)meegoIconName);
                        meegoIconName = NULL;
                    }
                }

                Atom notifyAtom = XInternAtom(dpy, "_MEEGO_TABLET_NOTIFY", False);
                result = XGetWindowProperty(dpy,
                                            wins[i],
                                            notifyAtom,
                                            0L, (long)BUFSIZ, false,
                                            XA_STRING,
                                            &actualType,
                                            &actualFormat,
                                            &numTypeItems,
                                            &bytesLeft,
                                            &notifyIconName);
                if (result == Success)
                {
                    if (notifyIconName &&
                        !((actualType == XA_STRING) &&  (actualFormat == 8)))
                    {
                        XFree ((char *)notifyIconName);
                        notifyIconName = NULL;
                    }
                }

                // _NET_WM_PID
                Atom pidAtom = XInternAtom(dpy, "_NET_WM_PID", True);
                unsigned char *propPid = 0;
                result = XGetWindowProperty(dpy,
                                            wins[i],
                                            pidAtom,
                                            0L, (long)BUFSIZ, false,
                                            XA_CARDINAL,
                                            &actualType,
                                            &actualFormat,
                                            &numTypeItems,
                                            &bytesLeft,
                                            &propPid);
                if (result == Success && propPid != 0)
                {
                    pid = *((unsigned long *)propPid);
                    XFree(propPid);
                }

                result = XGetWindowProperty(dpy,
                                            wins[i],
                                            windowTypeAtom,
                                            0L, 16L, false,
                                            XA_ATOM,
                                            &actualType,
                                            &actualFormat,
                                            &numTypeItems,
                                            &bytesLeft,
                                            &typeData);
                if (result == Success)
                {
                    Atom *type = (Atom *)typeData;

                    bool includeInWindowList = false;

                    // plain Xlib windows do not have a type
                    if (numTypeItems == 0)
                        includeInWindowList = true;
                    for (unsigned int n = 0; n < numTypeItems; n++)
                    {
                        if (type[n] == windowTypeDesktopAtom ||
                            type[n] == windowTypeNotificationAtom ||
                            type[n] == windowTypeDockAtom)
                        {
                            includeInWindowList = false;
                            break;
                        }
                        if (type[n] == windowTypeNormalAtom)
                        {
                            includeInWindowList = true;
                        }
                    }

                    if (includeInWindowList)
                    {
                        if (getNetWmState(dpy, wins[i]).contains(skipTaskbarAtom))
                        {
                            includeInWindowList = false;
                        }
                    }

                    if (includeInWindowList)
                    {
                        Pixmap pixmap = 0;
                        XTextProperty textProperty;
                        QString title;

                        if (XGetWMName(dpy, wins[i], &textProperty) != 0)
                        {
                            title = QString((const char *)(textProperty.value));
                            XFree(textProperty.value);
                        }

                        // Get window icon
                        XWMHints *wmhints = XGetWMHints(dpy, wins[i]);
                        if (wmhints != NULL)
                        {
                            pixmap = wmhints->icon_pixmap;
                            XFree(wmhints);
                        }

                        if (!windowsBeingClosed.contains(wins[i]))
                        {
                            QString name((char *)meegoIconName);
                            QString notify((char *)notifyIconName);

                            Atom iconGeometryAtom = XInternAtom(QX11Info::display(), "_NET_WM_ICON_GEOMETRY", False);

                            unsigned int geom[4];
                            geom[0] = 0; // x
                            geom[1] = 0; // y
                            geom[2] = 100; // width
                            geom[3] = 100; // height
                            XChangeProperty(QX11Info::display(),
                                            wins[i],
                                            iconGeometryAtom,
                                            XA_CARDINAL,
                                            sizeof(unsigned int) * 8,
                                            PropModeReplace,
                                            (unsigned char *)&geom, 4);

                            openWindows.append(wins[i]);
                            windowList.append(WindowInfo(title,
                                                         wins[i],
                                                         wAttributes,
                                                         pixmap,
                                                         name,
                                                         notify,
                                                         pid,
                                                         orientation));
                        }
                        else
                        {
                            windowsStillBeingClosed.append(wins[i]);
                        }
                    }
                    XFree(typeData);
                }
            }
        }

        XFree(wins);

        for (int i = windowsBeingClosed.count() - 1; i >= 0; i--)
        {
            Window window = windowsBeingClosed.at(i);
            if (!windowsStillBeingClosed.contains(window))
            {
                windowsBeingClosed.removeAt(i);
            }
        }

        emit windowListUpdated(windowList);
    }
}

void Application::updateApps(const QList<WindowInfo> &windowList)
{
    foreach (Desktop *d, m_runningApps + m_runningAppsOverflow)
    {
        bool found = false;

        foreach (WindowInfo info, windowList)
        {
            if (d->wid() == (int)info.window())
            {
                found = true;
                break;
            }

            if (d->pid() == (int)info.pid())
            {
                d->setWid(info.window());
                if (d->wid() == m_foregroundWindow)
                {
                    setForegroundOrientationForWindow(info.window());
                }
                found = true;
                break;
            }
        }

        if (!found)
        {
            d->setWid(-1);
        }
    }

    foreach (WindowInfo info, windowList)
    {
        bool found = false;
        foreach (Desktop *d, m_runningApps + m_runningAppsOverflow)
        {
            if (d->wid() == (int)info.window() ||
                d->pid() == (int)info.pid() ||
                (!info.iconName().isEmpty() &&
                 d->filename().contains(info.iconName())))
            {
                found = true;
            }
        }

        if (!found && !info.iconName().isEmpty())
        {
            QString path = "/usr/share/" + m_homeScreenDirectoryName + "/applications/" + info.iconName() + ".desktop";
            if (QFile::exists(path))
            {
                Desktop *d = new Desktop("/usr/share/" + m_homeScreenDirectoryName + "/applications/" + info.iconName() + ".desktop");
                d->setPid(info.pid());
                d->setWid(info.window());

                if (d->wid() == m_foregroundWindow)
                {
                    setForegroundOrientationForWindow(info.window());
                }
                if (m_runningApps.length() < m_runningAppsLimit)
                    m_runningApps << d;
                else
                    m_runningAppsOverflow << d;
            }
        }
    }
}

QVector<Atom> Application::getNetWmState(Display *display, Window window)
{
    QVector<Atom> atomList;

    Atom actualType;
    int actualFormat;
    ulong propertyLength;
    ulong bytesLeft;
    uchar *propertyData = 0;
    Atom netWmState = XInternAtom(display, "_NET_WM_STATE", false);

    // Step 1: Get the size of the list
    bool result = XGetWindowProperty(display, window, netWmState, 0, 0,
                                     false, XA_ATOM, &actualType,
                                     &actualFormat, &propertyLength,
                                     &bytesLeft, &propertyData);
    if (result == Success && actualType == XA_ATOM && actualFormat == 32)
    {
        atomList.resize(bytesLeft / sizeof(Atom));
        XFree(propertyData);

        // Step 2: Get the actual list
        if (XGetWindowProperty(display, window, netWmState, 0,
                               atomList.size(), false, XA_ATOM,
                               &actualType, &actualFormat,
                               &propertyLength, &bytesLeft,
                               &propertyData) == Success)
        {
            if (propertyLength != (ulong)atomList.size())
            {
                atomList.resize(propertyLength);
            }

            if (!atomList.isEmpty())
            {
                memcpy(atomList.data(), propertyData,
                       atomList.size() * sizeof(Atom));
            }
            XFree(propertyData);
        }
        else
        {
            qWarning("Unable to retrieve window properties: %i", (int) window);
            atomList.clear();
        }
    }

    return atomList;
}

void Application::loadTranslators()
{
    qtTranslator.load("qt_" + QLocale::system().name() + ".qm",
                      QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    commonTranslator.load("meegolabs-ux-components_" + QLocale::system().name() + ".qm",
                          QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    homescreenTranslator.load(m_homeScreenDirectoryName + "_" + QLocale::system().name() + ".qm",
                              QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    panelsTranslator.load("meego-ux-panels_" + QLocale::system().name(),
                          QLibraryInfo::location(QLibraryInfo::TranslationsPath) + ".qm");
    daemonTranslator.load("meego-ux-daemon_" + QLocale::system().name(),
                          QLibraryInfo::location(QLibraryInfo::TranslationsPath) + ".qm");
    mediaTranslator.load("meego-ux-media-qml_" + QLocale::system().name() + ".qm",
                         QLibraryInfo::location(QLibraryInfo::TranslationsPath));
}

void Application::toggleSwitcher()
{
    m_homeLongPressTimer->stop();
    if (taskSwitcher && taskSwitcher->isVisible())
        cleanupTaskSwitcher();
    else
        showTaskSwitcher();
}

void Application::launchDesktopByName(QString name)
{
    // verify that we don't have this already in our list
    foreach (Desktop *d, m_runningApps + m_runningAppsOverflow)
    {
        if (d->filename() == name)
        {
            foreach (Window win, openWindows)
            {
                if ((int)win != d->wid())
                {
                    minimizeWindow(win);
                }
            }
            if (d->wid() > 0)
            {
                raiseWindow(d->wid());
            }
            else
            {
                d->launch();
            }
            return;
        }
    }

    foreach (Window win, openWindows)
    {
        if (gridScreen && (int)win != gridScreen->winId())
        {
            minimizeWindow(win);
        }
    }

    Desktop *d = new Desktop(name, this);
    d->launch();
    if (m_runningApps.length() < m_runningAppsLimit)
        m_runningApps << d;
    else
        m_runningAppsOverflow << d;

    emit runningAppsChanged();
}

void Application::closeDesktopByName(QString name)
{
    for (int i = 0; i < m_runningApps.length(); i++)
    {
        Desktop *d = m_runningApps.at(i);
        if (d->filename() == name)
        {
            if (d->wid() > 0)
            {
                closeWindow(d->wid());
            }
            else if(d->pid() > 0)
            {
                // send kill

            }

            m_runningApps.takeAt(i);
            d->deleteLater();
            if (m_runningAppsOverflow.length() > 0 || m_runningApps.length() > m_runningAppsLimit)
                m_runningApps << m_runningAppsOverflow.takeFirst();
            emit runningAppsChanged();
            return;
        }
    }
}

void Application::raiseWindow(int windowId)
{
    XEvent ev;
    memset(&ev, 0, sizeof(ev));

    Display *display = QX11Info::display();

    Window rootWin = QX11Info::appRootWindow(QX11Info::appScreen());

    ev.xclient.type         = ClientMessage;
    ev.xclient.window       = windowId;
    ev.xclient.message_type = XInternAtom(QX11Info::display(), "_NET_ACTIVE_WINDOW", false);
    ev.xclient.format       = 32;
    ev.xclient.data.l[0]    = 1;
    ev.xclient.data.l[1]    = CurrentTime;
    ev.xclient.data.l[2]    = 0;

    XSendEvent(display,
               rootWin, False,
               SubstructureRedirectMask, &ev);
}

void Application::closeWindow(int windowId)
{
    XEvent ev;
    memset(&ev, 0, sizeof(ev));

    Display *display = QX11Info::display();

    Window rootWin = QX11Info::appRootWindow(QX11Info::appScreen());

    ev.xclient.type         = ClientMessage;
    ev.xclient.window       = windowId;
    ev.xclient.message_type = closeWindowAtom;
    ev.xclient.format       = 32;
    ev.xclient.data.l[0]    = CurrentTime;
    ev.xclient.data.l[1]    = rootWin;

    XSendEvent(display,
               rootWin, False,
               SubstructureRedirectMask, &ev);
}

uint Application::addGroup(uint notificationUserId,
                           const QString &eventType,
                           const QString &summary,
                           const QString &body,
                           const QString &action,
                           const QString &imageURI,
                           uint count,
                           const QString &identifier)
{
    return m_notificationDataStore->storeGroup(notificationUserId, eventType, summary, body, action, imageURI, count, identifier);
}

uint Application::addGroup(uint notificationUserId, const QString &eventType)
{
    return m_notificationDataStore->storeGroup(notificationUserId, eventType);
}

uint Application::addNotification(uint notificationUserId, uint groupId, const QString &eventType)
{
    return  m_notificationDataStore->storeNotification(notificationUserId, groupId, eventType);
}

uint Application::addNotification(uint notificationUserId, uint groupId, const QString &eventType, const QString &summary, const QString &body, const QString &action, const QString &imageURI, uint count, const QString &identifier)
{
    void *map = context_provider_map_new();
    context_provider_map_set_string(map, "notificationType", eventType.toAscii());
    context_provider_map_set_string(map, "notificationSummary", summary.toAscii());
    context_provider_map_set_string(map, "notificationBody", body.toAscii());
    context_provider_map_set_string(map, "notificationAction", action.toAscii());
    context_provider_map_set_string(map, "notificationImage", imageURI.toAscii());


    context_provider_set_map(CONTEXT_NOTIFICATIONS_LAST, map, 0);
    context_provider_map_free(map);

    context_provider_set_boolean(CONTEXT_NOTIFICATIONS_UNREAD, true);

    m_lastNotificationId = m_notificationDataStore->storeNotification(notificationUserId, groupId, eventType, summary, body, action, imageURI, count, identifier);

    return m_lastNotificationId;
}

QList < MNotificationGroup > Application::notificationGroupListWithIdentifiers(uint notificationUserId)
{
    return m_notificationDataStore->getNotificationGroupListWithId(notificationUserId);
}

QList < uint > Application::notificationIdList(uint notificationUserId)
{
    return m_notificationDataStore->getNotificationIdList(notificationUserId);
}

QList < MNotification >  Application::notificationListWithIdentifiers(uint notificationUserId)
{
    return m_notificationDataStore->getNotificationListWithId(notificationUserId);
}

uint Application::notificationUserId()
{
    return m_notificationDataStore->getNotificationUserId();
}

bool Application::removeGroup(uint notificationUserId, uint groupId)
{
    return m_notificationDataStore->deleteGroup(notificationUserId, groupId);
}

bool Application::removeNotification(uint notificationUserId, uint notificationId)
{
    bool result = false;

    if (notificationId == m_lastNotificationId)
    {
        m_lastNotificationId = 0;
        context_provider_set_null(CONTEXT_NOTIFICATIONS_LAST);
    }

    result = m_notificationDataStore->deleteNotification(notificationUserId, notificationId);
    if (result && m_notificationDataStore->notificationCount() == 0)
    {
        context_provider_set_boolean(CONTEXT_NOTIFICATIONS_UNREAD, false);
    }

    return result;
}

bool Application::updateGroup(uint notificationUserId, uint groupId, const QString &eventType)
{
    return m_notificationDataStore->updateGroup(notificationUserId, groupId, eventType);
}

bool Application::updateGroup(uint notificationUserId, uint groupId, const QString &eventType, const QString &summary, const QString &body, const QString &action, const QString &imageURI, uint count, const QString &identifier)
{
    return m_notificationDataStore->updateGroup(notificationUserId, groupId, eventType, summary, body, action, imageURI, count, identifier);
}

bool Application::updateNotification(uint notificationUserId, uint notificationId, const QString &eventType)
{
    return m_notificationDataStore->updateNotification(notificationUserId, notificationId, eventType);
}

bool Application::updateNotification(uint notificationUserId, uint notificationId, const QString &eventType, const QString &summary, const QString &body, const QString &action, const QString &imageURI, uint count, const QString &identifier)
{
    return m_notificationDataStore->updateNotification(notificationUserId, notificationId, eventType, summary, body, action, imageURI, count, identifier);
}

void Application::openStatusIndicatorMenu()
{
    context_provider_set_boolean(CONTEXT_NOTIFICATIONS_UNREAD, false);
    context_provider_set_null(CONTEXT_NOTIFICATIONS_LAST);

    if (statusIndicatorMenu)
    {
        statusIndicatorMenu->activateWindow();
        statusIndicatorMenu->raise();
        statusIndicatorMenu->show();
        return;
    }

    statusIndicatorMenu = new Dialog(true, false, useOpenGL);
    connect(statusIndicatorMenu->engine(), SIGNAL(quit()), this, SLOT(cleanupStatusIndicatorMenu()));
    statusIndicatorMenu->setAttribute(Qt::WA_X11NetWmWindowTypeDialog);
    statusIndicatorMenu->rootContext()->setContextProperty("notificationModel", m_notificationModel);
    statusIndicatorMenu->setSkipAnimation();

    statusIndicatorMenu->setSource(QUrl::fromLocalFile("/usr/share/meego-ux-daemon/statusindicatormenu.qml"));
    statusIndicatorMenu->show();
}

void Application::clearAllNotifications()
{
    m_notificationDataStore->clearAllNotifications();
}

void Application::setForegroundOrientationForWindow(uint wid)
{
    Atom actualType;
    int actualFormat;
    unsigned long numItems, bytesLeft;
    unsigned char *data = NULL;

    int result = XGetWindowProperty(QX11Info::display(),
                                    wid,
                                    foregroundOrientationAtom,
                                    0L, (long)BUFSIZ, false,
                                    XA_CARDINAL,
                                    &actualType,
                                    &actualFormat,
                                    &numItems,
                                    &bytesLeft,
                                    &data);
    if (result == Success && data != 0)
    {
        m_foregroundOrientation = *((unsigned long *)data);
        emit foregroundOrientationChanged();
        XFree(data);
    }

}

void Application::setOrientationLocked(bool locked)
{
    orientationLocked = locked;
    if (locked)
        emit stopOrientationSensor();
    else
        emit startOrientationSensor();
}

void Application::updateScreenSaver(Window window)
{
    Display *dpy = QX11Info::display();
    if (inhibitList.contains(window))
    {
        XSetScreenSaver(dpy, -1, 0, DefaultBlanking, DontAllowExposures);
    }
    else
    {
        XSetScreenSaver(dpy, m_screenSaverTimeout, 0, DefaultBlanking, DontAllowExposures);
    }
}

void Application::screenSaverTimeoutChanged()
{
    m_screenSaverTimeout = m_screenSaverTimeoutItem->value().toInt();
    updateScreenSaver((Window)m_foregroundWindow);
}
