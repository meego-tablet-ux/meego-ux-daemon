#ifndef NOTIFICATIONDATASTORE_H
#define NOTIFICATIONDATASTORE_H

#include <MNotification>
#include <mnotificationgroup.h>
#include <QList>
#include <QObject>
#include <QHash>
#include <QMultiHash>
#include <QMutableHashIterator>
#include <QDebug>

#include "notificationitem.h"

class NotificationDataStore : public QObject
{
    Q_OBJECT
public:
    NotificationDataStore(QObject *parent = NULL);
    uint storeNotification(uint notificationUserId, uint groupId, const QString &eventType, const QString &summary, const QString &body, const QString &action, const QString &imageURI, uint count, const QString &identifier);
    uint storeNotification(uint notificationUserId, uint groupId, const QString &eventType);
    bool deleteNotification(uint notificationUserId, uint notificationId);
    void triggerNotification(uint notificationUserId, uint notificationId);
    bool updateNotification(uint notificationUserId, uint notificationId, const QString &eventType, const QString &summary, const QString &body, const QString &action, const QString &imageURI, uint count, const QString &identifier);
    bool updateNotification(uint notificationUserId, uint notificationId, const QString &eventType);
    QList<MNotification> getNotificationListWithId(uint notificationUserId);

    uint storeGroup(uint notificationUserId, const QString &eventType, const QString &summary, const QString &body, const QString &action, const QString &imageURI, uint count, const QString &identifier);
    uint storeGroup(uint notificationUserId, const QString &eventType);
    bool updateGroup(uint notificationUserId, uint groupId, const QString &eventType, const QString &summary, const QString &body, const QString &action, const QString &imageURI, uint count, const QString &identifier);
    bool updateGroup(uint notificationUserId, uint groupId, const QString &eventType);
    bool deleteGroup(uint notificationUserId, uint groupId);
    QList < uint >  getNotificationIdList(uint notificationUserId);
    QList < MNotificationGroup >  getNotificationGroupListWithId(uint notificationUserId);
    uint getNotificationUserId();

    void clearAllNotifications() {
        while (m_notifications.length() > 0)
            delete m_notifications.takeLast();
        emit dataChanged();
    }

    int notificationCount() {
        return m_notifications.length();
    }

    NotificationItem *notificationAt(int index) {
        if (index < m_notifications.length())
            return m_notifications[index];
        return NULL;
    }

    QList <NotificationItem *> notificationsWithEventType(QString eType)
    {
        QList<NotificationItem *> result;
        for (int i = 0; i < m_notifications.size(); i++)
        {
            if (m_notifications[i]->getEventType() == eType)
	    {		
                result << m_notifications[i];
	     }
        }

        return result;
    }

    static NotificationDataStore *instance();

signals:
    void dataChanged();

private:
    QList<NotificationItem *> m_notifications;
    QList<NotificationItem *> m_groups;

    uint m_nextUserId;
    uint m_nextNotifyId;
    uint m_nextGroupId;
};

#endif // NOTIFICATIONDATASTORE_H
