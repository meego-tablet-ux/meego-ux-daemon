/*
 * Copyright 2011 Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include <mremoteaction.h>
#include <QtDeclarative/qdeclarative.h>
#include "notificationmodel.h"
#include "notificationdatastore.h"
#include "notificationitem.h"

NotificationModel::NotificationModel(QObject *parent) :
    QAbstractListModel(parent),
    m_filtersItem(NULL),
    m_data(NotificationDataStore::instance())
{
    QHash<int, QByteArray> roles;
    roles.insert(NotificationItem::UserId, "userId");
    roles.insert(NotificationItem::NotificationId, "notificationId");
    roles.insert(NotificationItem::GroupId, "groupId");
    roles.insert(NotificationItem::EventType, "eventType");
    roles.insert(NotificationItem::Summary, "summary");
    roles.insert(NotificationItem::Body, "body");
    roles.insert(NotificationItem::Action, "action");
    roles.insert(NotificationItem::ImageURI, "imageURI");
    roles.insert(NotificationItem::DeclineAction, "declineAction");
    roles.insert(NotificationItem::Identifier, "identifier");
    roles.insert(NotificationItem::Count, "count");
    roles.insert(NotificationItem::Timestamp, "time");
    setRoleNames(roles);

    for (int i=0; i < m_data->notificationCount(); i++)
    {
        displayData << m_data->notificationAt(i);
    }

    connect(m_data, SIGNAL(dataChanged()), this, SLOT(onDataStoreUpdated()));
}

int NotificationModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return displayData.size();
}

QVariant NotificationModel::data(const QModelIndex &index, int role) const
{
    if(!displayData.isEmpty())
    {
        if (!index.isValid() || index.row() >= displayData.size())
            return QVariant();
        if (role == NotificationItem::UserId)
            return displayData[index.row()]->getUserId();
        if (role == NotificationItem::NotificationId)
            return displayData[index.row()]->getNotificationId();
        if (role == NotificationItem::GroupId)
            return displayData[index.row()]->getGroupId();
        if (role == NotificationItem::EventType)
            return displayData[index.row()]->getEventType();
        if (role == NotificationItem::Summary)
            return displayData[index.row()]->getSummary();
        if (role == NotificationItem::Body)
            return displayData[index.row()]->getBody();
        if (role == NotificationItem::Action)
            return displayData[index.row()]->getAction();
        if (role == NotificationItem::DeclineAction)
            return displayData[index.row()]->getDeclineAction();
        if (role == NotificationItem::ImageURI)
            return displayData[index.row()]->getImageURI();
        if (role == NotificationItem::Identifier)
            return displayData[index.row()]->getIdentifier();
        if (role == NotificationItem::Count)
            return displayData[index.row()]->getCount();
        if (role == NotificationItem::Timestamp)
            return displayData[index.row()]->getTimestamp();
    }

    return QVariant();
}

void NotificationModel::onDataStoreUpdated()
{
    refreshViewableList();
    emit modelReset();
}

int NotificationModel::getCount()
{
   return displayData.size();
}

void NotificationModel::trigger(uint userId, uint notificationId)
{
    m_data->triggerNotification(userId, notificationId);
    m_data->deleteNotification(userId, notificationId);
}

void NotificationModel::triggerDeclineAction(uint userId, uint notificationId)
{
    m_data->triggerDeclineNotification(userId, notificationId);
    m_data->deleteNotification(userId, notificationId);
}

void NotificationModel::deleteNotification(uint userId, uint notificationId)
{
    m_data->deleteNotification(userId, notificationId);
}

void NotificationModel::refreshViewableList()
{
    QList<NotificationItem *> filteredList;
    QList<NotificationItem *> mergeList;
    QList<QString> mergeKeys;
    QMultiMap<QString, NotificationItem *> itemsToMerge;
    NotificationItem *currentNotificationItem;


    for (int k=0; k < m_data->notificationCount(); k++)
    {
        currentNotificationItem = m_data->notificationAt(k);

        if (m_listFilters.isEmpty() || m_listFilters.contains(currentNotificationItem->getEventType()))
        {
            if (m_notificationMergeNumbers.contains(currentNotificationItem->getEventType()))
            {
                if (m_notificationMergeNumbers[currentNotificationItem->getEventType()].roleToMerge == "summary")
                    itemsToMerge.insert(currentNotificationItem->getEventType() + currentNotificationItem->getSummary(), currentNotificationItem);

                else if (m_notificationMergeNumbers[currentNotificationItem->getEventType()].roleToMerge == "body")
                    itemsToMerge.insert(currentNotificationItem->getEventType() + currentNotificationItem->getBody(), currentNotificationItem);

                else
                    itemsToMerge.insert(currentNotificationItem->getEventType(),currentNotificationItem);
            }

            else if (m_notificationMergeNumbers.contains("all"))
            {
                itemsToMerge.insert(currentNotificationItem->getEventType(), currentNotificationItem);
            }

            else
            {
                filteredList << currentNotificationItem;
            }
        }
    }

    if (!itemsToMerge.empty())
    {
        mergeKeys = itemsToMerge.uniqueKeys();

        for (int j = 0; j < mergeKeys.size(); j++)
        {
            filteredList << mergeListItems(itemsToMerge.values(mergeKeys[j]));
        }
    }

    if(!displayData.isEmpty())
    {
        beginRemoveRows(QModelIndex(), 0, displayData.count()-1);
        displayData.clear();
        endRemoveRows();
    }

    if(!filteredList.isEmpty())
    {
        beginInsertRows(QModelIndex(), 0, filteredList.count()-1);
        displayData = filteredList;
        endInsertRows();
    }
}

void NotificationModel::addFilter(QString filter)
{
    if (!m_listFilters.contains(filter))
    {
        m_listFilters.append(filter);
    }
}

void NotificationModel::removeFilter(QString filter)
{
    m_listFilters.removeAll(filter);
}

void NotificationModel::clearFilters()
{
    m_listFilters.clear();
    refreshViewableList();
    emit modelReset();  
}

void NotificationModel::applyFilters()
{
    m_listFilters.clear();
    if (m_filtersItem->value() != QVariant::Invalid)
    {
        QStringList filterList = m_filtersItem->value().toStringList();

        foreach (QString filter, filterList)
        {
            addFilter(filter);
        }
    }
    refreshViewableList();
    emit modelReset();
}

void NotificationModel::setFilterKey(const QString key)
{
    if (m_filtersItem)
    {
        delete m_filtersItem;
    }
    m_filtersItem = new MGConfItem(key, this);
    connect(m_filtersItem, SIGNAL(valueChanged()), this, SLOT(applyFilters()));
    applyFilters();
}

void NotificationModel::setNotificationMergeRule(int maxNumber, QString eventType, QString mergeRole)
{
    m_notificationMergeNumbers[eventType].numToMergeAt = maxNumber;
    m_notificationMergeNumbers[eventType].roleToMerge = mergeRole;

}

QList<NotificationItem *> NotificationModel::mergeListItems(QList<NotificationItem *> listToMerge)
{
    QList<NotificationItem *> tmpList;

    if(m_notificationMergeNumbers.contains(listToMerge[0]->getEventType())) 
    {
	if (listToMerge.size() > m_notificationMergeNumbers[listToMerge[0]->getEventType()].numToMergeAt)
	{
            listToMerge[0]->setCount(listToMerge.size());
            tmpList << listToMerge[0];
	}
	else
	{
            tmpList<< listToMerge;
	}        
    }
    else if (listToMerge.size() > m_notificationMergeNumbers["all"].numToMergeAt)
    {
        listToMerge[0]->setCount(listToMerge.size());
        tmpList << listToMerge[0];
    }

    else
    {
        tmpList << listToMerge;
    }
    return tmpList;
}

QML_DECLARE_TYPE(NotificationModel);
