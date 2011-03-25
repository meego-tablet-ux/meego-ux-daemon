#include "notificationdatastore.h"
#include <iostream>
#include <fstream>
using namespace std;

static NotificationDataStore *gNotificationInstance = NULL;

NotificationDataStore *NotificationDataStore::instance()
{
    if (gNotificationInstance)
        return gNotificationInstance;

    gNotificationInstance = new NotificationDataStore();
    return gNotificationInstance;
}

NotificationDataStore::NotificationDataStore(QObject *parent) :
        QObject(parent),
        m_nextUserId(0),
        m_nextNotifyId(0),
        m_nextGroupId(0)
{
}

uint NotificationDataStore::storeNotification(uint notificationUserId, uint groupId, const QString &eventType, const QString &summary, const QString &body, const QString &action, const QString &imageURI, uint count, const QString &identifier)
{

    m_notifications << new NotificationItem(notificationUserId,
                                            ++m_nextNotifyId,
                                            groupId,
                                            eventType,
                                            summary,
                                            body,
                                            action,
                                            imageURI,
                                            count,
                                            identifier);
    emit dataChanged();
    return m_nextNotifyId;
}

uint NotificationDataStore::storeNotification(uint notificationUserId, uint groupId, const QString &eventType)
{
    NotificationItem *item = new NotificationItem;
    item->setNotificationId(++m_nextNotifyId);
    item->setUserId(notificationUserId);
    item->setGroupId(groupId);
    item->setEventType(eventType);
    m_notifications << item;

    emit dataChanged();
    return m_nextNotifyId;
}

bool NotificationDataStore::deleteNotification(uint notificationUserId, uint notificationId)
{
    int i = 0;
    foreach (NotificationItem *item, m_notifications)
    {
        if (item->getUserId() == notificationUserId &&
            item->getNotificationId() == notificationId)
        {
            break;
        }
        i++;
    }

    if (i < m_notifications.length())
    {
        delete m_notifications.takeAt(i);
        emit dataChanged();
        return true;
    }

    return false;
}

void NotificationDataStore::triggerNotification(uint notificationUserId, uint notificationId)
{
    for (int i = 0; i < m_notifications.length(); i++)
    {
        NotificationItem *item = m_notifications.at(i);
        if (item->getUserId() == notificationUserId &&
            item->getNotificationId() == notificationId)
        {
            if (!item->getAction().isEmpty())
            {
                MRemoteAction action(item->getAction());
                action.trigger();
            }
            return;
        }
    }
}

bool NotificationDataStore::updateNotification(uint notificationUserId, uint notificationId, const QString &eventType, const QString &summary, const QString &body, const QString &action, const QString &imageURI, uint count, const QString &identifier)
{
    foreach (NotificationItem *item, m_notifications)
    {
        if (item->getUserId() == notificationUserId &&
            item->getNotificationId() == notificationId)
        {
            item->setEventType(eventType);
            item->setSummary(summary);
            item->setBody(body);
            item->setAction(action);
            item->setImageURI(imageURI);
            item->setCount(count);
            item->setIdentifier(identifier);
            emit dataChanged();
            return true;
        }
    }

    return false;
}

bool NotificationDataStore::updateNotification(uint notificationUserId, uint notificationId, const QString &eventType)
{
    foreach (NotificationItem *item, m_notifications)
    {
        if (item->getUserId() == notificationUserId &&
            item->getNotificationId() == notificationId)
        {
            item->setEventType(eventType);
            emit dataChanged();
            return true;
        }
    }

    return false;
}

QList<MNotification> NotificationDataStore::getNotificationListWithId(uint notificationUserId)
{
    QList<MNotification> result;
    foreach (NotificationItem *item, m_notifications)
    {
        if (item->getUserId() == notificationUserId)
        {
            MNotification n(item->getEventType(), item->getSummary(), item->getBody());
            n.setAction(MRemoteAction(item->getAction()));
            n.setCount(item->getCount());
            n.setIdentifier(item->getIdentifier());
            n.setImage(item->getIdentifier());
            result << n;
        }
    }

    return result;
}


uint NotificationDataStore::storeGroup(uint notificationUserId, const QString &eventType, const QString &summary, const QString &body, const QString &action, const QString &imageURI, uint count, const QString &identifier)
{

    m_groups << new NotificationItem(notificationUserId,
                                     ++m_nextGroupId,
                                     0,
                                     eventType,
                                     summary,
                                     body,
                                     action,
                                     imageURI,
                                     count,
                                     identifier);
    emit dataChanged();
    return m_nextGroupId;
}

uint NotificationDataStore::storeGroup(uint notificationUserId, const QString &eventType)
{
    NotificationItem *item = new NotificationItem;
    item->setUserId(notificationUserId);
    item->setEventType(eventType);
    m_groups << item;

    emit dataChanged();
    return m_nextGroupId;
}

bool NotificationDataStore::updateGroup(uint notificationUserId, uint groupId, const QString &eventType, const QString &summary, const QString &body, const QString &action, const QString &imageURI, uint count, const QString &identifier)
{
    foreach (NotificationItem *item, m_groups)
    {
        if (item->getUserId() == notificationUserId &&
            item->getGroupId() == groupId)
        {
            item->setEventType(eventType);
            item->setSummary(summary);
            item->setBody(body);
            item->setAction(action);
            item->setImageURI(imageURI);
            item->setCount(count);
            item->setIdentifier(identifier);
            emit dataChanged();
            return true;
        }
    }

    return false;
}

bool NotificationDataStore::updateGroup(uint notificationUserId, uint groupId, const QString &eventType)
{
    foreach (NotificationItem *item, m_groups)
    {
        if (item->getUserId() == notificationUserId &&
            item->getGroupId() == groupId)
        {
            item->setEventType(eventType);
            emit dataChanged();
            return true;
        }
    }

    return false;
}

bool NotificationDataStore::deleteGroup(uint notificationUserId, uint groupId)
{
    int i = 0;
    foreach (NotificationItem *item, m_groups)
    {
        if (item->getUserId() == notificationUserId &&
            item->getGroupId() == groupId)
        {
            break;
        }
        i++;
    }

    if (i < m_groups.length())
    {
        NotificationItem *item = m_groups.takeAt(i);

        // Find any notifications that use this group and move them to the
        // empty group (i.e. group id 0)
        foreach (NotificationItem *i, m_notifications)
        {
            if (i->getGroupId() == groupId)
                i->setGroupId(0);
        }

        delete item;
        emit dataChanged();
        return true;
    }

    return false;
}

QList < uint >  NotificationDataStore::getNotificationIdList(uint notificationUserId)
{
    QList<uint> result;

    foreach (NotificationItem *item, m_notifications)
    {
        if (item->getUserId() == notificationUserId)
            result << item->getNotificationId();
    }

    return result;
}

QList < MNotificationGroup >  NotificationDataStore::getNotificationGroupListWithId(uint notificationUserId)
{
    QList<MNotificationGroup> result;
    foreach (NotificationItem *item, m_groups)
    {
        if (item->getUserId() == notificationUserId)
        {
            MNotificationGroup g(item->getEventType(), item->getSummary(), item->getBody());
            g.setAction(MRemoteAction(item->getAction()));
            g.setCount(item->getCount());
            g.setIdentifier(item->getIdentifier());
            g.setImage(item->getIdentifier());
            result << g;
        }
    }

    return result;

}

uint NotificationDataStore::getNotificationUserId()
{
    return ++m_nextUserId;
}
