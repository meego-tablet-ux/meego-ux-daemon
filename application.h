/*
 * Copyright 2011 Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef APPLICATION_H
#define APPLICATION_H

#include <QApplication>
#include <QDBusConnection>
#include <QDeclarativeEngine>
#include <QDeclarativeListProperty>
#include <QTranslator>
#include <MNotification>
#include <mnotificationgroup.h>
#include <alarm-interface.h>
#include <alarm-types.h>
#include <alarm-dbus-names.h>

#include <QAmbientLightReading>
#include <QAmbientLightSensor>
#include <QOrientationSensor>
QTM_USE_NAMESPACE

#include "volumecontrol.h"
#include "desktop.h"
#include "dialog.h"

#include "windowinfo.h"

#define CG_CONTROLLER_MAX 100

struct cgroup_group_spec {
        char path[FILENAME_MAX];
        const char *controllers[CG_CONTROLLER_MAX];
};

class Dialog;
class Desktop;
class MGConfItem;
class QSettings;
class NotificationDataStore;
class NotificationModel;
class DisplayInfo;
class LockscreenAdaptor;

class Application : public QApplication
{
    Q_OBJECT
    Q_PROPERTY(int orientation READ getOrientation NOTIFY orientationChanged)
    Q_PROPERTY(bool orientationLocked READ getOrientationLocked WRITE setOrientationLocked);
    Q_PROPERTY(int foregroundOrientation READ getForegroundOrientation NOTIFY foregroundOrientationChanged);
    Q_PROPERTY(QDeclarativeListProperty<Desktop> runningApps READ runningApps NOTIFY runningAppsChanged)
    Q_PROPERTY(int runningAppsLimit READ runningAppsLimit WRITE setRunningAppsLimit);
    Q_PROPERTY(int preferredLandscapeOrientation READ preferredLandscapeOrientation);
    Q_PROPERTY(int preferredPortraitOrientation READ preferredPortraitOrientation);
    Q_PROPERTY(bool haveAppStore READ haveAppStore NOTIFY haveAppStoreChanged);
    Q_PROPERTY(int foregroundWindow READ foregroundWindow NOTIFY foregroundWindowChanged);
    Q_PROPERTY(bool screenOn READ screenOn NOTIFY screenOnChanged)
    Q_PROPERTY(QStringList applicationDirectories READ applicationDirectories NOTIFY applicationDirectoriesChanged);

public:
    explicit Application(int & argc, char ** argv);
    ~Application();

    int getOrientation() {
        return m_orientation;
    }

    // We have the multiple top level windows, each of which could
    // choose to lock or unlock the orientation, so the qApp based
    // locking will not work here.  Leaving the property in place
    // so the QML doesn't explode, but this will be property handled
    // in the switch to MeeGo.Components which has a different approach
    // to orientation locking.
    bool getOrientationLocked() {
        return orientationLocked;
    }
    void setOrientationLocked(bool locked) {
        orientationLocked = locked;
    }

    int getForegroundOrientation();

    QSettings *themeConfig;

    int runningAppsLimit() {
        return m_runningAppsLimit;
    }
    void setRunningAppsLimit(int limit);
    QDeclarativeListProperty<Desktop> runningApps();

    QStringList applicationDirectories() const {
        return m_applicationDirectories;
    }

    int preferredLandscapeOrientation() {
        return m_preferredLandscapeOrientation;
    }
    int preferredPortraitOrientation() {
        return m_preferredPortraitOrientation;
    }

    bool haveAppStore() {
        return m_haveAppStore;
    }

    int foregroundWindow()
    {
        return m_foregroundWindow;
    }

    bool screenOn() {
        return m_screenOn;
    }
    void setScreenOn(bool value);

public slots:
    void showTaskSwitcher();
    void showPanels();
    void showGrid();
    void showAppStore();
    void showAlarmDialog(int alarmId, QString title, QString message, bool snooze, QString soundUri);
    void showHardNotificationDialog(QString subject, QString body, QString remoteAction, uint notificationUserId, uint m_lastNotificationId, QString declineAction, QString imageURI);
    void goHome();
    void lock();
    void launchDesktopByName(QString name);
    void closeDesktopByName(QString name);
    void openStatusIndicatorMenu();
    void clearAllNotifications();
    void desktopLaunched(int pid);
    void activateScreenSaver();

    // MNotificationManager Interface
    uint addGroup(uint notificationUserId, const QString &eventType, const QString &summary, const QString &body, const QString &action, const QString &imageURI, const QString &declineAction, uint count, const QString &identifier);
    uint addGroup(uint notificationUserId, const QString &eventType, const QString &summary, const QString &body, const QString &action, const QString &imageURI, uint count, const QString &identifier);
    uint addGroup(uint notificationUserId, const QString &eventType);
    uint addNotification(uint notificationUserId, uint groupId, const QString &eventType, const QString &summary, const QString &body, const QString &action, const QString &imageURI, const QString &declineAction, uint count, const QString &identifier);
    uint addNotification(uint notificationUserId, uint groupId, const QString &eventType, const QString &summary, const QString &body, const QString &action, const QString &imageURI, uint count, const QString &identifier);
    uint addNotification(uint notificationUserId, uint groupId, const QString &eventType);
    QList < MNotificationGroup >  notificationGroupListWithIdentifiers(uint notificationUserId);
    QList < uint >  notificationIdList(uint notificationUserId);
    QList < MNotification >  notificationListWithIdentifiers(uint notificationUserId);
    uint notificationUserId();
    bool removeGroup(uint notificationUserId, uint groupId);
    bool removeNotification(uint notificationUserId, uint notificationId);
    bool updateGroup(uint notificationUserId, uint groupId, const QString &eventType, const QString &summary, const QString &body, const QString &action, const QString &imageURI, const QString &declineAction, uint count, const QString &identifier);
    bool updateGroup(uint notificationUserId, uint groupId, const QString &eventType, const QString &summary, const QString &body, const QString &action, const QString &imageURI, uint count, const QString &identifier);
    bool updateGroup(uint notificationUserId, uint groupId, const QString &eventType);
    bool updateNotification(uint notificationUserId, uint notificationId, const QString &eventType, const QString &summary, const QString &body, const QString &action, const QString &imageURI, const QString &declineAction, uint count, const QString &identifier);
    bool updateNotification(uint notificationUserId, uint notificationId, const QString &eventType, const QString &summary, const QString &body, const QString &action, const QString &imageURI, uint count, const QString &identifier);
    bool updateNotification(uint notificationUserId, uint notificationId, const QString &eventType);

    // Alarms
    void stopSnooze(int id);

signals:
    void orientationChanged();
    void runningAppsChanged();
    void haveAppStoreChanged();
    void foregroundWindowChanged();
    void foregroundOrientationChanged();
    void stopOrientationSensor();
    void startOrientationSensor();
    void alarm(int alarmId, QString title, QString message, bool snooze, QString soundUri);
    void hardNotification(QString subject, QString body, QString remoteAction, uint userId, uint notificationId, QString declineAction, QString imageURI);
    void screenOnChanged();
    void applicationDirectoriesChanged();

private slots:
    void cleanupPanels();
    void cleanupGrid();
    void cleanupTaskSwitcher();
    void cleanupLockscreen();
    void cleanupAlarmDialog();
    void cleanupHardNotificationDialog();
    void updateApps(const QList<WindowInfo> &windowList);
    void toggleSwitcher();
    void cleanupStatusIndicatorMenu();
    void screenSaverTimeoutChanged();
    void automaticBacklightControlChanged();
    void updateBacklight();
    void updateOrientation();
    void updateAmbientLight();
    void alarmHandler(const QDBusMessage &msg);
    void applicationDirectoriesUpdated();

protected:
    /*! \reimp
     * The _NET_CLIENT_LIST property of the root window is interesting and
     * causes an update to the window list.
     */
    virtual bool x11EventFilter(XEvent *event);
    //! \reimp_end

private:
    void updateWindowList();
    static QVector<Atom> getNetWmState(Display *display, Window window);
    void minimizeWindow(int windowId);
    void loadTranslators();
    void grabHomeKey(const char* key);
    void raiseWindow(int windowId);
    void setForegroundOrientationForWindow(uint wid);
    void updateScreenSaver(Window window);
    void setBacklight(int percentage);
    bool namesMatchFuzzy(const Desktop& d, const WindowInfo& w) const;

    int m_orientation;
    bool orientationLocked;
    bool useOpenGL;
    Dialog *taskSwitcher;
    Dialog *lockScreen;
    Dialog *gridScreen;
    Dialog *panelsScreen;
    Dialog *statusIndicatorMenu;
    Dialog *alarmDialog;
    Dialog *hardNotificationDialog;
    Atom windowTypeAtom;
    Atom windowTypeNormalAtom;
    Atom windowTypeDesktopAtom;
    Atom windowTypeNotificationAtom;
    Atom windowTypeDockAtom;
    Atom clientListAtom;
    Atom closeWindowAtom;
    Atom skipTaskbarAtom;
    Atom windowStateAtom;
    Atom activeWindowAtom;
    Atom foregroundOrientationAtom;
    Atom inhibitScreenSaverAtom;

    int  m_ss_event;
    int  m_ss_error;
    QList<Window> windowsBeingClosed;
    QList<Window> openWindows;

    QSet<KeyCode> homeKeys;
    KeyCode menu;
    KeyCode mediaPlayKey;
    KeyCode mediaPauseKey;
    KeyCode mediaStopKey;
    KeyCode mediaPreviousKey;
    KeyCode mediaNextKey;
    KeyCode volumeUpKey;
    KeyCode volumeDownKey;
    KeyCode volumeMuteKey;
    KeyCode powerKey;

    QTranslator qtTranslator;
    QTranslator commonTranslator;
    QTranslator homescreenTranslator;
    QTranslator panelsTranslator;
    QTranslator daemonTranslator;
    QTranslator mediaTranslator;

    int m_runningAppsLimit;
    QList<Desktop *> m_runningApps;
    QList<Desktop *> m_runningAppsOverflow;

    QTimer *m_homeLongPressTimer;

    bool m_homeActive;
    Time m_homePressTime;

    int m_preferredLandscapeOrientation;
    int m_preferredPortraitOrientation;

    bool m_showPanelsAsHome;
    bool m_haveAppStore;

    int m_foregroundWindow;
    int m_foregroundOrientation;

    QString m_appLauncherPath;
    QString m_lockscreenPath;

    NotificationDataStore *m_notificationDataStore;
    NotificationModel *m_notificationModel;
    uint m_lastNotificationId;

    MGConfItem *m_screenSaverTimeoutItem;
    int m_screenSaverTimeout;
    QList<Window> inhibitList;

    MGConfItem *m_applicationDirectoriesItem;
    QStringList m_applicationDirectories;

    QDBusInterface *m_player;

    MGConfItem *m_automaticBacklightItem;
    bool m_automaticBacklight;
    MGConfItem *m_manualBacklightItem;
    QList<DisplayInfo *> displayList;

    QAmbientLightSensor ambientLightSensor;
    QOrientationSensor orientationSensor;

    VolumeControl volumeControl;

    AlarmInterface *m_alarmService;

    bool m_screenOn;

    bool m_orientationSensorAvailable;
    bool m_ambientLightSensorAvailable;

    LockscreenAdaptor *m_lockScreenAdaptor;

    bool m_enableRenderingSwap;
};

#endif // APPLICATION_H
