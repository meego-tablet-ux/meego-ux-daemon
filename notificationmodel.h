/*
 * Copyright 2011 Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef NOTIFICATIONMODEL_H
#define NOTIFICATIONMODEL_H

#include <QObject>
#include <QAbstractListModel>
#include <MGConfItem>
#include "notificationitem.h"

#define MAXCUSTOMNOTIFICATIONS 4

struct MergeArgs{
    int numToMergeAt;
    QString roleToMerge;
};

class NotificationDataStore;

class NotificationModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ getCount NOTIFY modelReset);
    Q_PROPERTY(QString filterKey READ filterKey WRITE setFilterKey);

public:
    explicit NotificationModel(QObject *parent = 0);

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
    virtual QVariant data(const QModelIndex &index, int role) const;

    int getCount();

    QString filterKey() const {
        return m_filterKey;
    }
    void setFilterKey(const QString key);
    void setNotificationMergeRule(int maxNumber, QString eventType="all", QString mergeRole="all");

signals:
    void modelReset();

public slots:
    void trigger(uint userId, uint notificationId);
    void triggerDeclineAction(uint userId, uint notificationId);
    void refreshViewableList();
    void addFilter(QString filter);
    void removeFilter(QString filter);
    void clearFilters();
    void applyFilters();
    void deleteNotification(uint userId, uint notificationId);

private slots:
    void onDataStoreUpdated();

private:
    QString m_filterKey;
    MGConfItem *m_filtersItem;
    QList<NotificationItem *> displayData;
    QList<QString> m_listFilters;
    NotificationDataStore *m_data;
    QMap<QString, MergeArgs> m_notificationMergeNumbers;

    QList<NotificationItem *> mergeListItems(QList<NotificationItem *> listToMerge);
};

#endif // NOTIFICATIONMODEL_H
