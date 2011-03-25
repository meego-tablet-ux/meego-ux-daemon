/*
 * This file was generated by qdbusxml2cpp version 0.7
 * Command line was: qdbusxml2cpp -c NotificationManagerAdaptor interfaces/notificationmanager.xml -a notificationsmanageradaptor
 *
 * qdbusxml2cpp is Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 *
 * This is an auto-generated file.
 * This file may have been hand-edited. Look for HAND-EDIT comments
 * before re-generating it.
 */

#ifndef NOTIFICATIONSMANAGERADAPTOR_H_1298582340
#define NOTIFICATIONSMANAGERADAPTOR_H_1298582340

#include <QObject>
#include <QDBusAbstractAdaptor>
#include <MNotification>
#include <mnotificationgroup.h>

class QByteArray;
template<class T> class QList;
template<class Key, class Value> class QMap;
class QString;
class QStringList;
class QVariant;

/*
 * Adaptor class for interface com.meego.core.MNotificationManager
 */
class NotificationManagerAdaptor: public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.meego.core.MNotificationManager")
    Q_CLASSINFO("D-Bus Introspection", ""
"  <interface name=\"com.meego.core.MNotificationManager\">\n"
"    <method name=\"notificationUserId\">\n"
"      <arg direction=\"out\" type=\"u\" name=\"notificationUserId\"/>\n"
"    </method>\n"
"    <method name=\"addGroup\">\n"
"      <arg direction=\"in\" type=\"u\" name=\"notificationUserId\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"eventType\"/>\n"
"      <arg direction=\"out\" type=\"u\" name=\"groupId\"/>\n"
"    </method>\n"
"    <method name=\"addGroup\">\n"
"      <arg direction=\"in\" type=\"u\" name=\"notificationUserId\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"eventType\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"summary\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"body\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"action\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"imageURI\"/>\n"
"      <arg direction=\"in\" type=\"u\" name=\"count\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"identifier\"/>\n"
"      <arg direction=\"out\" type=\"u\" name=\"groupId\"/>\n"
"    </method>\n"
"    <method name=\"updateGroup\">\n"
"      <arg direction=\"in\" type=\"u\" name=\"notificationUserId\"/>\n"
"      <arg direction=\"in\" type=\"u\" name=\"groupId\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"eventType\"/>\n"
"      <arg direction=\"out\" type=\"b\" name=\"result\"/>\n"
"    </method>\n"
"    <method name=\"updateGroup\">\n"
"      <arg direction=\"in\" type=\"u\" name=\"notificationUserId\"/>\n"
"      <arg direction=\"in\" type=\"u\" name=\"groupId\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"eventType\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"summary\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"body\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"action\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"imageURI\"/>\n"
"      <arg direction=\"in\" type=\"u\" name=\"count\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"identifier\"/>\n"
"      <arg direction=\"out\" type=\"b\" name=\"result\"/>\n"
"    </method>\n"
"    <method name=\"removeGroup\">\n"
"      <arg direction=\"in\" type=\"u\" name=\"notificationUserId\"/>\n"
"      <arg direction=\"in\" type=\"u\" name=\"groupId\"/>\n"
"      <arg direction=\"out\" type=\"b\" name=\"result\"/>\n"
"    </method>\n"
"    <method name=\"addNotification\">\n"
"      <arg direction=\"in\" type=\"u\" name=\"notificationUserId\"/>\n"
"      <arg direction=\"in\" type=\"u\" name=\"groupId\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"eventType\"/>\n"
"      <arg direction=\"out\" type=\"u\" name=\"notificationId\"/>\n"
"    </method>\n"
"    <method name=\"addNotification\">\n"
"      <arg direction=\"in\" type=\"u\" name=\"notificationUserId\"/>\n"
"      <arg direction=\"in\" type=\"u\" name=\"groupId\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"eventType\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"summary\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"body\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"action\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"imageURI\"/>\n"
"      <arg direction=\"in\" type=\"u\" name=\"count\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"identifier\"/>\n"
"      <arg direction=\"out\" type=\"u\" name=\"notificationId\"/>\n"
"    </method>\n"
"    <method name=\"updateNotification\">\n"
"      <arg direction=\"in\" type=\"u\" name=\"notificationUserId\"/>\n"
"      <arg direction=\"in\" type=\"u\" name=\"notificationId\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"eventType\"/>\n"
"      <arg direction=\"out\" type=\"b\" name=\"result\"/>\n"
"    </method>\n"
"    <method name=\"updateNotification\">\n"
"      <arg direction=\"in\" type=\"u\" name=\"notificationUserId\"/>\n"
"      <arg direction=\"in\" type=\"u\" name=\"notificationId\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"eventType\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"summary\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"body\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"action\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"imageURI\"/>\n"
"      <arg direction=\"in\" type=\"u\" name=\"count\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"identifier\"/>\n"
"      <arg direction=\"out\" type=\"b\" name=\"result\"/>\n"
"    </method>\n"
"    <method name=\"removeNotification\">\n"
"      <arg direction=\"in\" type=\"u\" name=\"notificationUserId\"/>\n"
"      <arg direction=\"in\" type=\"u\" name=\"notificationId\"/>\n"
"      <arg direction=\"out\" type=\"b\" name=\"result\"/>\n"
"    </method>\n"
"    <method name=\"notificationIdList\">\n"
"      <arg direction=\"in\" type=\"u\" name=\"notificationUserId\"/>\n"
"      <arg direction=\"out\" type=\"au\" name=\"result\"/>\n"
"      <annotation value=\"QList &lt; uint > \" name=\"com.trolltech.QtDBus.QtTypeName.Out0\"/>\n"
"    </method>\n"
"    <method name=\"notificationListWithIdentifiers\">\n"
"      <arg direction=\"in\" type=\"u\" name=\"notificationUserId\"/>\n"
"      <arg direction=\"out\" type=\"a(uusssssus)\" name=\"result\"/>\n"
"      <annotation value=\"QList &lt; MNotification > \" name=\"com.trolltech.QtDBus.QtTypeName.Out0\"/>\n"
"    </method>\n"
"    <method name=\"notificationGroupListWithIdentifiers\">\n"
"      <arg direction=\"in\" type=\"u\" name=\"notificationUserId\"/>\n"
"      <arg direction=\"out\" type=\"a(usssssus)\" name=\"result\"/>\n"
"      <annotation value=\"QList &lt; MNotificationGroup > \" name=\"com.trolltech.QtDBus.QtTypeName.Out0\"/>\n"
"    </method>\n"
"  </interface>\n"
        "")
public:
    NotificationManagerAdaptor(QObject *parent);
    virtual ~NotificationManagerAdaptor();

public: // PROPERTIES
public Q_SLOTS: // METHODS
    uint addGroup(uint notificationUserId, const QString &eventType, const QString &summary, const QString &body, const QString &action, const QString &imageURI, uint count, const QString &identifier);
    uint addGroup(uint notificationUserId, const QString &eventType);
    uint addNotification(uint notificationUserId, uint groupId, const QString &eventType, const QString &summary, const QString &body, const QString &action, const QString &imageURI, uint count, const QString &identifier);
    uint addNotification(uint notificationUserId, uint groupId, const QString &eventType);
    QList < MNotificationGroup >  notificationGroupListWithIdentifiers(uint notificationUserId);
    QList < uint >  notificationIdList(uint notificationUserId);
    QList < MNotification >  notificationListWithIdentifiers(uint notificationUserId);
    uint notificationUserId();
    bool removeGroup(uint notificationUserId, uint groupId);
    bool removeNotification(uint notificationUserId, uint notificationId);
    bool updateGroup(uint notificationUserId, uint groupId, const QString &eventType, const QString &summary, const QString &body, const QString &action, const QString &imageURI, uint count, const QString &identifier);
    bool updateGroup(uint notificationUserId, uint groupId, const QString &eventType);
    bool updateNotification(uint notificationUserId, uint notificationId, const QString &eventType, const QString &summary, const QString &body, const QString &action, const QString &imageURI, uint count, const QString &identifier);
    bool updateNotification(uint notificationUserId, uint notificationId, const QString &eventType);
Q_SIGNALS: // SIGNALS
};

#endif
