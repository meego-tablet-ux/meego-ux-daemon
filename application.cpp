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
#include <QDBusConnectionInterface>
#include <QDBusPendingCall>
#include <QDBusReply>
#include <QDeclarativeEngine>
#include <QDeclarativeContext>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QSettings>
#include <QTimer>
#include <QInputContext>
#include <QInputContextFactory>
#include <QtCore/qmath.h>
#include <MGConfItem>
#include <context_provider.h>
#include <libcgroup.h>

#include "panelview.h"
#include "application.h"
#include "notificationsmanageradaptor.h"
#include "statusindicatormenuadaptor.h"
#include "lockscreenadaptor.h"
#include "dialog.h"
#include "desktop.h"
#include "notificationdatastore.h"
#include "notificationmodel.h"

#include <fcntl.h>
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
#include "atoms.h"

#define APPS_SOCK_PATH "/var/run/trm-app.sock"
#define MUSICSERVICE "com.meego.app.music"
#define MUSICINTERFACE "/com/meego/app/music"

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

    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sun_family = AF_LOCAL;
    strcpy(servaddr.sun_path, APPS_SOCK_PATH);

    connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));

    // This is a fire-n-forget message, so we don't care if there is
    // an error recieving, and we set a flag to prevent blocking incase
    // trm is borked and blocks the socket
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

Application::Application(int & argc, char ** argv, bool enablePanelView) :
    QApplication(argc, argv),
    locale(new meego::Locale(this)),
    m_orientation(1),
    m_enablePanelView(enablePanelView),
    taskSwitcher(NULL),
    lockScreen(NULL),
    panelsScreen(NULL),
    statusIndicatorMenu(NULL),
    m_runningAppsLimit(16),
    m_backlightSmartAjustTimer(NULL),
    m_homeActive(false),
    m_homePressTime(0),
    m_haveAppStore(QFile::exists("/usr/share/applications/com.intel.appup-tablet.desktop")),
    m_foregroundOrientation(2),
    m_notificationDataStore(NotificationDataStore::instance()),
    m_notificationModel(new NotificationModel),
    m_lastNotificationId(0),
    m_currentBacklightValue(80),
    m_requestedBacklightValue(80),
    m_screenOn(true)
{
    setApplicationName("meego-ux-daemon");

    setQuitOnLastWindowClosed(false);

    initAtoms();

    if (QSensor::sensorsForType("QOrientationSensor").length() > 0)
    {
        m_orientationSensorAvailable = true;
    }
    else
    {
        m_orientationSensorAvailable = false;
    }

    connect(&orientationSensor, SIGNAL(readingChanged()), SLOT(updateOrientation()));
    if (m_orientationSensorAvailable)
    {
        orientationSensor.start();
    }

    if (QSensor::sensorsForType("QAmbientLightSensor").length() > 0)
    {
        m_ambientLightSensorAvailable = true;
    }
    else
    {
        m_ambientLightSensorAvailable = false;
    }

    connect(&ambientLightSensor, SIGNAL(readingChanged()), SLOT(updateAmbientLight()));

    int (*oldXErrorHandler)(Display*, XErrorEvent*);

    Display *dpy = QX11Info::display();
    int screen = QX11Info::appScreen();
    XID root = QX11Info::appRootWindow(screen);
    windowTypeAtom = getAtom(ATOM_NET_WM_WINDOW_TYPE);
    windowTypeNormalAtom = getAtom(ATOM_NET_WM_WINDOW_TYPE_NORMAL);
    windowTypeDesktopAtom = getAtom(ATOM_NET_WM_WINDOW_TYPE_DESKTOP);
    windowTypeNotificationAtom = getAtom(ATOM_NET_WM_WINDOW_TYPE_NOTIFICATION);
    windowTypeDockAtom = getAtom(ATOM_NET_WM_WINDOW_TYPE_DOCK);
    clientListAtom = getAtom(ATOM_NET_CLIENT_LIST);
    closeWindowAtom = getAtom(ATOM_NET_CLOSE_WINDOW);
    skipTaskbarAtom = getAtom(ATOM_NET_WM_STATE_SKIP_TASKBAR);
    windowStateAtom = getAtom(ATOM_NET_WM_STATE);
    activeWindowAtom = getAtom(ATOM_NET_ACTIVE_WINDOW);
    foregroundOrientationAtom = getAtom(ATOM_MEEGO_ORIENTATION);
    inhibitScreenSaverAtom = getAtom(ATOM_MEEGO_INHIBIT_SCREENSAVER);

    // We need to explicitly turn off the DPMS timeouts since just configuring
    // XScreenSaver does not seem to do this.  Without this we will see a 10min timeout
    // kick in regardless of if the screen saver was disabled, inhibited, or configured
    // for a timeout of longer then 10 minutes
    DPMSSetTimeouts(QX11Info::display(), 0, 0, 0);

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
            Atom atom = getAtom(ATOM_Backlight);
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

                atom = getAtom(ATOM_BACKLIGHT);
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

    // Enable dynamic switching between gl and software rendering on systems
    // with graphics architectures that see a benifit in memory usage.
    MGConfItem *enableSwapItem = new MGConfItem("/meego/ux/EnableDynamicRendering");
    m_enableRenderingSwap = enableSwapItem->value().toBool();
    delete enableSwapItem;

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

    MGConfItem *appLauncherPathItem = new MGConfItem("/meego/ux/AppLauncherPath", this);
    if (!appLauncherPathItem ||
        appLauncherPathItem->value() == QVariant::Invalid ||
        !QFile::exists(appLauncherPathItem->value().toString()))
    {
        m_appLauncherPath = QString("/usr/share/meego-ux-appgrid/main.qml");
    }
    else
    {
        m_appLauncherPath = appLauncherPathItem->value().toString();
    }

    MGConfItem *lockScreenPathItem = new MGConfItem("/meego/ux/LockScreenPath", this);
    if (!lockScreenPathItem ||
        lockScreenPathItem->value() == QVariant::Invalid ||
        !QFile::exists(lockScreenPathItem->value().toString()))
    {
        m_lockscreenPath = QString("/usr/share/meego-ux-daemon/lockscreen.qml");
    }
    else
    {
        m_lockscreenPath = lockScreenPathItem->value().toString();
    }

    m_applicationDirectoriesItem = new MGConfItem("/meego/ux/ApplicationDirectories", this);
    if (m_applicationDirectoriesItem)
    {
        connect(m_applicationDirectoriesItem, SIGNAL(valueChanged()), this, SLOT(applicationDirectoriesUpdated()));
    }
    applicationDirectoriesUpdated();

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
    connect(locale, SIGNAL(localeChanged()), this, SLOT(loadTranslators()));

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

    m_lockScreenAdaptor = new LockscreenAdaptor(this);
    QDBusConnection::sessionBus().registerService("com.lockstatus");
    QDBusConnection::sessionBus().registerObject("/query", this);

    QDBusConnection::systemBus().registerService("com.lockstatus");
    QDBusConnection::systemBus().registerObject("/query", this);

    MGConfItem *homeKeyName = new MGConfItem("/meego/ux/HomeKey", this);
    if (homeKeyName && homeKeyName->value() != QVariant::Invalid)
    {
        grabHomeKey(homeKeyName->value().toByteArray());
    }

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
    mediaPauseKey    = grabKey("XF86AudioPause");
    mediaStopKey     = grabKey("XF86AudioStop");
    mediaPreviousKey = grabKey("XF86AudioPrev");
    mediaNextKey     = grabKey("XF86AudioNext");

    // listen for the standard volume control keys
    volumeUpKey   = grabKey("XF86AudioRaiseVolume");
    volumeDownKey = grabKey("XF86AudioLowerVolume");
    volumeMuteKey = grabKey("XF86AudioMute");

    powerKey = grabKey("XF86PowerOff");

    m_player = NULL;
    m_serviceWatcher = new QDBusServiceWatcher(MUSICSERVICE,
                                               QDBusConnection::sessionBus(),
                                               QDBusServiceWatcher::WatchForRegistration |
                                               QDBusServiceWatcher::WatchForUnregistration,
                                               this);
    connect(m_serviceWatcher, SIGNAL(serviceRegistered(const QString &)),
            this, SLOT(musicRegistered()));
    connect(m_serviceWatcher, SIGNAL(serviceUnregistered(const QString &)),
            this, SLOT(musicUnregistered()));

    QDBusReply<bool> registered= QDBusConnection::sessionBus().interface()->isServiceRegistered(MUSICSERVICE);
    if (registered.isValid()&&registered.value())
    {
        musicRegistered();
    }

    if (m_showPanelsAsHome)
    {
        if(m_enablePanelView)
        {
            panelsScreen = new PanelView();
        }
        else
        {
            panelsScreen = new Dialog(false,false,false);
            panelsScreen->setSource(QUrl::fromLocalFile("/usr/share/meego-ux-panels/main.qml"));
        }
        panelsScreen->setAttribute(Qt::WA_X11NetWmWindowTypeDesktop);
        panelsScreen->rootContext()->setContextProperty("notificationModel", m_notificationModel);
        panelsScreen->show();
        gridScreen = NULL;
    }
    else
    {
        gridScreen = new Dialog(false, false, false);
        gridScreen->setAttribute(Qt::WA_X11NetWmWindowTypeDesktop);
        gridScreen->setSource(QUrl::fromLocalFile(m_appLauncherPath));
        gridScreen->show();
        panelsScreen = NULL;
    }

    m_volumeLongPressTimer = new QTimer(this);
    m_volumeLongPressTimer->setInterval(3000);
    connect(m_volumeLongPressTimer, SIGNAL(timeout()), SLOT(volumeLongPressTimeout()));

    m_homeLongPressTimer = new QTimer(this);
    m_homeLongPressTimer->setInterval(500);
    connect(m_homeLongPressTimer, SIGNAL(timeout()), this, SLOT(toggleSwitcher()));

    m_powerLongPressTimer = new QTimer(this);
    m_powerLongPressTimer->setInterval(3000);
    connect(m_powerLongPressTimer, SIGNAL(timeout()), this, SLOT(showPowerDialog()));

    // We use this to create a window of time where we ignore power
    // button events right after the screensaver off is triggered
    m_powerIgnoreTimer = new QTimer(this);
    m_powerIgnoreTimer->setInterval(500);
    connect(m_powerIgnoreTimer, SIGNAL(timeout()), m_powerIgnoreTimer, SLOT(stop()));

    if (m_screenSaverTimeout > 0)
    {
        // Lock the screen
        lock();
    }

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
}

Application::~Application()
{
    while (!displayList.isEmpty())
        delete displayList.takeLast();

    if(m_player)
    {
        delete m_player;
    }
}

void Application::musicRegistered()
{
    m_player = new QDBusInterface(MUSICSERVICE,
                                  MUSICINTERFACE,
                                  MUSICSERVICE);
    if (!m_player->isValid())
    {
        musicUnregistered();
    }
}

void Application::musicUnregistered()
{
    if (m_player)
    {
        m_player->deleteLater();
    }
    m_player = NULL;
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
        if (m_enableRenderingSwap)
        {
            taskSwitcher->switchToGLRendering();
        }
        taskSwitcher->activateWindow();
        taskSwitcher->raise();
        taskSwitcher->show();
        return;
    }

    taskSwitcher = new Dialog(true, true, false);
    taskSwitcher->setAttribute(Qt::WA_X11NetWmWindowTypeDialog);
    taskSwitcher->setSystemDialog();
    connect(taskSwitcher->engine(), SIGNAL(quit()), this, SLOT(cleanupTaskSwitcher()));
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
        if (panelsScreen)
        {
            panelsScreen->show();
            panelsScreen->activateWindow();
            panelsScreen->raise();
        }
        else
        {
            if(m_enablePanelView)
            {
                panelsScreen = new PanelView();
            }
            else
            {
                panelsScreen = new Dialog(false,false, false);
                panelsScreen->setSource(QUrl::fromLocalFile("/usr/share/meego-ux-panels/main.qml"));
            }
            connect(panelsScreen, SIGNAL(requestClose()), this, SLOT(cleanupPanels()));
            panelsScreen->rootContext()->setContextProperty("notificationModel", m_notificationModel);
            panelsScreen->show();
        }
    }
}

void Application::showGrid()
{
    if (!m_showPanelsAsHome)
    {
        goHome();
    }
    else
    {
        if (gridScreen)
        {
            gridScreen->show();
            gridScreen->activateWindow();
            gridScreen->raise();
        }
        else
        {
            gridScreen = new Dialog(false, false, false);
            connect(gridScreen, SIGNAL(requestClose()), this, SLOT(cleanupGrid()));
            gridScreen->setSource(QUrl::fromLocalFile(m_appLauncherPath));
            gridScreen->show();
        }
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
    e.xclient.message_type = getAtom(ATOM_WM_CHANGE_STATE);
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
    if (m_enableRenderingSwap)
    {
        taskSwitcher->switchToSoftwareRendering();
    }
}

void Application::cleanupLockscreen()
{
    lockScreen->hide();
    if (m_enableRenderingSwap)
    {
        lockScreen->switchToSoftwareRendering();
    }
    m_lockScreenAdaptor->sendLockScreenOn(false);
}

void Application::cleanupStatusIndicatorMenu()
{
    statusIndicatorMenu->hide();
    if (m_enableRenderingSwap)
    {
        statusIndicatorMenu->switchToSoftwareRendering();
    }
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

    foreach (Window win, openWindows)
    {
        minimizeWindow(win);
    }
}

void Application::activateScreenSaver()
{
    XActivateScreenSaver(QX11Info::display());

    // For some reason we do not always get screen saver off
    // events till the screen is turned back on, when we get
    // and off and then an on event immediately after.
    //
    // I have only seen this when responding to the power key
    setScreenOn(false);
}

void Application::lock()
{
    if (lockScreen)
    {
        lockScreen->setSource(QUrl::fromLocalFile(m_lockscreenPath));
        lockScreen->activateWindow();
        lockScreen->raise();
        lockScreen->show();
    }
    else
    {
        lockScreen = new Dialog(true, true, true);
        lockScreen->setAttribute(Qt::WA_X11NetWmWindowTypeDialog);
        connect(lockScreen->engine(), SIGNAL(quit()), this, SLOT(cleanupLockscreen()));

        NotificationModel *model = new NotificationModel(lockScreen);
        model->setNotificationMergeRule(1);
        lockScreen->rootContext()->setContextProperty("notificationModel", model);

        lockScreen->setSource(QUrl::fromLocalFile(m_lockscreenPath));
        lockScreen->show();
    }

    m_lockScreenAdaptor->sendLockScreenOn(true);
    send_ux_msg(UX_CMD_FOREGROUND, ::getpid());
}

bool Application::x11EventFilter(XEvent *event)
{
    if (event->type == KeyRelease)
    {
        XKeyEvent * keyEvent = (XKeyEvent *)event;
        if (homeKeys.contains(keyEvent->keycode))
        {
	    if (m_homeLongPressTimer->isActive() && (!lockScreen || !lockScreen->isVisible()))
            {
                m_lockScreenAdaptor->home();
                m_homeLongPressTimer->stop();

                if (m_homeActive || (taskSwitcher && taskSwitcher->isVisible()))
                    toggleSwitcher();
                else
                    goHome();
            }
        }
        else if (keyEvent->keycode == menu)
        {
            toggleSwitcher();
        }
        else if (keyEvent->keycode == mediaPlayKey)
        {
            if(m_player)
            {
                m_player->asyncCall("play");
            }
        }
        else if (keyEvent->keycode == mediaPauseKey)
        {
            if(m_player)
            {
                m_player->asyncCall("pause");
            }
        }
        else if (keyEvent->keycode == mediaStopKey)
        {
            // The service does not really have a stop method, so just pause
            if(m_player)
            {
                m_player->asyncCall("pause");
            }
        }
        else if (keyEvent->keycode == mediaPreviousKey)
        {
            if(m_player)
            {
                m_player->asyncCall("prev");
            }
        }
        else if (keyEvent->keycode == mediaNextKey)
        {
            if(m_player)
            {
                m_player->asyncCall("next");
            }
        }
        else if (keyEvent->keycode == volumeUpKey)
        {
            // Lower the volume in 16 levels
            int v = qMin(volumeControl.volume() + qCeil(100.0/16.0), 100);
            //qDebug() << "Increasing volume from " << volumeControl.volume() << " to " << v;
            volumeControl.setVolume(v);
        }
        else if (keyEvent->keycode == volumeDownKey)
        {
            if (m_volumeLongPressTimer->isActive())
            {
                m_volumeLongPressTimer->stop();

                // Raise the volume in 16 levels
                int v = qMax(volumeControl.volume() - qFloor(100.0/16.0), 0);
                //qDebug() << "Decreasing volume from " << volumeControl.volume() << " to " << v;
                volumeControl.setVolume(v);
            }
        }
        else if (keyEvent->keycode == volumeMuteKey)
        {
            volumeControl.mute(!volumeControl.muted());
            // TODO: splash a volume indication UI
        }
        else if (keyEvent->keycode == powerKey)
        {
            if (m_powerLongPressTimer->isActive())
            {
                m_powerLongPressTimer->stop();

                lock();

                // activate the screen saver a very short period later
                // allow the lockscreen to setup first
                QTimer::singleShot(250, this, SLOT(activateScreenSaver()));
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
        else if (keyEvent->keycode == powerKey)
        {
            // Filter out duplicate events and also block any
            // events just after the screen turned on so we can
            // allow the act of pressing the power button to turn on
            // the screen without triggering another screen off event
            if (!m_powerLongPressTimer->isActive() &&
                    !m_powerIgnoreTimer->isActive())
            {
                m_powerLongPressTimer->start();
            }
        }
        else if (keyEvent->keycode == volumeDownKey)
        {
            m_volumeLongPressTimer->start();
        }
    }

    if (event->type == PropertyNotify &&
        event->xproperty.window == DefaultRootWindow(QX11Info::display()) &&
        event->xproperty.atom == clientListAtom)
    {
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
            XFree(data);

            if (m_foregroundWindow != (int)w && !isSystemModelDialog(w))
            {
                m_foregroundWindow = (int)w;
                emit foregroundWindowChanged();

                if (!lockScreen || !lockScreen->isVisible())
                {
                    XSelectInput(QX11Info::display(), w,
                                 PropertyChangeMask|KeyPressMask|KeyReleaseMask);
                }

                setForegroundOrientationForWindow(w);

                updateScreenSaver(w);

                if (m_enableRenderingSwap)
                {
                    if (panelsScreen && !m_enablePanelView)
                    {
                        Dialog *d = qobject_cast<Dialog *>(panelsScreen);
                        if (d->winId() == (int)w)
                            d->switchToGLRendering();
                        else
                            d->switchToSoftwareRendering();
                    }

                    if (gridScreen)
                    {
                        if (gridScreen->winId() == (int)w)
                            gridScreen->switchToGLRendering();
                        else
                            gridScreen->switchToSoftwareRendering();
                    }
                }
                int altWinId = 0;
                int homeWinId = m_showPanelsAsHome ? panelsScreen->winId() : gridScreen->winId();
                if (m_showPanelsAsHome && gridScreen)
                {
                    altWinId = gridScreen->winId();
                }
                else if (!m_showPanelsAsHome && panelsScreen)
                {
                    altWinId = panelsScreen->winId();
                }
                m_homeActive = m_foregroundWindow == homeWinId;

                if ((!panelsScreen || !panelsScreen->isVisible()) &&
                        (!gridScreen || !gridScreen->isVisible()) &&
                        (!lockScreen || !lockScreen->isVisible()))
                    updateWindowList();
                else
                    send_ux_msg(UX_CMD_FOREGROUND, ::getpid());

                if (altWinId && m_foregroundWindow != altWinId &&
                        (!lockScreen || !lockScreen->isVisible()))
                    minimizeWindow(altWinId);
            }
        }
        else
        {
            qDebug() << "XXX active window ???";
        }
    }
    else if (event->type == PropertyNotify &&
             event->xproperty.atom == inhibitScreenSaverAtom)
    {
        XPropertyEvent *e = (XPropertyEvent *) event;
        Window w = event->xproperty.window;

        if (e->state == PropertyNewValue && !inhibitList.contains(w))
        {
            inhibitList << w;
        }
        else if (e->state == PropertyDelete && inhibitList.contains(w))
        {
            inhibitList.removeAll(w);
        }

        if (m_foregroundWindow == (int)w)
            updateScreenSaver(w);
    }

    if (event->type == m_ss_event)
    {
        XScreenSaverNotifyEvent *sevent = (XScreenSaverNotifyEvent *) event;
        setScreenOn(sevent->state == ScreenSaverOff);
        return true;
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

                Atom iconAtom = getAtom(ATOM_NET_WM_ICON_NAME);
                Atom stringAtom = getAtom(ATOM_UTF8_STRING);
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

                // If _NET_WM_ICON_NAME invalid, try WM_ICON_NAME
                if (!meegoIconName)
                {
                    XGetIconName(dpy,
                                 wins[i],
                                 (char**)&meegoIconName);
                }

                // If WM_ICON_NAME invalid, try WM_NAME
                if (!meegoIconName)
                {
                    XFetchName(dpy,
                               wins[i],
                               (char**)&meegoIconName);
                }

                Atom notifyAtom = getAtom(ATOM_MEEGO_TABLET_NOTIFY);
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
                Atom pidAtom = getAtom(ATOM_NET_WM_PID);
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

                            Atom iconGeometryAtom = getAtom(ATOM_NET_WM_ICON_GEOMETRY);

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
                                                         pid));
                        }
                        else
                        {
                            windowsStillBeingClosed.append(wins[i]);
                        }
                    }
                    XFree(typeData);
                }
                if(meegoIconName)
                    XFree ((char *)meegoIconName);
                if(notifyIconName)
                    XFree ((char *)notifyIconName);
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

        updateApps(windowList);
    }
}

bool Application::namesMatchFuzzy(const Desktop& d, const WindowInfo& w) const
{
    QString d_name, d_exec, d_fn, w_name, w_title;

    // Filter the WindowInfo name
    w_name = w.iconName();
    if( w.iconName().isEmpty() ) {
        w_name = w.title();
    }
    w_name = w_name.toLower().replace("-","").replace("_","");

    if( w_name.isEmpty() ) {
        return false;
    }

    // Filter the Desktop names
    d_name = d.title().toLower().replace("-","").replace("_","");
    d_fn = d.filename().toLower().replace("-","").replace("_","");
    d_exec = d.exec().toLower().replace("-","").replace("_","");

    if( d_name.isEmpty() && d_fn.isEmpty() && d_exec.isEmpty() ) {
        return false;
    }

    if( d_name.contains(w_name) ||
        d_fn.contains(w_name) ||
        d_exec.contains(w_name) )
    {
        return true;
    }

    return false;
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
            // take this opportunity to clear out any stale
            // inhibitScreenSaver entries
            inhibitList.removeAll(d->wid());
            d->setWid(-1);
        }
    }

    foreach (WindowInfo info, windowList)
    {
        bool found = false;
        foreach (Desktop *d, m_runningApps + m_runningAppsOverflow)
        {
            if (d->wid() == (int)info.window() ||
                d->pid() == (int)info.pid())
            {
                found = true;
            }

            if(!found && namesMatchFuzzy(*d, info))
            {
                d->setPid(info.pid());
                d->setWid(info.window());
                found = true;
            }
        }

        if (!found && !info.iconName().isEmpty())
        {
            foreach (QString dir, m_applicationDirectories)
            {
                QString path = dir + "/" + info.iconName() + ".desktop";
                if (QFile::exists(path))
                {
                    Desktop *d = new Desktop(path);
                    if (d->contains("Desktop Entry/X-MEEGO-SKIP-TASKSWITCHER"))
                    {
                        delete d;
                    }
                    else
                    {
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

                    // A match for this window was found, so stop iterating
                    // through the directories so that the first match will
                    // shadow all additional directories
                    break;
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
    Atom netWmState = getAtom(ATOM_NET_WM_STATE);

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
    qtTranslator.load("qt_" + locale->locale() + ".qm",
                      QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    commonTranslator.load("meegolabs-ux-components_" + locale->locale() + ".qm",
                          QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    homescreenTranslator.load("meego-ux-appgrid_" + locale->locale() + ".qm",
                              QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    panelsTranslator.load("meego-ux-panels_" + locale->locale() + ".qm",
                          QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    daemonTranslator.load("meego-ux-daemon_" + locale->locale() + ".qm",
                          QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    mediaTranslator.load("meego-ux-media-qml_" + locale->locale() + ".qm",
                         QLibraryInfo::location(QLibraryInfo::TranslationsPath));

    foreach (QWidget *widget, QApplication::topLevelWidgets())
    {
        Dialog *dialog = static_cast<Dialog *>(widget);
        dialog->setSource(dialog->source());
    }
}

void Application::toggleSwitcher()
{
    m_homeLongPressTimer->stop();

    // If the lockscreen is active then just ignore the request
    if (lockScreen && lockScreen->isVisible())
        return;

    if (taskSwitcher && taskSwitcher->isVisible())
        cleanupTaskSwitcher();
    else
        showTaskSwitcher();
}

void Application::launchDesktopByName(QString name, QString cmd, QString cdata, bool noRaise)
{
    ///////////////////////////////////////////////////////////////////////
    // If the caller sends a fully qualified path to the desktop file, then
    // still match the the app against any desktop of the same name already
    // running, regardless of the full path.  This ensures that a vendors
    // use of "shadow" desktop directories is transparent, and that the
    // original and the shadowed desktop are not treated as two different
    // applications by the task switcher
    QString dname = QFileInfo(name).baseName();

    // verify that we don't have this already in our list
    foreach (Desktop *d, m_runningApps + m_runningAppsOverflow)
    {
        if (QFileInfo(d->filename()).baseName() == dname)
        {
            if (d->wid() > 0)
            {
                if (cmd.isEmpty())
                {
                    if (!noRaise) raiseWindow(d->wid());
                }
                else
                {
                    /////////////////////////////////////////////////
                    // Attempt to find the meego-qml-launcher dbus
                    // interface for sending additional commands to a
                    // live app, and if that fails then just exec the
                    // app with the extra args and let the singleton
                    // mechanism take care of the RPC

                    QString appName = dname;
                    appName.chop(8);

                    QString service = "com.meego.launcher." + appName;
                    QString object = "/com/meego/launcher";
                    QString interface = "com.meego.launcher";
                    QDBusInterface iface(service, object, interface);
                    if (iface.isValid())
                    {
                        QStringList args = d->exec().split(" ");
                        args << "--cmd" << cmd;
                        if (!cdata.isEmpty()) 
                            args << "--cdata" << cdata;
                        if (noRaise)
                            args << "--noraise";
                        iface.asyncCall(QLatin1String("raise"), args);
                    }
                    else
                    {
		        if (cdata.isEmpty())
			{
                            QProcess::startDetached(d->exec() + " --cmd " + cmd + (noRaise ? " --noraise" : ""));
                        }
                        else
			{
                            QProcess::startDetached(d->exec() + " --cmd " + cmd + " --cdata " + cdata + (noRaise ? " --noraise": ""));
                        }
                    }
                }
            }
            else
            {
                if (cmd.isEmpty())
                {
                    // If we get here then the user thinks the app is
                    // running when its really not, so ask the app to
                    // restore to start and store to it's previously
                    // running state
                    d->launch("restore", "", noRaise);
                }
                else
                {
                    d->launch(cmd, cdata, noRaise);
                }
            }
            return;
        }
    }

    Desktop *d = NULL;
    if (QFile::exists(name))
    {
        d = new Desktop(name, this);
    }
    else
    {
        // Allow callers to just specify the filename and let the launcher
        // figure out what directores desktop file can be found
        foreach (QString dir, m_applicationDirectories)
        {
            QString fullname = dir + "/" + dname + ".desktop";
            if (QFile::exists(fullname))
            {
                d = new Desktop(fullname, this);
                break;
            }
        }
    }
    if (d && d->isValid())
    {
        d->launch(cmd, cdata);
        if (!d->contains("Desktop Entry/X-MEEGO-SKIP-TASKSWITCHER"))
        {
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

void Application::closeDesktopByWid(unsigned int wid)
{
    for (int i = 0; i < m_runningApps.length(); i++)
    {
        Desktop *d = m_runningApps.at(i);
        if (d->wid() == (int)wid)
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

QString Application::getDesktopByWid(unsigned int wid)
{
    for (int i = 0; i < m_runningApps.length(); i++)
    {
        Desktop *d = m_runningApps.at(i);
        if (d->wid() == (int)wid)
        {
            return d->filename();
        }
    }

    return QString();
}

void Application::raiseWindow(int windowId)
{
    XEvent ev;
    memset(&ev, 0, sizeof(ev));

    Display *display = QX11Info::display();

    Window rootWin = QX11Info::appRootWindow(QX11Info::appScreen());

    ev.xclient.type         = ClientMessage;
    ev.xclient.window       = windowId;
    ev.xclient.message_type = getAtom(ATOM_NET_ACTIVE_WINDOW);
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
                           const QString &declineAction,
                           uint count,
                           const QString &identifier)
{
    return m_notificationDataStore->storeGroup(notificationUserId, eventType, summary, body, action, imageURI, declineAction, count, identifier);
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
    QString declineAction;
    return m_notificationDataStore->storeGroup(notificationUserId, eventType, summary, body, action, imageURI, declineAction, count, identifier);
}

uint Application::addGroup(uint notificationUserId, const QString &eventType)
{
    return m_notificationDataStore->storeGroup(notificationUserId, eventType);
}

uint Application::addNotification(uint notificationUserId, uint groupId, const QString &eventType)
{
    return  m_notificationDataStore->storeNotification(notificationUserId, groupId, eventType);
}

uint Application::addNotification(uint notificationUserId, uint groupId, const QString &eventType, const QString &summary, const QString &body, const QString &action, const QString &imageURI, const QString &declineAction, uint count, const QString &identifier)
{
    m_lastNotificationId = m_notificationDataStore->storeNotification(notificationUserId, groupId, eventType, summary, body, action, imageURI, declineAction, count, identifier);

    if (eventType != MNotification::HardNotification &&
        eventType != MNotification::PhoneIncomingCall)
    {
        void *map = context_provider_map_new();
        context_provider_map_set_string(map, "notificationType", eventType.toAscii());
        context_provider_map_set_string(map, "notificationSummary", summary.toUtf8());
        context_provider_map_set_string(map, "notificationBody", body.toUtf8());
        context_provider_map_set_string(map, "notificationAction", action.toAscii());
        context_provider_map_set_string(map, "notificationImage", imageURI.toAscii());

        context_provider_set_map(CONTEXT_NOTIFICATIONS_LAST, map, 0);
        context_provider_map_free(map);

        context_provider_set_boolean(CONTEXT_NOTIFICATIONS_UNREAD, true);
    }
    else
    {
        // We provide this mapping of special MNotification types
        // to an incoming call to catch any left over apps that might
        // have learned how to do this from handset.  The telepathy
        // backend will directly call into meego-ux-alarms.
        QDBusInterface alarmd("org.meego.alarms",
                              "/incomingCall",
                              "org.meego.alarms");
        alarmd.call("incomingCall",
                    summary,
                    body,
                    action,
                    declineAction,
                    "" /* ringer file */,
                    imageURI);
    }

    return m_lastNotificationId;
}

uint Application::addNotification(uint notificationUserId, uint groupId, const QString &eventType, const QString &summary, const QString &body, const QString &action, const QString &imageURI, uint count, const QString &identifier)
{
    QString declineAction;
    return addNotification(notificationUserId, groupId, eventType, summary, body, action, imageURI, declineAction, count, identifier);
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

bool Application::updateGroup(uint notificationUserId, uint groupId, const QString &eventType, const QString &summary, const QString &body, const QString &action, const QString &imageURI, const QString &declineAction, uint count, const QString &identifier)
{
    return m_notificationDataStore->updateGroup(notificationUserId, groupId, eventType, summary, body, action, imageURI, declineAction, count, identifier);
}

bool Application::updateGroup(uint notificationUserId, uint groupId, const QString &eventType, const QString &summary, const QString &body, const QString &action, const QString &imageURI, uint count, const QString &identifier)
{
    QString declineAction;
    return m_notificationDataStore->updateGroup(notificationUserId, groupId, eventType, summary, body, action, imageURI, declineAction, count, identifier);
}

bool Application::updateNotification(uint notificationUserId, uint notificationId, const QString &eventType)
{
    return m_notificationDataStore->updateNotification(notificationUserId, notificationId, eventType);
}

bool Application::updateNotification(uint notificationUserId, uint notificationId, const QString &eventType, const QString &summary, const QString &body, const QString &action, const QString &imageURI, const QString &declineAction, uint count, const QString &identifier)
{
    return m_notificationDataStore->updateNotification(notificationUserId, notificationId, eventType, summary, body, action, imageURI, declineAction, count, identifier);
}

bool Application::updateNotification(uint notificationUserId, uint notificationId, const QString &eventType, const QString &summary, const QString &body, const QString &action, const QString &imageURI, uint count, const QString &identifier)
{
    QString declineAction;
    return m_notificationDataStore->updateNotification(notificationUserId, notificationId, eventType, summary, body, action, imageURI, declineAction, count, identifier);
}

void Application::openStatusIndicatorMenu()
{
    context_provider_set_boolean(CONTEXT_NOTIFICATIONS_UNREAD, false);
    context_provider_set_null(CONTEXT_NOTIFICATIONS_LAST);

    if (statusIndicatorMenu)
    {
        if (m_enableRenderingSwap)
        {
            statusIndicatorMenu->switchToGLRendering();
        }
        statusIndicatorMenu->activateWindow();
        statusIndicatorMenu->raise();
        statusIndicatorMenu->show();
        return;
    }

    statusIndicatorMenu = new Dialog(true, true, false);
    statusIndicatorMenu->setAttribute(Qt::WA_X11NetWmWindowTypeDialog);
    statusIndicatorMenu->setSystemDialog();
    connect(statusIndicatorMenu->engine(), SIGNAL(quit()), this, SLOT(cleanupStatusIndicatorMenu()));
    statusIndicatorMenu->rootContext()->setContextProperty("notificationModel", m_notificationModel);

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

void Application::updateScreenSaver(Window window)
{
    Display *dpy = QX11Info::display();

    if (inhibitList.contains(window))
    {
        XSetScreenSaver(dpy, 0, 0, DefaultBlanking, DontAllowExposures);
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
    if (!m_ambientLightSensorAvailable)
        return;

    QAmbientLightReading *reading = ambientLightSensor.reading();
    switch(reading->lightLevel())
    {
    case QAmbientLightReading::Dark:
        smartSetBacklight(20);
        break;
    case QAmbientLightReading::Twilight:
        smartSetBacklight(40);
        break;
    case QAmbientLightReading::Light:
        smartSetBacklight(60);
        break;
    case QAmbientLightReading::Bright:
        smartSetBacklight(80);
        break;
    case QAmbientLightReading::Sunny:
        smartSetBacklight(100);
        break;
    case QAmbientLightReading::Undefined:
        break;
    }
}

void Application::updateOrientation()
{
    int qmlOrient = -1;
    M::OrientationAngle mtfOrient;
    switch (orientationSensor.reading()->orientation())
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
    case QOrientationReading::TopUp:
        mtfOrient = M::Angle0;
        qmlOrient = 1;
        break;
    default:
        // ignore faceup and facedown
        break;
    }

    if (qmlOrient == -1)
        return;

    m_orientation = qmlOrient;
    emit orientationChanged();

    // Need to tell the MInputContext plugin to rotate the VKB too
    QMetaObject::invokeMethod(inputContext(),
                              "notifyOrientationChanged",
                              Q_ARG(M::OrientationAngle, mtfOrient));
}

void Application::automaticBacklightControlChanged()
{
    m_automaticBacklight = m_automaticBacklightItem->value(true).toBool();
    if (m_ambientLightSensorAvailable)
    {
        if (m_automaticBacklight && m_screenOn)
        {
            ambientLightSensor.start();
        }
        else
        {
            ambientLightSensor.stop();
        }
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
    m_currentBacklightValue = percentage;
    foreach (DisplayInfo *info, displayList)
    {
        long value = info->valueForPercentage(m_currentBacklightValue);
        XRRChangeOutputProperty (QX11Info::display(), info->output(),
                                 info->backlightAtom(),
                                 XA_INTEGER, 32,
                                 PropModeReplace,
                                 (unsigned char *) &value, 1);
    }
}

void Application::smartSetBacklight(int percentage)
{
    // Any request to raise the backlight are immediately addressed,
    // but request to lower the backlight require a time period of
    // no other lowering request so that we don't incorrectly adjust
    // the brightness because the users hand cast a shadow on the sensor
    // or the user just walk across a dark section of a room.
    m_requestedBacklightValue = percentage;
    if (m_requestedBacklightValue == m_currentBacklightValue)
    {
        if (m_backlightSmartAjustTimer && m_backlightSmartAjustTimer->isActive())
        {
            // the shadow as passed so kill the timer
            m_backlightSmartAjustTimer->stop();
        }
    }
    else if (m_requestedBacklightValue > m_currentBacklightValue)
    {
        if (m_backlightSmartAjustTimer && m_backlightSmartAjustTimer->isActive())
        {
            // all increase request trump outstanding lower request
            m_backlightSmartAjustTimer->stop();
        }
        setBacklight(m_requestedBacklightValue);
    }
    else
    {
        if (!m_backlightSmartAjustTimer)
        {
            m_backlightSmartAjustTimer = new QTimer(this);
            m_backlightSmartAjustTimer->setInterval(1000 * 60);
            connect(m_backlightSmartAjustTimer, SIGNAL(timeout()), this, SLOT(doSetBacklight()));
        }

        m_backlightSmartAjustTimer->start();
    }
}

void Application::doSetBacklight()
{
    setBacklight(m_requestedBacklightValue);
}

void Application::setScreenOn(bool value)
{
    if (value == m_screenOn)
        return;

    m_screenOn = value;
    emit screenOnChanged();

    m_lockScreenAdaptor->sendScreenOn(m_screenOn);

    if (m_screenOn)
    {
        m_powerIgnoreTimer->start();

        if (m_orientationSensorAvailable)
            orientationSensor.start();

        automaticBacklightControlChanged();

        // Re-enable updates
        if (lockScreen)
        {
            lockScreen->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
        }

        send_ux_msg(UX_CMD_SCREEN_ON, 0);
        context_provider_set_string("Session.State", "normal");
    }
    else
    {
        // Open the lockscreen so that it's ready when the screen comes back on,
        // and to ensure all apps take measures to save power because they are
        // no longer in the foreground
        lock();

        if (m_orientationSensorAvailable)
            orientationSensor.stop();
        if (m_ambientLightSensorAvailable && m_automaticBacklight)
            ambientLightSensor.stop();

        // The lookscreen window is still visible as far as Qt knows, so
        // manually disable updates to prevent issues with painting while the
        // screen is off
        if (lockScreen)
        {
            lockScreen->setViewportUpdateMode(QGraphicsView::NoViewportUpdate);
        }

        // Outstanding request to dim the display are dropped since we want
        // the display to be bright the next time we turn the screen back on
        if (m_backlightSmartAjustTimer && m_backlightSmartAjustTimer->isActive())
        {
            m_backlightSmartAjustTimer->stop();
        }

        send_ux_msg(UX_CMD_SCREEN_OFF, 0);
        context_provider_set_string("Session.State", "blanked");
    }
}

void Application::applicationDirectoriesUpdated()
{
    m_applicationDirectories.clear();
    if (!m_applicationDirectoriesItem ||
         m_applicationDirectoriesItem->value() == QVariant::Invalid)
    {
        // Fallback for images that are not setting this config
        m_applicationDirectories << "/usr/share/meego-ux-appgrid/virtual-applications";
        m_applicationDirectories << "/usr/share/meego-ux-appgrid/applications";
        m_applicationDirectories << "~/.local/share/applications";
        m_applicationDirectories << "/usr/share/applications";
    }
    else
    {
        m_applicationDirectories = m_applicationDirectoriesItem->value().toStringList();
    }

    emit applicationDirectoriesChanged();
}

void Application::cleanupPanels()
{
    panelsScreen->deleteLater();
    panelsScreen = NULL;
}

void Application::cleanupGrid()
{
    gridScreen->deleteLater();
    gridScreen = NULL;
}

void Application::showPowerDialog()
{
    QDBusInterface iface("org.meego.shutdownverification",
                         "/org/meego/shutdownverification",
                         "org.meego.shutdownverification",
                         QDBusConnection::sessionBus());
    if(iface.isValid())
    {
        iface.asyncCall(QLatin1String("show"));
    }
}

void Application::volumeLongPressTimeout()
{
    // If the user does a press-n-hold on the volume down
    // button then we interpet that is a panic request to
    // make the device quiet.
    volumeControl.setVolume(0);
}

bool Application::isSystemModelDialog(unsigned target)
{
    Atom actualType;
    int actualFormat;
    unsigned long numWindowItems, bytesLeft;
    unsigned char *data = NULL;

    int result = XGetWindowProperty(QX11Info::display(),
                                    target,
                                    getAtom(ATOM_MEEGO_SYSTEM_DIALOG),
                                    0, 0x7fffffff,
                                    false, XA_WINDOW,
                                    &actualType,
                                    &actualFormat,
                                    &numWindowItems,
                                    &bytesLeft,
                                    &data);

    if (result == Success && data != None)
    {
        XFree(data);
        return true;
    }

    return false;
}

bool Application::lockScreenOn()
{
    return lockScreen && lockScreen->isVisible();
}
