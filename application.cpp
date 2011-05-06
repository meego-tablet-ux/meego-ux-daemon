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
#include <QDBusPendingCall>
#include <QDeclarativeEngine>
#include <QDeclarativeContext>
#include <QLibraryInfo>
#include <QSettings>
#include <QTimer>
#include <QInputContext>
#include <QInputContextFactory>
#include <MGConfItem>
#include <context_provider.h>
#include <libcgroup.h>

#include "application.h"
#include "notificationsmanageradaptor.h"
#include "statusindicatormenuadaptor.h"
#include "dialog.h"
#include "desktop.h"
#include "notificationdatastore.h"
#include "notificationmodel.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include <QX11Info>
#include <X11/Xatom.h>
#include <X11/keysymdef.h>
#include <X11/extensions/scrnsaver.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/dpms.h>

#define APPS_SOCK_PATH "/var/run/trm-app.sock"

#define CONTEXT_NOTIFICATIONS_LAST "Notifications.Last"
#define CONTEXT_NOTIFICATIONS_UNREAD "Notifications.Unread"

Q_DECLARE_METATYPE(QList<MNotification>);
Q_DECLARE_METATYPE(QList<MNotificationGroup>);


enum ux_info_cmd {
    UX_CMD_FOREGROUND = 1,
    UX_CMD_LAUNCHED,
    UX_CMD_SCREEN_OFF,
    UX_CMD_SCREEN_ON
};

struct ux_msg {
    unsigned int data;
    enum ux_info_cmd cmd;
};

static void send_ux_msg(ux_info_cmd cmd, unsigned int data)
{
    // don't send repeated foreground notifications for the same pid
    static unsigned int lastForegroundId = 0;
    if (cmd == UX_CMD_FOREGROUND)
    {
        if (data == lastForegroundId)
            return;
        lastForegroundId = data;
    }

    struct ux_msg *msg = (struct ux_msg *) malloc(sizeof(struct ux_msg));
    if (!msg)
        return;

    msg->cmd = cmd;
    msg->data = data;

    int sockfd;
    struct sockaddr_un servaddr;

    if (!QFile::exists(APPS_SOCK_PATH))
    {
        free(msg);
        return;
    }

    sockfd = socket(AF_LOCAL, SOCK_STREAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sun_family = AF_LOCAL;
    strcpy(servaddr.sun_path, APPS_SOCK_PATH);

    connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
    write(sockfd, msg, sizeof(struct ux_msg));

    free(msg);
    close(sockfd);
}

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

class DisplayInfo {
public:
    DisplayInfo(RROutput output, int minBrightness, int maxBrightness, Atom backlightAtom) :
        m_output(output),
        m_minBrightness(minBrightness),
        m_maxBrightness(maxBrightness),
        m_backlightAtom(backlightAtom) {
    }

    RROutput output() {
        return m_output;
    }

    int valueForPercentage(int percentage) {
        return ((percentage * (m_maxBrightness - m_minBrightness))/100);
    }

    Atom backlightAtom() {
        return m_backlightAtom;
    }

private:
    RROutput m_output;
    int m_minBrightness;
    int m_maxBrightness;
    Atom m_backlightAtom;
};

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

    connect(&orientationSensor, SIGNAL(readingChanged()), SLOT(updateOrientation()));
    orientationSensor.start();

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

    // Query all the info we will need for setting backlight values
    int major, minor;
    if (XRRQueryVersion (dpy, &major, &minor))
    {
        XRRScreenResources  *resources = XRRGetScreenResources (dpy, root);
        for (int index = 0; index < resources->noutput; index++)
        {
            unsigned long   nitems;
            unsigned long   bytes_after;
            unsigned char *prop = NULL;
            Atom actual_type;
            int actual_format;

            RROutput output = resources->outputs[index];

            // Attempt to support both the "Backlight" and
            // "BACKLIGHT" convention
            Atom atom = XInternAtom(dpy, "Backlight", True);
            if ((XRRGetOutputProperty (dpy, output, atom,
                                      0, 4, False, False, None,
                                      &actual_type, &actual_format,
                                      &nitems, &bytes_after,
                                      &prop) != Success) ||
                    (actual_type != XA_INTEGER) ||
                    (nitems != 1) ||
                    (actual_format != 32))
            {
                if (prop)
                {
                    XFree(prop);
                    prop = NULL;
                }

                atom = XInternAtom(dpy, "BACKLIGHT", True);
                if ((XRRGetOutputProperty (dpy, output, atom,
                                          0, 4, False, False, None,
                                          &actual_type, &actual_format,
                                          &nitems, &bytes_after,
                                          &prop) != Success) ||
                        (actual_type != XA_INTEGER) ||
                        (nitems != 1) ||
                        (actual_format != 32))
                {
                    if (prop)
                    {
                        XFree(prop);
                    }
                    continue;
                }
            }
            XFree(prop);

            XRRPropertyInfo *info = XRRQueryOutputProperty(dpy, output, atom);
            if (info->range && info->num_values == 2)
            {
                displayList << new DisplayInfo(output, info->values[0], info->values[1], atom);
            }
            XFree(info);
        }
    }

    m_automaticBacklightItem = new MGConfItem("/meego/ux/AutomaticBacklightControl", this);
    connect(m_automaticBacklightItem, SIGNAL(valueChanged()), this, SLOT(automaticBacklightControlChanged()));

    m_manualBacklightItem = new MGConfItem("/meego/ux/ManualBacklightValue", this);
    connect(m_manualBacklightItem, SIGNAL(valueChanged()), this, SLOT(updateBacklight()));

    // This will trigger the backlight to be updated either using a value
    // from the ambient light sensor, or from the manually setting, or
    // full brightness.
    automaticBacklightControlChanged();

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
    grabHomeKey("XF86HomePage");

    // enable developers quick access to the application switcher
    menu = grabKey("Menu");

    // listen for the media control keys so that when bluez maps the A2DP
    // controls to the standard media keys, we can control media playback
    // regardless of which application is in the foreground
    mediaPlayKey     = grabKey("XF86AudioPlay");
    mediaPauseKey     = grabKey("XF86AudioPause");
    mediaStopKey     = grabKey("XF86AudioStop");
    mediaPreviousKey = grabKey("XF86AudioPrev");
    mediaNextKey     = grabKey("XF86AudioNext");

    // listen for the standard volume control keys
    volumeUpKey   = grabKey("XF86AudioRaiseVolume");
    volumeDownKey = grabKey("XF86AudioLowerVolume");
    volumeMuteKey = grabKey("XF86AudioMute");

    powerKey = grabKey("XF86PowerOff");

    m_player = new QDBusInterface("com.meego.app.music",
                                  "/com/meego/app/music",
                                  "com.meego.app.music");


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

    struct cgroup_group_spec spec = {
        "unlimited", { "freezer"}
    };

    if (!cgroup_init())
    {
        if (!cgroup_register_unchanged_process(::getpid(), 0))
        {
            cgroup_change_cgroup_path(spec.path, ::getpid(), spec.controllers);
        }
    }
    send_ux_msg(UX_CMD_LAUNCHED, ::getpid());
}

Application::~Application()
{
    while (!displayList.isEmpty())
        delete displayList.takeLast();

    delete m_player;
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
        // TODO: We need a way of letting trm know that a third
        //       party lockscreen is now in the foreground
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

            NotificationModel *model = new NotificationModel(lockScreen);
            model->setFilterKey("/meego/ux/settings/lockscreen/filters");
            lockScreen->rootContext()->setContextProperty("notificationModel", model);

            lockScreen->setSource(QUrl::fromLocalFile("/usr/share/meego-ux-daemon/lockscreen.qml"));
            lockScreen->show();
        }
        send_ux_msg(UX_CMD_FOREGROUND, ::getpid());
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
        else if (keyEvent->keycode == mediaPlayKey)
        {
            m_player->asyncCall("play");
        }
        else if (keyEvent->keycode == mediaPauseKey)
        {
            m_player->asyncCall("pause");
        }
        else if (keyEvent->keycode == mediaStopKey)
        {
            // The service does not really have a stop method, so just pause
            m_player->asyncCall("pause");
        }
        else if (keyEvent->keycode == mediaPreviousKey)
        {
            m_player->asyncCall("prev");
        }
        else if (keyEvent->keycode == mediaNextKey)
        {
            m_player->asyncCall("next");
        }
        else if (keyEvent->keycode == volumeUpKey)
        {
            // increase volume and show UI indication
        }
        else if (keyEvent->keycode == volumeDownKey)
        {
            // decrease volume and show UI indication
        }
        else if (keyEvent->keycode == volumeMuteKey)
        {
            // mute volume and show UI indication
        }
        else if (keyEvent->keycode == powerKey)
        {
            // turn off the display and trigger the lockscreen
            DPMSForceLevel(QX11Info::display(), DPMSModeOff);
            if (!lockScreen)
                lock();
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
                if (m_foregroundWindow != (int)w)
                {
                    m_foregroundWindow = (int)w;
                    emit foregroundWindowChanged();

                    XSelectInput(QX11Info::display(), w, PropertyChangeMask);
                    setForegroundOrientationForWindow(w);

                    updateScreenSaver(w);

                    int homeWinId = m_showPanelsAsHome ? panelsScreen->winId() : gridScreen->winId();
                    int altWinId = m_showPanelsAsHome ? gridScreen->winId() : panelsScreen->winId();
                    m_homeActive = m_foregroundWindow == homeWinId;

                    if (m_homeActive)
                        send_ux_msg(UX_CMD_FOREGROUND, ::getpid());
                    else
                        updateWindowList();

                    if (!taskSwitcher && m_foregroundWindow != altWinId)
                        minimizeWindow(altWinId);
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
        Window w = event->xproperty.window;
        if (inhibitList.contains(w))
        {
            inhibitList.removeAll(w);
        }
        else
        {
            inhibitList << w;
        }
        updateScreenSaver(w);
    }

    if (event->type == m_ss_event)
    {
        XScreenSaverNotifyEvent *sevent = (XScreenSaverNotifyEvent *) event;
        if (sevent->state == ScreenSaverOn)
        {
            send_ux_msg(UX_CMD_SCREEN_ON, 0);
            lock();
            return true;
        }
        else if (sevent->state == ScreenSaverOff)
        {
            send_ux_msg(UX_CMD_SCREEN_OFF, 0);
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
                    if ((int)wins[i] == m_foregroundWindow)
                    {
                        send_ux_msg(UX_CMD_FOREGROUND, pid);
                    }
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
                {
                    m_runningApps << d;
                    emit runningAppsChanged();
                }
                else
                {
                    m_runningAppsOverflow << d;
                }
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
    connect(d, SIGNAL(launched(int)), this, SLOT(desktopLaunched(int)));
    d->launch();
    if (m_runningApps.length() < m_runningAppsLimit)
    {
        m_runningApps << d;
        emit runningAppsChanged();
    }
    else
    {
        m_runningAppsOverflow << d;
    }
}

void Application::closeDesktopByName(QString name)
{
    for (int i = 0; i < m_runningApps.length(); i++)
    {
        Desktop *d = m_runningApps.at(i);
        if (d->filename() == name)
        {
            d->terminate();

            m_runningApps.takeAt(i);
            d->deleteLater();
            if (m_runningAppsOverflow.length() > 0 || m_runningApps.length() > m_runningAppsLimit)
            {
                m_runningApps << m_runningAppsOverflow.takeFirst();
            }
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

void Application::desktopLaunched(int pid)
{
    send_ux_msg(UX_CMD_LAUNCHED, pid);
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

namespace M {
    enum OrientationAngle { Angle0=0, Angle90=90, Angle180=180, Angle270=270 };
}

void Application::updateAmbientLight()
{
    QAmbientLightReading *reading = ambientLightSensor.reading();
    switch(reading->lightLevel())
    {
    case QAmbientLightReading::Dark:
        setBacklight(20);
        break;
    case QAmbientLightReading::Twilight:
        setBacklight(40);
        break;
    case QAmbientLightReading::Light:
        setBacklight(60);
        break;
    case QAmbientLightReading::Bright:
        setBacklight(80);
        break;
    case QAmbientLightReading::Sunny:
    case QAmbientLightReading::Undefined:
        setBacklight(100);
        break;
    }
}

void Application::updateOrientation()
{
    int orientation = orientationSensor.reading()->orientation();

    int qmlOrient;
    M::OrientationAngle mtfOrient;
    switch (orientation)
    {
    case QOrientationReading::LeftUp:
        mtfOrient = M::Angle270;
        qmlOrient = 2;
        break;
    case QOrientationReading::TopDown:
        mtfOrient = M::Angle180;
        qmlOrient = 3;
        break;
    case QOrientationReading::RightUp:
        mtfOrient = M::Angle90;
        qmlOrient = 0;
        break;
    default: // assume QOrientationReading::TopUp
        mtfOrient = M::Angle0;
        qmlOrient = 1;
        break;
    }

    if (panelsScreen)
    {
        panelsScreen->setOrientation(qmlOrient);
    }

    // Need to tell the MInputContext plugin to rotate the VKB too
    QMetaObject::invokeMethod(inputContext(),
                              "notifyOrientationChanged",
                              Q_ARG(M::OrientationAngle, mtfOrient));
}

void Application::automaticBacklightControlChanged()
{
    m_automaticBacklight = m_automaticBacklightItem->value().toBool();
    if (m_automaticBacklight)
    {
        ambientLightSensor.start();
    }
    else
    {
        ambientLightSensor.stop();
    }
    updateBacklight();
}

void Application::updateBacklight()
{
    if (m_automaticBacklight)
    {
        updateAmbientLight();
        return;
    }

    int requested = 100; // default to full brightness
    if (m_manualBacklightItem->value() != QVariant::Invalid)
    {
        requested = m_manualBacklightItem->value().toInt();
        if (requested > 100) requested = 100;
        if (requested < 0) requested = 0;
    }

    setBacklight(requested);
}

void Application::setBacklight(int percentage)
{
    foreach (DisplayInfo *info, displayList)
    {
        long value = info->valueForPercentage(percentage);
        XRRChangeOutputProperty (QX11Info::display(), info->output(),
                                 info->backlightAtom(),
                                 XA_INTEGER, 32,
                                 PropModeReplace,
                                 (unsigned char *) &value, 1);
    }
}
